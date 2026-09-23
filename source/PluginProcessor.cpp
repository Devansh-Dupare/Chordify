#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

PluginProcessor::~PluginProcessor()
{
}

PluginProcessor::ParameterValues::ParameterValues (juce::AudioProcessorValueTreeState& tree)
    : chordSource (*tree.getRawParameterValue (params::id::chordSource)),
      root (*tree.getRawParameterValue (params::id::root)),
      chordType (*tree.getRawParameterValue (params::id::chordType)),
      harmonics (*tree.getRawParameterValue (params::id::harmonics)),
      detune (*tree.getRawParameterValue (params::id::detune)),
      spread (*tree.getRawParameterValue (params::id::spread)),
      glide (*tree.getRawParameterValue (params::id::glide)),
      decay (*tree.getRawParameterValue (params::id::decay)),
      brightness (*tree.getRawParameterValue (params::id::brightness)),
      timbre (*tree.getRawParameterValue (params::id::timbre)),
      mix (*tree.getRawParameterValue (params::id::mix))
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    // Resonators keep ringing for their decay time after the input stops
    return parameterValues.decay.load();
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    // Steinberg's VST3 validator fails plugins whose single default program has no name
    return "Default";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (auto& word : heldNotes)
        word.store (0, std::memory_order_relaxed);

    resonatorBank.prepare (sampleRate);
    setLatencySamples (engine->getLatencySamples());

    midiVoices.reset();
    internalVoices = {};
    lastPartialInputs.reset();

    // Hosts may send larger blocks than announced; renderSegment splits those into pieces this size
    const auto blockSize = std::max (samplesPerBlock, 32);
    monoInput.setSize (1, blockSize);
    wetOutput.setSize (2, blockSize);

    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (parameterValues.mix.load() / 100.0f);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data (they may contain garbage)
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateHeldNotes (midiMessages);

    // Render up to each MIDI event, then apply it, so chord changes are sample accurate
    const auto numSamples = buffer.getNumSamples();
    int position = 0;
    for (const auto metadata : midiMessages)
    {
        const auto eventPosition = std::clamp (metadata.samplePosition, position, numSamples);
        renderSegment (buffer, position, eventPosition - position);
        handleMidiEvent (metadata.getMessage());
        position = eventPosition;
    }
    renderSegment (buffer, position, numSamples - position);
}

void PluginProcessor::handleMidiEvent (const juce::MidiMessage& message)
{
    // Tracked in both chord source modes, so switching to MIDI picks up notes already held
    if (message.isNoteOn())
        midiVoices.noteOn (message.getNoteNumber());
    else if (message.isNoteOff())
        midiVoices.noteOff (message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        midiVoices.allNotesOff();
    else if (message.isPitchWheel())
        midiVoices.setPitchBend ((float) (message.getPitchWheelValue() - 8192) / 8192.0f);
}

void PluginProcessor::updateEngine()
{
    const auto& p = parameterValues;

    PartialInputs inputs;
    if ((params::ChordSource) juce::roundToInt (p.chordSource.load()) == params::ChordSource::midi)
    {
        inputs.voices = midiVoices.getVoices();
    }
    else
    {
        internalVoices = chordify::internalChord (juce::roundToInt (p.root.load()), juce::roundToInt (p.chordType.load()), internalVoices);
        inputs.voices = internalVoices;
    }

    inputs.settings.numHarmonics = juce::roundToInt (p.harmonics.load());
    inputs.settings.brightness = p.brightness.load() / 100.0f;
    inputs.settings.oddOnly = juce::roundToInt (p.timbre.load()) == 1;
    inputs.settings.detuneCents = p.detune.load();
    inputs.settings.spread = p.spread.load() / 100.0f;
    inputs.settings.sampleRate = currentSampleRate;

    if (inputs != lastPartialInputs)
    {
        engine->setPartials (chordify::computePartials (inputs.voices, inputs.settings));
        lastPartialInputs = inputs;
    }

    engine->setDecay (p.decay.load());
    engine->setGlide (p.glide.load() / 1000.0f);
    mixSmoothed.setTargetValue (p.mix.load() / 100.0f);
}

void PluginProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    if (numSamples <= 0)
        return;

    updateEngine();

    const auto numInputs = std::min (getTotalNumInputChannels(), buffer.getNumChannels());
    const auto numOutputs = std::min (getTotalNumOutputChannels(), buffer.getNumChannels());
    auto* mono = monoInput.getWritePointer (0);
    auto* wetLeft = wetOutput.getWritePointer (0);
    auto* wetRight = wetOutput.getWritePointer (1);

    for (int offset = 0; offset < numSamples;)
    {
        const auto n = std::min (numSamples - offset, monoInput.getNumSamples());
        const auto first = start + offset;

        // The resonators are excited by the mono sum of the input
        monoInput.clear (0, 0, n);
        for (int ch = 0; ch < numInputs; ++ch)
            juce::FloatVectorOperations::addWithMultiply (mono, buffer.getReadPointer (ch, first), 1.0f / (float) numInputs, n);

        engine->process (mono, wetLeft, wetRight, n);

        if (numOutputs == 1)
            juce::FloatVectorOperations::add (wetLeft, wetRight, n); // fold the stereo wet signal to mono

        for (int i = 0; i < n; ++i)
        {
            const auto wetAmount = mixSmoothed.getNextValue();
            for (int ch = 0; ch < numOutputs; ++ch)
            {
                auto* out = buffer.getWritePointer (ch, first + i);
                const auto wet = ch == 1 ? wetRight[i] : (numOutputs == 1 ? 0.5f * wetLeft[i] : wetLeft[i]);
                *out = *out * (1.0f - wetAmount) + wet * wetAmount;
            }
        }

        offset += n;
    }
}

void PluginProcessor::updateHeldNotes (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto note = message.getNoteNumber();
        const auto bit = std::uint64_t { 1 } << (note % 64);

        if (message.isNoteOn())
            heldNotes[(size_t) note / 64].fetch_or (bit, std::memory_order_relaxed);
        else if (message.isNoteOff())
            heldNotes[(size_t) note / 64].fetch_and (~bit, std::memory_order_relaxed);
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            for (auto& word : heldNotes)
                word.store (0, std::memory_order_relaxed);
    }
}

std::bitset<128> PluginProcessor::getHeldNotes() const
{
    std::bitset<128> notes;
    for (size_t word = 0; word < heldNotes.size(); ++word)
    {
        const auto bits = heldNotes[word].load (std::memory_order_relaxed);
        for (size_t i = 0; i < 64; ++i)
            notes[word * 64 + i] = ((bits >> i) & 1) != 0;
    }
    return notes;
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

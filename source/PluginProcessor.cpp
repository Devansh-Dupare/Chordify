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
    : chordSlot (*tree.getRawParameterValue (params::id::chordSlot)),
      harmonics (*tree.getRawParameterValue (params::id::harmonics)),
      detune (*tree.getRawParameterValue (params::id::detune)),
      spread (*tree.getRawParameterValue (params::id::spread)),
      glide (*tree.getRawParameterValue (params::id::glide)),
      decay (*tree.getRawParameterValue (params::id::decay)),
      excite (*tree.getRawParameterValue (params::id::excite)),
      brightness (*tree.getRawParameterValue (params::id::brightness)),
      timbre (*tree.getRawParameterValue (params::id::timbre)),
      mix (*tree.getRawParameterValue (params::id::mix)),
      inputHpf (*tree.getRawParameterValue (params::id::inputHpf)),
      tone (*tree.getRawParameterValue (params::id::tone)),
      output (*tree.getRawParameterValue (params::id::output))
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
    return (int) params::factoryPresets().size();
}

int PluginProcessor::getCurrentProgram()
{
    return currentProgram;
}

void PluginProcessor::setCurrentProgram (int index)
{
    const auto& presets = params::factoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    currentProgram = index;
    params::applyPreset (parameters, presets[(size_t) index]);
    parameters.state.setProperty (programProperty, index, nullptr);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    const auto& presets = params::factoryPresets();
    return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String();
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

namespace
{
    // The stored dB value picks up float error from the 0..1 round trip (0 dB reads as 7e-7 dB),
    // so round to 0.01 dB: 0 dB is then exactly unity gain and Mix 0% stays bit-exact
    float outputGainFromDb (float db)
    {
        return juce::Decibels::decibelsToGain (std::round (db * 100.0f) / 100.0f);
    }
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    resonatorBank.prepare (sampleRate);
    exciter.setAmount (parameterValues.excite.load() / 100.0f);
    exciter.prepare (sampleRate);
    inputHighPass.setCutoff (parameterValues.inputHpf.load());
    inputHighPass.prepare (sampleRate);
    tiltEq.setTone (parameterValues.tone.load() / 100.0f);
    tiltEq.prepare (sampleRate);
    setLatencySamples (resonatorBank.getLatencySamples());

    chordVoices = {};
    lastPartialInputs.reset();

    // Hosts may send larger blocks than announced; processBlock splits those into pieces this size
    const auto blockSize = std::max (samplesPerBlock, 32);
    monoInput.setSize (1, blockSize);
    wetOutput.setSize (2, blockSize);

    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (parameterValues.mix.load() / 100.0f);
    outputGainSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (outputGainFromDb (parameterValues.output.load()));
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
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data (they may contain garbage)
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateEngine();

    const auto numSamples = buffer.getNumSamples();
    const auto numInputs = std::min (totalNumInputChannels, buffer.getNumChannels());
    const auto numOutputs = std::min (totalNumOutputChannels, buffer.getNumChannels());
    auto* mono = monoInput.getWritePointer (0);
    auto* wetLeft = wetOutput.getWritePointer (0);
    auto* wetRight = wetOutput.getWritePointer (1);

    for (int first = 0; first < numSamples;)
    {
        const auto n = std::min (numSamples - first, monoInput.getNumSamples());

        // The resonators are excited by the mono sum of the input
        monoInput.clear (0, 0, n);
        for (int ch = 0; ch < numInputs; ++ch)
            juce::FloatVectorOperations::addWithMultiply (mono, buffer.getReadPointer (ch, first), 1.0f / (float) numInputs, n);

        inputHighPass.process (mono, n);
        exciter.process (mono, mono, n);

        resonatorBank.process (mono, wetLeft, wetRight, n);
        tiltEq.process (wetLeft, wetRight, n);

        if (numOutputs == 1)
            juce::FloatVectorOperations::add (wetLeft, wetRight, n); // fold the stereo wet signal to mono

        std::array<float, 2> peaks {};
        for (int i = 0; i < n; ++i)
        {
            const auto wetAmount = mixSmoothed.getNextValue();
            const auto gain = outputGainSmoothed.getNextValue();
            for (int ch = 0; ch < numOutputs; ++ch)
            {
                auto* out = buffer.getWritePointer (ch, first + i);
                const auto wet = ch == 1 ? wetRight[i] : (numOutputs == 1 ? 0.5f * wetLeft[i] : wetLeft[i]);
                *out = (*out * (1.0f - wetAmount) + wet * wetAmount) * gain;
                peaks[(size_t) std::min (ch, 1)] = std::max (peaks[(size_t) std::min (ch, 1)], std::abs (*out));
            }
        }

        if (numOutputs == 1)
            peaks[1] = peaks[0];
        for (size_t ch = 0; ch < 2; ++ch)
            if (peaks[ch] > outputPeaks[ch].load (std::memory_order_relaxed))
                outputPeaks[ch].store (peaks[ch], std::memory_order_relaxed);

        first += n;
    }
}

void PluginProcessor::updateEngine()
{
    const auto& p = parameterValues;

    // The chord: the piano selection or a saved slot, as Chord Slot picks
    const auto notes = chordSlots.notesFor (juce::roundToInt (p.chordSlot.load()));
    playingChord.store (ChordSlots::pack (notes), std::memory_order_relaxed);
    chordVoices = chordify::chordFromNotes (notes, chordVoices);

    PartialInputs inputs;
    inputs.voices = chordVoices;
    inputs.settings.numHarmonics = juce::roundToInt (p.harmonics.load());
    inputs.settings.brightness = p.brightness.load() / 100.0f;
    inputs.settings.oddOnly = juce::roundToInt (p.timbre.load()) == 1;
    inputs.settings.detuneCents = p.detune.load();
    inputs.settings.spread = p.spread.load() / 100.0f;
    inputs.settings.sampleRate = currentSampleRate;

    if (inputs != lastPartialInputs)
    {
        resonatorBank.setPartials (chordify::computePartials (inputs.voices, inputs.settings));
        lastPartialInputs = inputs;
    }

    exciter.setAmount (p.excite.load() / 100.0f);
    inputHighPass.setCutoff (p.inputHpf.load());
    tiltEq.setTone (p.tone.load() / 100.0f);
    outputGainSmoothed.setTargetValue (outputGainFromDb (p.output.load()));
    resonatorBank.setDecay (p.decay.load());
    resonatorBank.setGlide (p.glide.load() / 1000.0f);
    mixSmoothed.setTargetValue (p.mix.load() / 100.0f);
}

std::bitset<128> PluginProcessor::getChordNotes() const
{
    return ChordSlots::unpack (playingChord.load (std::memory_order_relaxed));
}

std::array<float, 2> PluginProcessor::takeOutputPeaks()
{
    return { outputPeaks[0].exchange (0.0f, std::memory_order_relaxed), outputPeaks[1].exchange (0.0f, std::memory_order_relaxed) };
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
    auto state = parameters.copyState();
    chordSlots.writeTo (state);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        chordSlots.readFrom (state);
        parameters.replaceState (state);
        currentProgram = parameters.state.getProperty (programProperty, 0);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

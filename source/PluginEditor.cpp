#include "PluginEditor.h"

namespace
{
    constexpr int margin = 14;
    constexpr int gap = 10;
    constexpr int headerHeight = 44;
    constexpr int displayHeight = 68;
    constexpr int sectionHeight = 150;
    constexpr int meterWidth = 210;
    constexpr int keyboardWidth = 14 * 30; // two octaves of 30 px white keys
    constexpr int chordTypeButtonsWidth = ui::ChordTypeButtons::columns * 56;

    juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
    }
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      engine (p.getParameterTree(), params::id::engine),
      chordSource (p.getParameterTree(), params::id::chordSource),
      timbre (p.getParameterTree(), params::id::timbre),
      keyboard (p.getParameterTree(), [&p] { return ui::ChordKeyboard::SoundingChord { p.getChordNotes(), p.getChordRoot() }; }),
      chordTypeButtons (p.getParameterTree()),
      harmonics (p.getParameterTree(), params::id::harmonics),
      brightness (p.getParameterTree(), params::id::brightness),
      detune (p.getParameterTree(), params::id::detune),
      spread (p.getParameterTree(), params::id::spread),
      decay (p.getParameterTree(), params::id::decay),
      glide (p.getParameterTree(), params::id::glide),
      excite (p.getParameterTree(), params::id::excite),
      inputHpf (p.getParameterTree(), params::id::inputHpf),
      tone (p.getParameterTree(), params::id::tone, 0.0f),
      mix (p.getParameterTree(), params::id::mix),
      output (p.getParameterTree(), params::id::output, 0.0f)
{
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);
    presetBox.setTooltip ("Factory presets");
    presetBox.onChange = [this] {
        const auto index = presetBox.getSelectedItemIndex();
        if (index >= 0 && index != processorRef.getCurrentProgram())
        {
            processorRef.setCurrentProgram (index);
            processorRef.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
        }
        shownProgram = index;
    };
    syncPresetBox();
    addAndMakeVisible (presetBox);
    addAndMakeVisible (engine);

    addAndMakeVisible (chordDisplay);
    addAndMakeVisible (meter);

    chordControls.add (chordSource);
    chordControls.add (octaveButtons);
    chordSection.addItem (chordControls, ui::Choice::preferredWidth);
    chordSection.addItem (keyboard, keyboardWidth);
    chordSection.addItem (chordTypeButtons, chordTypeButtonsWidth);

    voicingSection.addItem (timbre, ui::Choice::preferredWidth);
    for (auto* knob : { &harmonics, &brightness, &detune, &spread })
        voicingSection.addItem (*knob, ui::Knob::preferredWidth);

    for (auto* knob : { &decay, &glide, &excite })
        resonanceSection.addItem (*knob, ui::Knob::preferredWidth);

    for (auto* knob : { &inputHpf, &tone, &mix, &output })
        outputSection.addItem (*knob, ui::Knob::preferredWidth);

    for (auto* section : { &chordSection, &voicingSection, &resonanceSection, &outputSection })
        addAndMakeVisible (*section);

   #if JUCE_DEBUG
    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };
   #endif

    const auto contentWidth = std::max ({ chordSection.getMinimumWidth(), voicingSection.getMinimumWidth(),
        resonanceSection.getMinimumWidth() + gap + outputSection.getMinimumWidth() });
    setSize (2 * margin + contentWidth,
        2 * margin + headerHeight + gap + displayHeight + 3 * (gap + sectionHeight));

    // Last, so the change propagates to every child added above (sliders rebuild their value boxes)
    setLookAndFeel (&lookAndFeel);

    updateChordDisplay();
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    meter.update (processorRef.takeOutputPeaks());
    keyboard.refresh();
    updateChordDisplay();
    syncPresetBox();
}

void PluginEditor::syncPresetBox()
{
    // The host can change the program too
    if (const auto current = processorRef.getCurrentProgram(); current != shownProgram)
    {
        shownProgram = current;
        presetBox.setSelectedItemIndex (current, juce::dontSendNotification);
    }
}

void PluginEditor::updateChordDisplay()
{
    auto& tree = processorRef.getParameterTree();
    const auto source = (params::ChordSource) juce::roundToInt (tree.getRawParameterValue (params::id::chordSource)->load());
    const auto type = juce::roundToInt (tree.getRawParameterValue (params::id::chordType)->load());

    // In Internal mode read the chord straight from the parameters, so the name is right even while
    // no audio is being processed; the MIDI modes show what the processor is playing
    auto notes = processorRef.getChordNotes();
    auto rootNote = processorRef.getChordRoot();
    if (source == params::ChordSource::internal)
    {
        rootNote = juce::roundToInt (tree.getRawParameterValue (params::id::root)->load());
        notes.reset();
        for (auto interval : chordify::chordIntervals (type))
            notes[(size_t) std::min (rootNote + interval, 127)] = true;
    }

    juce::StringArray noteNames;
    for (int note = 0; note < 128; ++note)
        if (notes[(size_t) note])
            noteNames.add (noteName (note));

    juce::String name, detail;
    if (noteNames.isEmpty())
    {
        name = juce::String::fromUTF8 ("\xe2\x80\x94"); // em dash
        detail = source == params::ChordSource::internal ? "" : "Waiting for MIDI notes";
    }
    else if (rootNote >= 0)
    {
        name = noteName (rootNote) + " " + params::chordTypeNames[type];
        detail = noteNames.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  "));
    }
    else
    {
        name = noteNames.joinIntoString (" ");
        detail = "MIDI voicing";
    }

    juce::StringArray footnotes;
    footnotes.add ("Chord source: " + params::chordSourceNames[(int) source]);

    juce::StringArray heldNames;
    const auto held = processorRef.getHeldNotes();
    for (int note = 0; note < 128; ++note)
        if (held[(size_t) note])
            heldNames.add (noteName (note));
    if (! heldNames.isEmpty())
        footnotes.add ("MIDI in: " + heldNames.joinIntoString (" "));
    if (juce::roundToInt (tree.getRawParameterValue (params::id::engine)->load()) == (int) params::Engine::spectral)
        footnotes.add ("Spectral engine not built yet, using Resonator");

    chordDisplay.setChord (name, detail, footnotes.joinIntoString (juce::String::fromUTF8 ("   \xc2\xb7   ")));
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);

    auto header = getLocalBounds().reduced (margin).removeFromTop (headerHeight);
    g.setColour (ui::colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (PRODUCT_NAME_WITHOUT_VERSION, header, juce::Justification::centredLeft, false);

    g.setColour (ui::colours::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String ("by Duphon  v" VERSION), header.withTrimmedLeft (122), juce::Justification::centredLeft, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (margin);

    auto header = area.removeFromTop (headerHeight);
   #if JUCE_DEBUG
    inspectButton.setBounds (header.removeFromRight (70).withSizeKeepingCentre (70, 26));
    header.removeFromRight (gap);
   #endif
    engine.setBounds (header.removeFromRight (ui::Choice::preferredWidth).withTrimmedBottom (2));
    header.removeFromRight (gap);
    presetBox.setBounds (header.removeFromRight (220).withTrimmedTop (16).withTrimmedBottom (2));

    area.removeFromTop (gap);
    auto display = area.removeFromTop (displayHeight);
    meter.setBounds (display.removeFromRight (meterWidth).reduced (0, 10));
    display.removeFromRight (gap);
    chordDisplay.setBounds (display);

    area.removeFromTop (gap);
    chordSection.setBounds (area.removeFromTop (sectionHeight));

    area.removeFromTop (gap);
    voicingSection.setBounds (area.removeFromTop (sectionHeight));

    area.removeFromTop (gap);
    auto bottomRow = area.removeFromTop (sectionHeight);
    const auto resonanceWidth = (bottomRow.getWidth() - gap) * resonanceSection.getMinimumWidth()
                                / (resonanceSection.getMinimumWidth() + outputSection.getMinimumWidth());
    resonanceSection.setBounds (bottomRow.removeFromLeft (resonanceWidth));
    bottomRow.removeFromLeft (gap);
    outputSection.setBounds (bottomRow);
}

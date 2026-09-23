#include "PluginEditor.h"

namespace
{
    constexpr int margin = 14;
    constexpr int gap = 10;
    constexpr int headerHeight = 44;
    constexpr int displayHeight = 68;
    constexpr int sectionHeight = 150;
    constexpr int chordSectionHeight = 250;
    constexpr int minContentWidth = 860;
    constexpr int meterWidth = 210;
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      chordPanel (p.getParameterTree(), p.getChordSlots()),
      timbre (p.getParameterTree(), params::id::timbre),
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

    addAndMakeVisible (chordDisplay);
    addAndMakeVisible (meter);

    chordSection.addItem (chordPanel, ui::Section::fillWidth);

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

    const auto contentWidth = std::max ({ minContentWidth, voicingSection.getMinimumWidth(),
        resonanceSection.getMinimumWidth() + gap + outputSection.getMinimumWidth() });
    setSize (2 * margin + contentWidth,
        2 * margin + headerHeight + gap + displayHeight + (gap + chordSectionHeight) + 2 * (gap + sectionHeight));

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
    chordPanel.refresh();
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
    // Read from the slots and Chord Slot rather than the audio thread, so it's right even while
    // no audio is being processed
    const auto active = juce::roundToInt (processorRef.getParameterTree().getRawParameterValue (params::id::chordSlot)->load());
    const auto notes = processorRef.getChordSlots().notesFor (active);

    const auto name = notes.any() ? ChordSlots::noteNames (notes).replace (" ", juce::String::fromUTF8 ("  \xc2\xb7  "))
                                  : juce::String::fromUTF8 ("\xe2\x80\x94"); // em dash
    const auto detail = notes.none() ? juce::String ("No notes: pick some on the keyboard") : juce::String();
    const auto footnote = active == params::pianoSlot ? juce::String ("Playing the notes on the keyboard")
                                                      : "Playing slot " + juce::String (active);

    chordDisplay.setChord (name, detail, footnote);
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
    presetBox.setBounds (header.removeFromRight (220).withSizeKeepingCentre (220, 28));

    area.removeFromTop (gap);
    auto display = area.removeFromTop (displayHeight);
    meter.setBounds (display.removeFromRight (meterWidth).reduced (0, 10));
    display.removeFromRight (gap);
    chordDisplay.setBounds (display);

    area.removeFromTop (gap);
    chordSection.setBounds (area.removeFromTop (chordSectionHeight));

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

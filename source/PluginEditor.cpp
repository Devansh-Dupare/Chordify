#include "PluginEditor.h"

namespace
{
    // Layout grid at the base size, top to bottom like the real instrument:
    // brand + display, the control row, chord memory, then the keys along the bottom edge
    constexpr int margin = 24;
    constexpr int topY = 22, topHeight = 84;
    constexpr int controlsY = 142, controlsHeight = 124;
    constexpr int seamY = 286;
    constexpr int slotsY = 300, slotsHeight = 58;
    constexpr int hintY = 362;
    constexpr int keysY = 386, keysHeight = 152;
    constexpr int leftColumnWidth = 104;
    constexpr int groupGap = 30;

    const juce::String dot = juce::String::fromUTF8 ("\xc2\xb7");
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      timbre (p.getParameterTree(), params::id::timbre, "Timbre", { "ALL", "ODD" }),
      harmonics (p.getParameterTree(), params::id::harmonics, "Harmonics"),
      brightness (p.getParameterTree(), params::id::brightness, "Bright"),
      detune (p.getParameterTree(), params::id::detune, "Detune"),
      spread (p.getParameterTree(), params::id::spread, "Spread"),
      decay (p.getParameterTree(), params::id::decay, "Decay"),
      glide (p.getParameterTree(), params::id::glide, "Glide"),
      excite (p.getParameterTree(), params::id::excite, "Excite"),
      inputHpf (p.getParameterTree(), params::id::inputHpf, "Low Cut"),
      tone (p.getParameterTree(), params::id::tone, "Tone"),
      mix (p.getParameterTree(), params::id::mix, "Mix"),
      output (p.getParameterTree(), params::id::output, "Output"),
      keyboard (p.getParameterTree(), p.getChordSlots()),
      slotButtons (p.getParameterTree(), p.getChordSlots())
{
    addAndMakeVisible (body);
    body.addAndMakeVisible (background);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);
    presetBox.setTooltip ("Factory presets (sound only; your chords stay)");
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

    octaveDown.setTooltip ("Scroll the keyboard down an octave");
    octaveUp.setTooltip ("Scroll the keyboard up an octave");
    clear.setTooltip ("Remove every note from the keyboard");
    octaveDown.onClick = [this] { keyboard.shiftOctave (-1); };
    octaveUp.onClick = [this] { keyboard.shiftOctave (1); };
    clear.onClick = [this] { keyboard.clearNotes(); };
    octaveRange.setJustificationType (juce::Justification::centred);
    octaveRange.setFont (pt1::printFont (10.5f));
    octaveRange.setColour (juce::Label::textColourId, pt1::colours::ink);
    octaveRange.setInterceptsMouseClicks (false, false);

    keyboard.onWindowChanged = [this] { updateOctaveRange(); };
    keyboard.onNoteLimit = [this] { display.flash ("Max 8 notes in a chord"); };
    slotButtons.onNothingToSave = [this] { display.flash ("Pick notes first, then click an empty slot"); };
    updateOctaveRange();

    for (auto* child : std::initializer_list<juce::Component*> { &display, &presetBox, &timbre, &harmonics, &brightness, &detune, &spread,
             &decay, &glide, &excite, &inputHpf, &tone, &mix, &output, &slotButtons, &keyboard, &octaveDown, &octaveUp, &octaveRange, &clear })
        body.addAndMakeVisible (child);

   #if JUCE_DEBUG
    body.addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };
   #endif

    layoutBody();

    // Last, so the change reaches every child added above (sliders rebuild their value boxes)
    setLookAndFeel (&lookAndFeel);

    // Resizable at a fixed aspect ratio: the body is scaled, never stretched
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (baseWidth, baseHeight);

    updateDisplay();
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PluginEditor::resized()
{
    body.setBounds (0, 0, baseWidth, baseHeight);
    body.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
}

void PluginEditor::layoutBody()
{
    const auto contentWidth = baseWidth - 2 * margin;
    background.setBounds (0, 0, baseWidth, baseHeight);

    // Top: brand, the LCD, presets
    const juce::Rectangle<int> brand (margin + 4, topY + 8, 210, 64);
    display.setBounds (margin + 240, topY, 516, topHeight);
    const juce::Rectangle<int> presetLabel (baseWidth - margin - 180, topY + 10, 180, 16);
    presetBox.setBounds (baseWidth - margin - 180, topY + 30, 180, 34);
   #if JUCE_DEBUG
    inspectButton.setBounds (baseWidth - margin - 70, topY + 66, 70, 22);
   #endif

    // Control row: three printed groups of knobs
    const std::vector<std::pair<juce::String, std::vector<juce::Component*>>> groups {
        { "VOICING", { &timbre, &harmonics, &brightness, &detune, &spread } },
        { "RESONANCE", { &decay, &glide, &excite } },
        { "OUTPUT", { &inputHpf, &tone, &mix, &output } },
    };
    int items = 0;
    for (const auto& group : groups)
        items += (int) group.second.size();
    const auto itemWidth = (contentWidth - groupGap * ((int) groups.size() - 1)) / items;

    std::vector<pt1::CaseBackground::PrintedGroup> printedGroups;
    auto x = margin;
    for (const auto& [title, members] : groups)
    {
        const juce::Rectangle<int> area (x, controlsY, itemWidth * (int) members.size(), controlsHeight);
        for (auto* member : members)
        {
            member->setBounds (x, controlsY, itemWidth, controlsHeight);
            x += itemWidth;
        }
        printedGroups.push_back ({ title, area.reduced (6, 0) });
        x += groupGap;
    }

    // Chord memory: eight slots
    const juce::Rectangle<int> memoryLabel (margin, slotsY + 10, leftColumnWidth, 36);
    slotButtons.setBounds (margin + leftColumnWidth, slotsY, contentWidth - leftColumnWidth, slotsHeight);

    // Keys along the bottom edge, with octave and clear to their left
    keyboard.setBounds (margin + leftColumnWidth + 4, keysY, contentWidth - leftColumnWidth - 8, keysHeight);
    const juce::Rectangle<int> octaveLabel (margin, keysY + 4, leftColumnWidth - 12, 16);
    octaveDown.setBounds (margin - 2, keysY + 22, 34, 36);
    octaveUp.setBounds (margin + leftColumnWidth - 44, keysY + 22, 34, 36);
    octaveRange.setBounds (margin + 30, keysY + 60, leftColumnWidth - 40, 18);
    clear.setBounds (margin - 2, keysY + 96, leftColumnWidth - 8, 38);

    background.setLayout (brand, printedGroups, { seamY },
        { { "PRESET", presetLabel, true },
            { "CHORD", memoryLabel.withHeight (18), true },
            { "MEMORY", memoryLabel.withTrimmedTop (18), true },
            { "OCTAVE", octaveLabel, true },
            { "Click keys to pick up to 8 notes  " + dot + "  Click an empty slot to save the chord  " + dot
                    + "  Click a slot to play it  " + dot + "  Right-click a slot to replace or clear it",
                { margin + leftColumnWidth + 6, hintY, contentWidth - leftColumnWidth, 16 }, false } });
}

void PluginEditor::timerCallback()
{
    display.setLevels (processorRef.takeOutputPeaks());
    keyboard.refresh();
    slotButtons.refresh();
    updateDisplay();
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

void PluginEditor::updateOctaveRange()
{
    const auto low = keyboard.getLowestNote();
    octaveRange.setText (juce::MidiMessage::getMidiNoteName (low, true, true, 3) + juce::String::fromUTF8 ("\xe2\x80\x93")
                             + juce::MidiMessage::getMidiNoteName (low + ui::ChordKeyboard::numKeys - 1, true, true, 3),
        juce::dontSendNotification);
}

void PluginEditor::updateDisplay()
{
    // Read from the slots and Chord Slot rather than the audio thread, so it's right even while
    // no audio is being processed
    const auto active = juce::roundToInt (processorRef.getParameterTree().getRawParameterValue (params::id::chordSlot)->load());
    const auto notes = processorRef.getChordSlots().notesFor (active);

    const auto noteText = notes.any() ? ChordSlots::noteNames (notes) : juce::String ("- - -");
    const auto source = active == params::pianoSlot ? juce::String ("Keyboard") : "Slot " + juce::String (active);
    const auto status = notes.any() ? "Playing: " + source + "  " + dot + "  " + juce::String ((int) notes.count()) + " notes"
                                    : juce::String ("No notes: pick some on the keys");
    display.setChord (noteText, status);
}

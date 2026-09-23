#include "PluginEditor.h"

namespace
{
    constexpr int margin = 12;
    constexpr int headerHeight = 40;
    constexpr int choiceHeight = 50;
    constexpr int knobWidth = 90;
    constexpr int knobHeight = 110;
    constexpr int knobsPerRow = 5;
    constexpr int footerHeight = 30;

    juce::String parameterName (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId)
    {
        auto* param = tree.getParameter (paramId);
        jassert (param != nullptr);
        return param->getName (32);
    }
}

PluginEditor::Knob::Knob (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId)
    : attachment (tree, paramId, slider)
{
    label.setText (parameterName (tree, paramId), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobWidth - 10, 20);
    addAndMakeVisible (label);
    addAndMakeVisible (slider);
}

void PluginEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (20));
    slider.setBounds (area);
}

PluginEditor::Choice::Choice (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId)
    : attachment (tree, paramId, box)
{
    // The attachment only selects an item; the items themselves come from the parameter
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (tree.getParameter (paramId)))
        box.addItemList (param->choices, 1);
    box.setSelectedItemIndex (juce::roundToInt (tree.getRawParameterValue (paramId)->load()), juce::dontSendNotification);

    label.setText (parameterName (tree, paramId), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void PluginEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (20));
    box.setBounds (area.reduced (0, 2));
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    auto& tree = processorRef.getParameterTree();

    for (auto* paramId : { params::id::engine, params::id::chordSource, params::id::chordType, params::id::timbre })
        addAndMakeVisible (*choices.emplace_back (std::make_unique<Choice> (tree, paramId)));

    for (auto* paramId : { params::id::root, params::id::harmonics, params::id::detune, params::id::spread, params::id::glide, params::id::decay,
             params::id::excite, params::id::brightness, params::id::inputHpf, params::id::tone, params::id::mix, params::id::output })
        addAndMakeVisible (*knobs.emplace_back (std::make_unique<Knob> (tree, paramId)));

    midiNotesLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (midiNotesLabel);

    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    const auto rows = ((int) knobs.size() + knobsPerRow - 1) / knobsPerRow;
    setSize (2 * margin + knobsPerRow * knobWidth,
        2 * margin + headerHeight + choiceHeight + rows * knobHeight + footerHeight);

    timerCallback();
    startTimerHz (20);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::timerCallback()
{
    const auto held = processorRef.getHeldNotes();

    juce::StringArray names;
    for (int note = 0; note < 128; ++note)
        if (held[(size_t) note])
            names.add (juce::MidiMessage::getMidiNoteName (note, true, true, 3));

    const auto text = "MIDI in: " + (names.isEmpty() ? juce::String ("-") : names.joinIntoString (" "));
    if (midiNotesLabel.getText() != text)
        midiNotesLabel.setText (text, juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto header = getLocalBounds().reduced (margin).removeFromTop (headerHeight);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (PRODUCT_NAME_WITHOUT_VERSION, header, juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String ("v" VERSION " ") + CMAKE_BUILD_TYPE, header.withTrimmedRight (80), juce::Justification::centredRight, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (margin);

    auto header = area.removeFromTop (headerHeight);
    inspectButton.setBounds (header.removeFromRight (70).reduced (0, 8));

    auto choiceRow = area.removeFromTop (choiceHeight);
    const auto choiceWidth = choiceRow.getWidth() / (int) choices.size();
    for (auto& choice : choices)
        choice->setBounds (choiceRow.removeFromLeft (choiceWidth).reduced (4, 0));

    midiNotesLabel.setBounds (area.removeFromBottom (footerHeight));

    for (size_t i = 0; i < knobs.size(); i += knobsPerRow)
    {
        auto row = area.removeFromTop (knobHeight);
        for (size_t k = i; k < std::min (knobs.size(), i + knobsPerRow); ++k)
            knobs[k]->setBounds (row.removeFromLeft (knobWidth));
    }
}

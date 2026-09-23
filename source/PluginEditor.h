#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
// Placeholder UI: one knob or dropdown per parameter plus a held-MIDI-notes readout.
// The visual design pass happens in Phase 5.
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob : juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
        void resized() override;

        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    struct Choice : juce::Component
    {
        Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
        void resized() override;

        juce::ComboBox box;
        juce::Label label;
        juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
    };

    void timerCallback() override;

    PluginProcessor& processorRef;

    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Label midiNotesLabel;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

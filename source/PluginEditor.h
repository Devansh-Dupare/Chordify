#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ChordKeyboard.h"
#include "ui/Widgets.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateChordDisplay();
    void syncPresetBox();

    PluginProcessor& processorRef;
    ui::LookAndFeel lookAndFeel;

    // Header
    juce::ComboBox presetBox;

    // Display strip
    ui::ChordDisplay chordDisplay;
    ui::LevelMeter meter;

    // Sections
    ui::Section chordSection { "Chord" }, voicingSection { "Voicing" }, resonanceSection { "Resonance" }, outputSection { "Output" };
    ui::ChordPanel chordPanel;
    ui::Choice timbre;
    ui::Knob harmonics, brightness, detune, spread;
    ui::Knob decay, glide, excite;
    ui::Knob inputHpf, tone, mix, output;

    int shownProgram = -1;

   #if JUCE_DEBUG
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect" };
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

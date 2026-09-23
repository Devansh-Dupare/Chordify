#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ChordKeyboard.h"
#include "ui/Pt1LookAndFeel.h"
#include "ui/Pt1Widgets.h"

//==============================================================================
// Casio PT-1-inspired skin. Everything is laid out once at a fixed base size inside `body`, and
// the whole body is scaled when the window is resized, so nothing stretches or distorts. Cached
// artwork is rendered at the physical pixel scale, so it stays sharp at any zoom.
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseWidth = 1000;
    static constexpr int baseHeight = 560;

    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void resized() override;

private:
    void timerCallback() override;
    void layoutBody();
    void updateDisplay();
    void updateOctaveRange();
    void syncPresetBox();

    PluginProcessor& processorRef;
    pt1::LookAndFeel lookAndFeel;

    juce::Component body;
    pt1::CaseBackground background;
    pt1::DisplayWindow display;
    juce::ComboBox presetBox;

    pt1::RockerSwitch timbre;
    pt1::Knob harmonics, brightness, detune, spread;
    pt1::Knob decay, glide, excite;
    pt1::Knob inputHpf, tone, mix, output;

    ui::ChordKeyboard keyboard;
    ui::ChordSlotButtons slotButtons;
    juce::TextButton octaveDown { juce::String::fromUTF8 ("\xe2\x88\x92") }, octaveUp { "+" }, clear { "Clear" };
    juce::Label octaveRange;
    juce::TooltipWindow tooltips { this, 600 };

    int shownProgram = -1;

   #if JUCE_DEBUG
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect" };
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

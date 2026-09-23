#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
    namespace colours
    {
        inline const juce::Colour background { 0xff14161b };
        inline const juce::Colour panel { 0xff1d2129 };
        inline const juce::Colour outline { 0xff2a2f3a };
        inline const juce::Colour track { 0xff2c323d };
        inline const juce::Colour text { 0xffe6e8ec };
        inline const juce::Colour textDim { 0xff8a91a0 };
        inline const juce::Colour accent { 0xff5ec8b8 };
        inline const juce::Colour warm { 0xfff2a65a };
        inline const juce::Colour hot { 0xffef5b5b };
    }

    // Set on a Slider (getProperties().set (arcOriginProperty, value)) to draw its value arc from
    // that value (e.g. 0 dB) instead of from the minimum
    inline const juce::Identifier arcOriginProperty { "arcOrigin" };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
            float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
            int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

        juce::Label* createSliderTextBox (juce::Slider&) override;
    };
}

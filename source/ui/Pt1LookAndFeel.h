#pragma once

#include "Pt1Style.h"
#include <map>

namespace pt1
{
    // Draws a raised rubber button (PT-1 style): soft drop shadow, beveled cream body. When down it
    // reads as pushed in: no shadow, inverted gradient.
    void drawRubberButton (juce::Graphics&, juce::Rectangle<float> bounds, bool down, bool highlighted, float cornerSize = 6.0f);

    // Knobs, buttons, dropdown and menus for the PT-1 skin. Set on the editor only.
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
            float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
            int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        juce::Font getPopupMenuFont() override;

        juce::Label* createSliderTextBox (juce::Slider&) override;

    private:
        // The static knob layers (shadow, skirt, ticks, cap, rim highlight) for one size, scale,
        // angle range and pressed state, rendered once and reused on every repaint
        const juce::Image& knobLayers (float diameter, float scale, float startAngle, float endAngle, bool pressed);

        std::map<std::tuple<int, int, int, int, bool>, juce::Image> knobCache;
    };
}

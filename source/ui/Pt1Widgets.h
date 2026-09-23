#pragma once

#include "Pt1Style.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pt1
{
    // The cream plastic case: gradient, fine grain, a seam, faux screws and the printed group
    // titles and lines. Rendered once per size and display scale, then cached.
    class CaseBackground : public juce::Component
    {
    public:
        struct PrintedGroup
        {
            juce::String title;
            juce::Rectangle<int> area; // the controls the title and bracket line sit above
        };

        struct PrintedText
        {
            juce::String text;
            juce::Rectangle<int> area;
            bool strong = true; // ink; otherwise the dimmer ink for small print
        };

        CaseBackground();

        void setLayout (juce::Rectangle<int> brandArea, std::vector<PrintedGroup> groups, std::vector<int> seamYs, std::vector<PrintedText> printedText);
        void paint (juce::Graphics&) override;

    private:
        void render (juce::Graphics&) const;

        juce::Rectangle<int> brand;
        std::vector<PrintedGroup> groups;
        std::vector<int> seams;
        std::vector<PrintedText> text;
        juce::Image cache;
        float cacheScale = 0.0f;
    };

    // Recessed LCD window: the chord's notes, what's playing, a level meter and short messages
    class DisplayWindow : public juce::Component, private juce::Timer
    {
    public:
        void setChord (const juce::String& notes, const juce::String& status);
        void setLevels (std::array<float, 2> peaks); // linear peaks since the last call
        void flash (const juce::String& message);    // shown in place of the status for a few seconds

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        juce::String notesText, statusText, message;
        std::array<float, 2> levelDb { -60.0f, -60.0f };
    };

    // A knob with its printed name above and its value below
    class Knob : public juce::Component
    {
    public:
        static constexpr int preferredWidth = 74;

        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& printedName);
        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        juce::String name;
        juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // PT-1 style rocker for a two-way choice parameter: the chosen side is pushed in
    class RockerSwitch : public juce::Component
    {
    public:
        static constexpr int preferredWidth = 74;

        RockerSwitch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& printedName,
            juce::StringArray sideLabels);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        int getSelected() const { return selected; }

    private:
        juce::Rectangle<float> switchBounds() const;

        juce::String name;
        juce::StringArray labels;
        juce::ParameterAttachment attachment;
        int selected = 0;
    };
}

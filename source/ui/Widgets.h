#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
    // Rotary knob with its parameter name above and value below
    class Knob : public juce::Component
    {
    public:
        static constexpr int preferredWidth = 84;

        // arcOrigin: value the arc is drawn from (e.g. 0 dB for a gain); defaults to the minimum
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, std::optional<float> arcOrigin = {});
        void resized() override;

    private:
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // Dropdown for a choice parameter, with its name above
    class Choice : public juce::Component
    {
    public:
        static constexpr int preferredWidth = 150;

        Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
        void resized() override;

    private:
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    // Titled panel that lays out items in a row. Fixed-width items share any spare width as even
    // gaps; an item added with fillWidth takes all the spare width instead.
    class Section : public juce::Component
    {
    public:
        static constexpr int fillWidth = 0;

        explicit Section (juce::String title);

        void addItem (juce::Component& item, int width);
        int getMinimumWidth() const;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        static constexpr int titleHeight = 24;
        static constexpr int padding = 10;

        juce::String title;
        std::vector<std::pair<juce::Component*, int>> items;
    };

    // Vertical stack of components sharing one slot in a Section (e.g. two dropdowns)
    class Stack : public juce::Component
    {
    public:
        void add (juce::Component& item);
        void resized() override;

    private:
        std::vector<juce::Component*> items;
    };

    // The chord that is sounding: a large name and the individual notes
    class ChordDisplay : public juce::Component
    {
    public:
        void setChord (const juce::String& name, const juce::String& detail, const juce::String& footnote);
        void paint (juce::Graphics&) override;

    private:
        juce::String chordName, chordDetail, footnoteText;
    };

    // Stereo peak meter, -60..+6 dBFS, with peak hold
    class LevelMeter : public juce::Component
    {
    public:
        static constexpr float minDb = -60.0f;
        static constexpr float maxDb = 6.0f;

        // Called at the editor's timer rate with the peaks since the last call
        void update (std::array<float, 2> peaks);
        void paint (juce::Graphics&) override;

    private:
        std::array<float, 2> levelDb { minDb, minDb };
        std::array<float, 2> holdDb { minDb, minDb };
        std::array<int, 2> holdFrames {};
    };
}

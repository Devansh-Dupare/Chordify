#pragma once

#include "Theme.h"
#include <bitset>
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
    // Two-octave piano for picking the chord root, bound to the Root parameter in both directions:
    // clicking (or dragging across) keys writes Root, and any change to Root, Chord Type or Chord
    // Source (automation, presets, session reload) repaints the highlighted chord.
    //
    // In Internal mode the highlight is computed from the parameters, so it is right even when no
    // audio is running. In the MIDI modes it shows the chord actually sounding, and a click switches
    // Chord Source back to Internal, because clicking a key means "play this chord".
    class ChordKeyboard : public juce::Component
    {
    public:
        static constexpr int numOctaves = 2;
        static constexpr int numKeys = numOctaves * 12;

        struct SoundingChord
        {
            std::bitset<128> notes;
            int root = -1; // -1 when the chord has no single root (MIDI voicing mode)
        };

        // soundingChord: what the processor is playing (for the MIDI modes); called on the message thread
        ChordKeyboard (juce::AudioProcessorValueTreeState&, std::function<SoundingChord()> soundingChord);

        // What a click on this note does: set Root (and switch to Internal if needed)
        void selectRoot (int note);
        void shiftOctave (int direction);

        // Re-read the sounding chord; the editor calls this from its timer
        void refresh();

        int getLowestNote() const { return windowStart; }
        std::optional<int> noteAt (juce::Point<float>) const; // the key under a point, if any
        std::function<void()> onWindowChanged; // the visible octaves moved
        std::bitset<128> getHighlightedNotes() const { return highlighted; }
        int getHighlightedRoot() const { return highlightedRoot; }

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;

    private:
        void update();
        juce::Rectangle<float> keyBounds (int note) const;
        static bool isBlack (int note);

        juce::RangedAudioParameter& rootParam;
        juce::RangedAudioParameter& chordTypeParam;
        juce::RangedAudioParameter& sourceParam;
        juce::ParameterAttachment rootAttachment, chordTypeAttachment, sourceAttachment;
        std::function<SoundingChord()> soundingChord;

        int root = 48, chordType = 0, source = 0;
        int windowStart = 48; // a C
        std::bitset<128> highlighted;
        int highlightedRoot = -1;
        int lastDraggedNote = -1;
    };

    // Octave down/up buttons for the keyboard, showing the visible range
    class OctaveButtons : public juce::Component
    {
    public:
        explicit OctaveButtons (ChordKeyboard&);
        void resized() override;

    private:
        void updateRange();

        ChordKeyboard& keyboard;
        juce::Label caption, range;
        juce::TextButton down { juce::String::fromUTF8 ("\xe2\x88\x92") }, up { "+" };
    };

    // One toggle button per chord type, bound to the Chord Type parameter in both directions
    class ChordTypeButtons : public juce::Component
    {
    public:
        static constexpr int columns = 5;

        explicit ChordTypeButtons (juce::AudioProcessorValueTreeState&);
        void resized() override;

    private:
        juce::RangedAudioParameter& param;
        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
    };
}

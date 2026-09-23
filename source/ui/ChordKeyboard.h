#pragma once

#include "../state/ChordSlots.h"
#include "Pt1Style.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
    // Piano for building a chord: click keys to add or remove notes (drag to paint several).
    //
    // It shows the chord that Chord Slot selects: the piano selection, or a saved slot. Editing
    // always writes the piano selection, starting from the chord shown, and switches Chord Slot to
    // Piano, so you hear each change straight away and a saved slot only changes when you save
    // over it. Chord Slot is bound with a ParameterAttachment, so automation relights the keys.
    class ChordKeyboard : public juce::Component
    {
    public:
        static constexpr int numOctaves = 3;
        static constexpr int numKeys = numOctaves * 12;

        ChordKeyboard (juce::AudioProcessorValueTreeState&, ChordSlots&);

        // Returns false if the note couldn't be added because the chord already has the maximum
        bool toggleNote (int note);
        void clearNotes();

        // Scroll the view by an octave (doesn't change the chord)
        void shiftOctave (int direction);

        // Re-read the chord; the editor calls this from its timer to catch slot edits
        void refresh();

        ChordSlots::Notes getShownNotes() const { return shown; }
        int getLowestNote() const { return windowStart; }
        std::optional<int> noteAt (juce::Point<float>) const; // the key under a point, if any

        std::function<void()> onWindowChanged; // the visible octaves moved
        std::function<void()> onNoteLimit;     // tried to add a note to a full chord

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        // Pre-rendered key layers, one image per key colour and state, so a repaint only blits
        // images. Rebuilt when the key size or display scale changes.
        enum KeyState
        {
            idle,
            hover,
            selected,
            selectedHover,
            pressed,
            numKeyStates
        };

        struct KeyArt
        {
            float scale = 0.0f;
            juce::Point<float> whiteSize, blackSize;
            std::array<juce::Image, numKeyStates> white, black;
        };

        void update();
        bool setNote (int note, bool on);
        void setHovered (int note);
        juce::Rectangle<float> keyBounds (int note) const;
        KeyState stateOf (int note) const;
        const KeyArt& keyArt (float scale);
        static bool isBlack (int note);

        juce::RangedAudioParameter& slotParam;
        ChordSlots& slots;
        juce::ParameterAttachment slotAttachment;

        int activeSlot = params::pianoSlot;
        ChordSlots::Notes shown;
        int windowStart = 48; // always a C

        bool dragAdds = true;
        int lastDraggedNote = -1;
        int hoveredNote = -1;
        int pressedNote = -1;
        KeyArt art;
    };

    // The saved chord slots. Click an empty slot to save the chord on the keyboard into it, click a
    // filled slot to play it; right-click to save over it or clear it. The playing slot is lit.
    class ChordSlotButtons : public juce::Component
    {
    public:
        ChordSlotButtons (juce::AudioProcessorValueTreeState&, ChordSlots&);
        ~ChordSlotButtons() override;

        void clickSlot (int index);  // index 0..numSlots-1
        void saveToSlot (int index); // the chord shown on the keyboard
        void clearSlot (int index);

        void refresh();
        void resized() override;

        std::function<void()> onNothingToSave; // clicked an empty slot with no notes picked

    private:
        class SlotButton;

        ChordSlots::Notes shownNotes() const;
        void playSlot (int index);
        void showMenu (int index);

        juce::RangedAudioParameter& slotParam;
        ChordSlots& slots;
        juce::OwnedArray<SlotButton> buttons;
        juce::ParameterAttachment attachment;
        int activeSlot = params::pianoSlot;
    };
}

#pragma once

#include "../dsp/ChordMapper.h"
#include "../params/Parameters.h"
#include <juce_data_structures/juce_data_structures.h>

// The notes picked on the piano and the saved chord slots.
//
// Edited on the message thread, read by the audio thread, saved with the plugin state. A chord has
// at most chordify::maxChordNotes (8) notes, so each one packs into a single 64-bit word (one byte
// per note, lowest first, 0xff = unused). That makes every read and write one atomic operation:
// the audio thread can never see half of an edit.
class ChordSlots
{
public:
    using Notes = std::bitset<128>;
    static constexpr int numSlots = params::numSlots;

    ChordSlots();

    Notes getPiano() const { return unpack (piano.load (std::memory_order_acquire)); }
    void setPiano (const Notes& notes) { piano.store (pack (notes), std::memory_order_release); }

    // index 0..numSlots-1
    Notes getSlot (int index) const;
    void setSlot (int index, const Notes& notes);

    // The notes a Chord Slot value plays: params::pianoSlot is the piano, 1..numSlots a slot
    Notes notesFor (int chordSlot) const;

    // Saved as properties of the state tree, e.g. piano="48 52 55" slot1="57 60 64"
    void writeTo (juce::ValueTree& state) const;
    void readFrom (const juce::ValueTree& state);

    static std::uint64_t pack (const Notes&);
    static Notes unpack (std::uint64_t);
    static juce::String toString (const Notes&);
    static juce::String noteNames (const Notes&); // "C2 E2 G2" (middle C = C3), for display
    static Notes fromString (const juce::String&);

    static const Notes& defaultPiano(); // C2 E2 G2, so a new instance isn't silent

private:
    std::atomic<std::uint64_t> piano;
    std::array<std::atomic<std::uint64_t>, numSlots> slots;
};

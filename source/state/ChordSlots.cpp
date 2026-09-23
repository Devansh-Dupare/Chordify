#include "ChordSlots.h"

namespace
{
    constexpr std::uint64_t emptyChord = ~std::uint64_t { 0 }; // every byte 0xff
    const juce::Identifier pianoProperty { "piano" };

    juce::Identifier slotProperty (int index)
    {
        return "slot" + juce::String (index + 1);
    }
}

ChordSlots::ChordSlots()
    : piano (pack (defaultPiano()))
{
    for (auto& slot : slots)
        slot.store (emptyChord);
}

const ChordSlots::Notes& ChordSlots::defaultPiano()
{
    static const Notes notes = [] {
        Notes n;
        for (auto note : { 48, 52, 55 })
            n[(size_t) note] = true;
        return n;
    }();
    return notes;
}

ChordSlots::Notes ChordSlots::getSlot (int index) const
{
    jassert (index >= 0 && index < numSlots);
    return unpack (slots[(size_t) std::clamp (index, 0, numSlots - 1)].load (std::memory_order_acquire));
}

void ChordSlots::setSlot (int index, const Notes& notes)
{
    jassert (index >= 0 && index < numSlots);
    slots[(size_t) std::clamp (index, 0, numSlots - 1)].store (pack (notes), std::memory_order_release);
}

ChordSlots::Notes ChordSlots::notesFor (int chordSlot) const
{
    if (chordSlot <= params::pianoSlot || chordSlot > numSlots)
        return getPiano();
    return getSlot (chordSlot - 1);
}

std::uint64_t ChordSlots::pack (const Notes& notes)
{
    std::uint64_t packed = emptyChord;
    int count = 0;
    for (size_t note = 0; note < notes.size() && count < chordify::maxChordNotes; ++note)
    {
        if (notes[note])
        {
            const auto shift = 8 * count++;
            packed = (packed & ~(std::uint64_t { 0xff } << shift)) | ((std::uint64_t) note << shift);
        }
    }
    return packed;
}

ChordSlots::Notes ChordSlots::unpack (std::uint64_t packed)
{
    Notes notes;
    for (int i = 0; i < chordify::maxChordNotes; ++i)
        if (const auto note = (packed >> (8 * i)) & 0xff; note < 128)
            notes[(size_t) note] = true;
    return notes;
}

juce::String ChordSlots::toString (const Notes& notes)
{
    juce::StringArray numbers;
    for (size_t note = 0; note < notes.size(); ++note)
        if (notes[note])
            numbers.add (juce::String ((int) note));
    return numbers.joinIntoString (" ");
}

juce::String ChordSlots::noteNames (const Notes& notes)
{
    juce::StringArray names;
    for (size_t note = 0; note < notes.size(); ++note)
        if (notes[note])
            names.add (juce::MidiMessage::getMidiNoteName ((int) note, true, true, 3));
    return names.joinIntoString (" ");
}

ChordSlots::Notes ChordSlots::fromString (const juce::String& text)
{
    Notes notes;
    for (const auto& token : juce::StringArray::fromTokens (text, " ", ""))
        if (token.containsOnly ("0123456789") && token.isNotEmpty())
            if (const auto note = token.getIntValue(); note >= 0 && note < 128)
                notes[(size_t) note] = true;
    return unpack (pack (notes)); // enforce the note limit
}

void ChordSlots::writeTo (juce::ValueTree& state) const
{
    state.setProperty (pianoProperty, toString (getPiano()), nullptr);
    for (int i = 0; i < numSlots; ++i)
        state.setProperty (slotProperty (i), toString (getSlot (i)), nullptr);
}

void ChordSlots::readFrom (const juce::ValueTree& state)
{
    // A state saved without chords (e.g. an older session) keeps the defaults
    if (state.hasProperty (pianoProperty))
        setPiano (fromString (state[pianoProperty]));
    for (int i = 0; i < numSlots; ++i)
        if (state.hasProperty (slotProperty (i)))
            setSlot (i, fromString (state[slotProperty (i)]));
}

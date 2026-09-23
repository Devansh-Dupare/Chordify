#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace chordify
{
    inline constexpr int maxVoices = 8;
    inline constexpr int maxHarmonics = 16;
    inline constexpr int maxPartials = maxVoices * maxHarmonics;

    // f = 440 * 2^((n - 69) / 12); fractional notes allow pitch bend and detune
    double midiNoteToHz (double note);

    // Semitone offsets from the root for each entry of params::chordTypeNames
    std::span<const int> chordIntervals (int chordType);

    // Voice and PartialSettings compare floats exactly on purpose: equality only detects
    // "nothing changed since last block", so the partials can skip being recomputed.
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

    // A chord tone: its fundamental and whether it is currently fed with input
    struct Voice
    {
        float note = 0.0f;  // MIDI note number, fractional after pitch bend
        bool used = false;  // false until the voice has ever sounded
        bool gated = false; // true while the note is held; released voices ring out

        bool operator== (const Voice&) const = default;
    };

    using Voices = std::array<Voice, maxVoices>;

    // Chord tones from the internal root + chord type parameters (all gated). Voices that were
    // part of the previous chord but not this one keep their note, released, so they ring out.
    Voices internalChord (float rootNote, int chordType, const Voices& previous);

    // Every voice released (keeping its pitch), so the chord rings out
    Voices releaseAll (Voices voices);

    // Assigns incoming MIDI notes to voices with last-note priority: once every voice is held,
    // a new note steals the voice held the longest. Otherwise it takes a never-used voice, or
    // the voice released the longest ago, so recent tails keep ringing.
    class VoiceAllocator
    {
    public:
        void noteOn (int note);
        void noteOff (int note);
        void allNotesOff();
        void reset();

        // Pitch wheel position in -1..1, mapped to +/- bendRangeSemitones
        void setPitchBend (float amount) { pitchBend = amount; }
        static constexpr float bendRangeSemitones = 2.0f;

        Voices getVoices() const;

        // The most recently pressed note that is still held, with pitch bend applied
        std::optional<float> lastHeldNote() const;

    private:
        struct Slot
        {
            int note = -1;
            bool gated = false;
            std::uint64_t age = 0; // order of the last note-on / note-off
        };

        std::array<Slot, maxVoices> slots {};
        std::uint64_t counter = 0;
        float pitchBend = 0.0f;
    };

    // Per-partial targets, laid out as [voice * maxHarmonics + harmonicIndex] so each resonator
    // keeps the same slot while its voice holds a note (letting frequencies glide smoothly).
    struct PartialGrid
    {
        std::array<float, maxPartials> freqHz {};
        std::array<float, maxPartials> amplitude {}; // 0 = slot unused
        std::array<float, maxPartials> gate {};      // 1 = fed with input, 0 = ringing out
        std::array<float, maxPartials> pan {};       // -1 (left) .. 1 (right)
    };

    struct PartialSettings
    {
        int numHarmonics = 8;
        float brightness = 0.5f; // 0..1: amplitude k^-p with p = 2 (dark) .. 0 (bright); 0.5 = 1/k
        bool oddOnly = false;    // odd harmonics only (square-like) instead of all (saw-like)
        float detuneCents = 0.0f;
        float spread = 0.0f;     // 0..1 stereo width
        double sampleRate = 48000.0;

        bool operator== (const PartialSettings&) const = default;
    };

#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

    // Highest partial frequency kept, as a fraction of the sample rate
    inline constexpr double maxPartialFraction = 0.45;
    inline constexpr double minPartialHz = 20.0;

    // Two held partials closer than this are merged into one resonator (e.g. a fifth's shared harmonics)
    inline constexpr double mergeCents = 10.0;

    PartialGrid computePartials (const Voices& voices, const PartialSettings& settings);
}

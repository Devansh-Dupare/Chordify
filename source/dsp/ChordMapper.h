#pragma once

#include <array>
#include <bitset>
#include <cstdint>

namespace chordify
{
    inline constexpr int maxVoices = 8;
    inline constexpr int maxHarmonics = 16;
    inline constexpr int maxPartials = maxVoices * maxHarmonics;

    // f = 440 * 2^((n - 69) / 12); fractional notes allow detune
    double midiNoteToHz (double note);

    // Voice and PartialSettings compare floats exactly on purpose: equality only detects
    // "nothing changed since last block", so the partials can skip being recomputed.
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

    // A chord tone: its fundamental and whether it is currently fed with input
    struct Voice
    {
        float note = 0.0f;  // MIDI note number
        bool used = false;  // false until the voice has ever sounded
        bool gated = false; // true while the note is in the chord; released voices ring out

        bool operator== (const Voice&) const = default;
    };

    using Voices = std::array<Voice, maxVoices>;

    // Maximum number of notes in a chord (one voice per note)
    inline constexpr int maxChordNotes = maxVoices;

    // One voice per selected note, lowest note first, so a chord change glides each voice to the
    // note in the same position of the new chord. Voices that were sounding before but aren't
    // needed now keep their note, released, so they ring out. Notes beyond maxChordNotes are ignored.
    Voices chordFromNotes (const std::bitset<128>& notes, const Voices& previous);

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

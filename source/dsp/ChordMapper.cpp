#include "ChordMapper.h"

#include <algorithm>
#include <cmath>

namespace chordify
{
    namespace
    {
        constexpr int major[] = { 0, 4, 7 };
        constexpr int minor[] = { 0, 3, 7 };
        constexpr int diminished[] = { 0, 3, 6 };
        constexpr int augmented[] = { 0, 4, 8 };
        constexpr int sus2[] = { 0, 2, 7 };
        constexpr int sus4[] = { 0, 5, 7 };
        constexpr int major7[] = { 0, 4, 7, 11 };
        constexpr int minor7[] = { 0, 3, 7, 10 };
        constexpr int dominant7[] = { 0, 4, 7, 10 };
        constexpr int power[] = { 0, 7 };

        // Same order as params::chordTypeNames
        constexpr std::span<const int> chordTable[] = { major, minor, diminished, augmented, sus2, sus4, major7, minor7, dominant7, power };

        // Deterministic value in -1..1 per slot, so detune and panning are stable between renders
        float slotPattern (int slot, std::uint32_t salt)
        {
            auto h = (std::uint32_t) (slot + 1) * 2654435761u ^ salt;
            h ^= h >> 15;
            h *= 0x2c1b3c6du;
            h ^= h >> 12;
            h *= 0x297a2d39u;
            h ^= h >> 15;
            return (float) h / 4294967295.0f * 2.0f - 1.0f;
        }
    }

    double midiNoteToHz (double note)
    {
        return 440.0 * std::exp2 ((note - 69.0) / 12.0);
    }

    std::span<const int> chordIntervals (int chordType)
    {
        return chordTable[std::clamp (chordType, 0, (int) std::size (chordTable) - 1)];
    }

    Voices internalChord (int rootNote, int chordType, const Voices& previous)
    {
        const auto intervals = chordIntervals (chordType);

        Voices voices {};
        for (size_t i = 0; i < voices.size(); ++i)
        {
            if (i < intervals.size())
                voices[i] = { (float) (rootNote + intervals[i]), true, true };
            else if (previous[i].used)
                voices[i] = { previous[i].note, true, false }; // dropped chord tone rings out
        }
        return voices;
    }

    //==============================================================================
    void VoiceAllocator::noteOn (int note)
    {
        ++counter;

        for (auto& slot : slots)
        {
            if (slot.note == note)
            {
                slot.gated = true;
                slot.age = counter;
                return;
            }
        }

        const auto older = [] (const Slot& a, const Slot& b) { return a.age < b.age; };

        // Prefer a free voice: never used first, otherwise the one released longest ago
        Slot* chosen = nullptr;
        for (auto& slot : slots)
            if (! slot.gated && (chosen == nullptr || (slot.note < 0 && chosen->note >= 0) || ((slot.note < 0) == (chosen->note < 0) && older (slot, *chosen))))
                chosen = &slot;

        // Every voice is held: steal the one held the longest
        if (chosen == nullptr)
            chosen = &*std::min_element (slots.begin(), slots.end(), older);

        *chosen = { note, true, counter };
    }

    void VoiceAllocator::noteOff (int note)
    {
        ++counter;

        for (auto& slot : slots)
        {
            if (slot.note == note && slot.gated)
            {
                slot.gated = false;
                slot.age = counter;
            }
        }
    }

    void VoiceAllocator::allNotesOff()
    {
        for (auto& slot : slots)
            slot.gated = false;
    }

    void VoiceAllocator::reset()
    {
        slots = {};
        counter = 0;
        pitchBend = 0.0f;
    }

    Voices VoiceAllocator::getVoices() const
    {
        Voices voices {};
        for (size_t i = 0; i < slots.size(); ++i)
            if (slots[i].note >= 0)
                voices[i] = { (float) slots[i].note + pitchBend * bendRangeSemitones, true, slots[i].gated };
        return voices;
    }

    //==============================================================================
    PartialGrid computePartials (const Voices& voices, const PartialSettings& settings)
    {
        PartialGrid grid;

        const auto numHarmonics = std::clamp (settings.numHarmonics, 1, maxHarmonics);
        const auto exponent = 2.0f * (1.0f - std::clamp (settings.brightness, 0.0f, 1.0f));
        const auto maxHz = maxPartialFraction * settings.sampleRate;
        const auto mergeRatio = std::exp2 (mergeCents / 1200.0);

        for (int v = 0; v < maxVoices; ++v)
        {
            const auto& voice = voices[(size_t) v];
            if (! voice.used)
                continue;

            const auto fundamental = midiNoteToHz (voice.note);

            for (int h = 0; h < numHarmonics; ++h)
            {
                const auto slot = (size_t) (v * maxHarmonics + h);
                const auto harmonic = settings.oddOnly ? 2 * h + 1 : h + 1;
                const auto freq = fundamental * harmonic;

                if (freq > maxHz || freq < minPartialHz)
                    continue;

                grid.freqHz[slot] = (float) freq;
                grid.amplitude[slot] = std::pow ((float) harmonic, -exponent);
                grid.gate[slot] = voice.gated ? 1.0f : 0.0f;
            }
        }

        // Merge held partials that land on (nearly) the same frequency, keeping the louder
        // amplitude on the earlier slot. Released partials are left alone so tails ring out intact.
        for (size_t j = 0; j < grid.freqHz.size(); ++j)
        {
            if (grid.amplitude[j] <= 0.0f || grid.gate[j] <= 0.0f)
                continue;

            for (size_t i = 0; i < j; ++i)
            {
                if (grid.amplitude[i] <= 0.0f || grid.gate[i] <= 0.0f)
                    continue;

                const auto ratio = (double) grid.freqHz[j] / grid.freqHz[i];
                if (ratio < mergeRatio && ratio > 1.0 / mergeRatio)
                {
                    grid.amplitude[i] = std::max (grid.amplitude[i], grid.amplitude[j]);
                    grid.amplitude[j] = 0.0f;
                    grid.gate[j] = 0.0f;
                    break;
                }
            }
        }

        // Detune and pan after merging, so merged partials are compared at their true pitch
        for (int slot = 0; slot < maxPartials; ++slot)
        {
            const auto s = (size_t) slot;
            if (grid.amplitude[s] <= 0.0f)
                continue;

            grid.freqHz[s] *= (float) std::exp2 (settings.detuneCents * slotPattern (slot, 0x9e3779b9u) / 1200.0);
            grid.pan[s] = std::clamp (settings.spread, 0.0f, 1.0f) * slotPattern (slot, 0x85ebca6bu);
        }

        return grid;
    }
}

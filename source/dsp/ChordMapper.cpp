#include "ChordMapper.h"

#include <algorithm>
#include <cmath>

namespace chordify
{
    namespace
    {
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

    Voices chordFromNotes (const std::bitset<128>& notes, const Voices& previous)
    {
        Voices voices {};
        size_t next = 0;
        for (size_t note = 0; note < notes.size() && next < voices.size(); ++note)
            if (notes[note])
                voices[next++] = { (float) note, true, true };

        for (; next < voices.size(); ++next)
            if (previous[next].used)
                voices[next] = { previous[next].note, true, false }; // no longer in the chord: ring out

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

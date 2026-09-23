#include "helpers/dsp_test_helpers.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <dsp/ResonatorBank.h>

using namespace chordify;

namespace
{
    struct Render
    {
        std::vector<float> left, right;
    };

    PartialSettings settingsFor (double sampleRate, int harmonics)
    {
        PartialSettings s;
        s.numHarmonics = harmonics;
        s.detuneCents = 0.0f;
        s.spread = 0.0f;
        s.sampleRate = sampleRate;
        return s;
    }

    Render render (ResonatorBank& bank, const std::vector<float>& input, int blockSize = 512)
    {
        Render out { std::vector<float> (input.size()), std::vector<float> (input.size()) };
        for (size_t start = 0; start < input.size(); start += (size_t) blockSize)
        {
            const auto n = (int) std::min ((size_t) blockSize, input.size() - start);
            bank.process (input.data() + start, out.left.data() + start, out.right.data() + start, n);
        }
        return out;
    }

    std::vector<float> noise (size_t length, float level = 0.25f)
    {
        juce::Random random (99);
        std::vector<float> x (length);
        for (auto& s : x)
            s = level * (random.nextFloat() * 2.0f - 1.0f);
        return x;
    }

    // Largest |second difference|: a smooth resonator output has little of it, a click has a lot
    double maxSecondDifference (const std::vector<float>& y, size_t start, size_t length)
    {
        double peak = 0.0;
        for (auto i = std::max<size_t> (start, 2); i < std::min (y.size(), start + length); ++i)
            peak = std::max (peak, (double) std::abs (y[i] - 2.0f * y[i - 1] + y[i - 2]));
        return peak;
    }
}

TEST_CASE ("Resonator bank adds no latency", "[bank]")
{
    ResonatorBank bank;
    bank.prepare (48000.0);
    bank.setPartials (computePartials (internalChord (60, 0, {}), settingsFor (48000.0, 8)));
    bank.setDecay (1.0f);

    std::vector<float> impulse (256, 0.0f);
    impulse[0] = 1.0f;
    const auto out = render (bank, impulse);

    CHECK (bank.getLatencySamples() == 0);
    // Gate and gains fade in from zero over the first control interval, but the response starts immediately
    CHECK (std::abs (out.left[1]) > 0.0f);
}

TEST_CASE ("Resonator bank is silent without voices", "[bank]")
{
    ResonatorBank bank;
    bank.prepare (48000.0);
    bank.setPartials ({});
    const auto out = render (bank, noise (4800));

    CHECK (test::peakDb (out.left, 0, out.left.size()) < -200.0);
    CHECK (bank.getNumActiveResonators() == 0);
}

TEST_CASE ("Resonator bank level is consistent across sample rates", "[bank]")
{
    // A sine at a partial's frequency comes through at the same level at every rate
    auto levelAt = [] (double sampleRate) {
        ResonatorBank bank;
        bank.prepare (sampleRate);
        Voices voices {};
        voices[0] = { 69.0f, true, true };
        bank.setPartials (computePartials (voices, settingsFor (sampleRate, 1)));
        bank.setDecay (0.5f);

        std::vector<float> sine ((size_t) (2.0 * sampleRate));
        for (size_t i = 0; i < sine.size(); ++i)
            sine[i] = 0.1f * (float) std::sin (2.0 * std::numbers::pi * 440.0 * (double) i / sampleRate);

        const auto out = render (bank, sine);
        return test::rms (out.left, out.left.size() / 2, out.left.size() / 2);
    };

    const auto reference = levelAt (48000.0);
    CHECK (reference > 0.0);
    CHECK (20.0 * std::log10 (levelAt (44100.0) / reference) == Catch::Approx (0.0).margin (0.5));
    CHECK (20.0 * std::log10 (levelAt (96000.0) / reference) == Catch::Approx (0.0).margin (0.5));
}

TEST_CASE ("Released voices ring out instead of cutting off", "[bank]")
{
    constexpr double sampleRate = 48000.0;
    ResonatorBank bank;
    bank.prepare (sampleRate);
    bank.setDecay (1.0f);

    Voices voices {};
    voices[0] = { 57.0f, true, true };
    bank.setPartials (computePartials (voices, settingsFor (sampleRate, 4)));
    const auto held = render (bank, noise ((size_t) sampleRate));
    const auto heldLevel = test::rms (held.left, (size_t) (0.5 * sampleRate), (size_t) (0.5 * sampleRate));

    voices[0].gated = false;
    bank.setPartials (computePartials (voices, settingsFor (sampleRate, 4)));
    const auto tail = render (bank, noise ((size_t) (2.0 * sampleRate))); // input keeps playing

    // 0.1-0.2 s after release the tail has only decayed ~6-12 dB, then keeps falling at 60 dB per T60
    const auto early = test::rms (tail.left, (size_t) (0.1 * sampleRate), (size_t) (0.1 * sampleRate));
    const auto late = test::rms (tail.left, (size_t) (1.1 * sampleRate), (size_t) (0.1 * sampleRate));
    CHECK (20.0 * std::log10 (early / heldLevel) > -15.0);
    CHECK (20.0 * std::log10 (early / late) == Catch::Approx (60.0).margin (6.0));

    // Once silent, the resonators are switched off to save CPU
    CHECK (bank.getNumActiveResonators() == 0);
}

TEST_CASE ("Chord changes are click-free", "[bank]")
{
    constexpr double sampleRate = 48000.0;
    const auto glide = GENERATE (0.0f, 0.05f);
    INFO ("glide " << glide);

    ResonatorBank bank;
    bank.prepare (sampleRate);
    bank.setDecay (1.0f);
    bank.setGlide (glide);

    // C major -> F major (similar register, so steady-state levels match), one harmonic per note
    const auto settings = settingsFor (sampleRate, 1);
    const auto cMajor = internalChord (60, 0, {});
    bank.setPartials (computePartials (cMajor, settings));
    const auto before = render (bank, noise ((size_t) sampleRate));

    bank.setPartials (computePartials (internalChord (65, 0, cMajor), settings));
    const auto after = render (bank, noise ((size_t) (0.5 * sampleRate)), 64);

    const auto steady = maxSecondDifference (before.left, (size_t) (0.5 * sampleRate), (size_t) (0.5 * sampleRate));
    const auto change = maxSecondDifference (after.left, 0, (size_t) (0.1 * sampleRate));
    INFO ("steady " << steady << " change " << change);
    CHECK (change < 2.0 * steady);

    for (auto y : after.left)
        REQUIRE (test::isFinite (y));
}

TEST_CASE ("Resonator bank survives extreme settings", "[bank]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0);
    const auto decay = GENERATE (0.05f, 10.0f);
    INFO ("fs " << sampleRate << " decay " << decay);

    ResonatorBank bank;
    bank.prepare (sampleRate);
    bank.setDecay (decay);

    Voices voices {};
    for (int v = 0; v < maxVoices; ++v)
        voices[(size_t) v] = { (float) (24 + v * 12), true, true }; // C0 .. C7, 8 voices x 16 harmonics
    bank.setPartials (computePartials (voices, settingsFor (sampleRate, maxHarmonics)));

    // Full-scale noise, then a full-scale sine sitting exactly on a partial
    auto input = noise ((size_t) sampleRate, 1.0f);
    for (size_t i = 0; i < (size_t) sampleRate; ++i)
        input.push_back ((float) std::sin (2.0 * std::numbers::pi * midiNoteToHz (60.0) * (double) i / sampleRate));

    const auto out = render (bank, input);
    for (size_t i = 0; i < out.left.size(); ++i)
    {
        REQUIRE (test::isFinite (out.left[i]));
        REQUIRE (std::abs (out.left[i]) <= 1.0f); // soft limiter ceiling
        REQUIRE (std::abs (out.right[i]) <= 1.0f);
    }
}

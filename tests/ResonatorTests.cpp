#include "helpers/dsp_test_helpers.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <dsp/Resonator.h>

using namespace chordify;

namespace
{
    std::vector<float> impulseResponse (double freq, double t60, double sampleRate, double seconds)
    {
        Resonator r;
        r.setCoefficients (makeResonatorCoefficients (freq, t60, sampleRate));

        std::vector<float> out ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = r.process (i == 0 ? 1.0f : 0.0f);
        return out;
    }

    // Steady-state amplitude of the resonator's response to a sine at inputFreq
    float sineGain (double resonatorFreq, double inputFreq, double t60, double sampleRate)
    {
        Resonator r;
        r.setCoefficients (makeResonatorCoefficients (resonatorFreq, t60, sampleRate));

        // Let the transient die away (> 3 T60), then measure the peak over the last 0.2 s
        const auto settle = (int) (std::max (3.0 * t60, 0.5) * sampleRate);
        const auto measure = (int) (0.2 * sampleRate);
        float peak = 0.0f;
        for (int i = 0; i < settle + measure; ++i)
        {
            const auto y = r.process ((float) std::sin (2.0 * std::numbers::pi * inputFreq * i / sampleRate));
            if (i >= settle)
                peak = std::max (peak, std::abs (y));
        }
        return peak;
    }
}

TEST_CASE ("Resonator decay maps to pole radius", "[resonator]")
{
    CHECK (poleRadiusForDecay (1.0, 48000.0) == Catch::Approx (std::pow (0.001, 1.0 / 48000.0)));

    // After T60 * fs samples the envelope R^n is exactly -60 dB
    const auto r = poleRadiusForDecay (2.5, 44100.0);
    CHECK (std::pow (r, 2.5 * 44100.0) == Catch::Approx (0.001));
}

TEST_CASE ("Resonator rings down in T60", "[resonator]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto t60 = GENERATE (0.1, 1.0, 4.0);
    const auto freq = GENERATE (110.0, 1000.0, 8000.0);
    INFO ("fs " << sampleRate << " T60 " << t60 << " f " << freq);

    // Compare RMS over 20 periods starting at t = 0.1 T60 and t = 1.1 T60. The response shrinks by
    // R per sample, so the two windows should be exactly 60 dB apart.
    const auto window = (size_t) (20.0 * sampleRate / freq);
    const auto t1 = (size_t) (0.1 * t60 * sampleRate);
    const auto t2 = (size_t) (1.1 * t60 * sampleRate);
    const auto ir = impulseResponse (freq, t60, sampleRate, (double) (t2 + window) / sampleRate);
    const auto dropDb = 20.0 * std::log10 (test::rms (ir, t1, window) / test::rms (ir, t2, window));

    // Within 5% of the target T60 (60 dB +/- 3 dB)
    CHECK (dropDb == Catch::Approx (60.0).margin (3.0));
}

TEST_CASE ("Resonator rings at its frequency", "[resonator]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0);
    const auto freq = GENERATE (55.0, 261.6256, 440.0, 3520.0);
    INFO ("fs " << sampleRate << " f " << freq);

    const auto ir = impulseResponse (freq, 2.0, sampleRate, 1.0);
    const auto measured = test::zeroCrossingFrequency (ir, sampleRate);

    CHECK (std::abs (1200.0 * std::log2 (measured / freq)) < 1.0);
}

TEST_CASE ("Resonator has unity gain at its frequency", "[resonator]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto t60 = GENERATE (0.05, 0.5, 2.0);
    INFO ("fs " << sampleRate << " T60 " << t60);

    // 0 dB at the resonant frequency, identical across sample rates (within 0.1 dB)
    CHECK (sineGain (440.0, 440.0, t60, sampleRate) == Catch::Approx (1.0f).margin (0.012f));

    // and attenuating away from it
    CHECK (sineGain (440.0, 880.0, t60, sampleRate) < 0.5f);
}

TEST_CASE ("Resonator is stable across its range", "[resonator]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0);
    const auto t60 = GENERATE (0.01, 0.05, 1.0, 10.0);
    const auto freqFraction = GENERATE (0.0, 0.3, 1.0); // 20 Hz .. 0.45 fs, log-spaced
    const auto freq = 20.0 * std::pow (0.45 * sampleRate / 20.0, freqFraction);
    INFO ("fs " << sampleRate << " T60 " << t60 << " f " << freq);

    const auto c = makeResonatorCoefficients (freq, t60, sampleRate);
    CHECK (c.k > 0.0f);

    Resonator r;
    r.setCoefficients (c);
    juce::Random random (7);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < (int) sampleRate; ++i)
    {
        const auto y = r.process (random.nextFloat() * 2.0f - 1.0f);
        finite = finite && test::isFinite (y);
        peak = std::max (peak, std::abs (y));
    }

    CHECK (finite);
    CHECK (peak < 20.0f);
}

#include "helpers/dsp_test_helpers.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <dsp/Exciter.h>

using namespace chordify;

namespace
{
    constexpr double sampleRate = 48000.0;

    std::vector<float> sine (double freq, double seconds, float level = 0.5f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = level * (float) std::sin (2.0 * std::numbers::pi * freq * (double) i / sampleRate);
        return x;
    }

    std::vector<float> excite (float amount, const std::vector<float>& input)
    {
        Exciter exciter;
        exciter.setAmount (amount);
        exciter.prepare (sampleRate);
        std::vector<float> out (input.size());
        exciter.process (input.data(), out.data(), (int) input.size());
        return out;
    }
}

TEST_CASE ("Exciter at 0% passes the input through exactly", "[exciter]")
{
    const auto input = sine (220.0, 0.5);
    const auto out = excite (0.0f, input);
    for (size_t i = 0; i < input.size(); ++i)
        REQUIRE (juce::exactlyEqual (out[i], input[i]));
}

TEST_CASE ("Exciter at 100% replaces a tone with noise at the same level", "[exciter]")
{
    const auto input = sine (220.0, 2.0);
    const auto out = excite (1.0f, input);
    const auto start = (size_t) (0.5 * sampleRate);
    const auto length = (size_t) sampleRate;

    // Level follows the input's peak envelope: within 4 dB of the input RMS
    CHECK (20.0 * std::log10 (test::rms (out, start, length) / test::rms (input, start, length)) == Catch::Approx (0.0).margin (4.0));

    // The tone itself is gone: its component drops by more than 30 dB
    CHECK (test::toneDb (input, start, length, 220.0, sampleRate) - test::toneDb (out, start, length, 220.0, sampleRate) > 30.0);
}

TEST_CASE ("Exciter noise follows the input envelope", "[exciter]")
{
    auto input = sine (220.0, 1.0);
    input.resize ((size_t) (2.0 * sampleRate), 0.0f); // 1 s of tone, then 1 s of silence
    const auto out = excite (1.0f, input);

    // The noise dies away with the input (30 ms release): no noise floor while the input is quiet
    CHECK (test::peakDb (out, (size_t) (1.3 * sampleRate), (size_t) (0.7 * sampleRate)) < -80.0);

    // Transients are kept: noise reaches the input level within a few milliseconds
    CHECK (test::rms (out, (size_t) (0.005 * sampleRate), (size_t) (0.01 * sampleRate)) > 0.5 * test::rms (input, 0, (size_t) (0.01 * sampleRate)));
}

#include "helpers/dsp_test_helpers.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <dsp/ToneFilters.h>

using namespace chordify;

namespace
{
    constexpr double sampleRate = 48000.0;

    std::vector<float> sine (double freq, double seconds = 1.0)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = 0.5f * (float) std::sin (2.0 * std::numbers::pi * freq * (double) i / sampleRate);
        return x;
    }

    // Steady-state gain in dB (second half of the signal)
    double gainDb (const std::vector<float>& in, const std::vector<float>& out)
    {
        const auto half = in.size() / 2;
        return 20.0 * std::log10 (test::rms (out, half, half) / test::rms (in, half, half));
    }

    double highPassGain (double freq, float cutoff)
    {
        InputHighPass hpf;
        hpf.setCutoff (cutoff);
        hpf.prepare (sampleRate);
        const auto in = sine (freq);
        auto out = in;
        hpf.process (out.data(), (int) out.size());
        return gainDb (in, out);
    }

    double tiltGain (double freq, float tone)
    {
        TiltEq eq;
        eq.setTone (tone);
        eq.prepare (sampleRate);
        const auto in = sine (freq);
        auto left = in, right = in;
        eq.process (left.data(), right.data(), (int) in.size());
        return gainDb (in, left);
    }
}

TEST_CASE ("Input high-pass", "[tonefilters]")
{
    CHECK (highPassGain (200.0, 200.0f) == Catch::Approx (-3.01).margin (0.1)); // Butterworth: -3 dB at cutoff
    CHECK (highPassGain (50.0, 200.0f) < -23.0);                                 // two octaves below: -24 dB
    CHECK (highPassGain (2000.0, 200.0f) == Catch::Approx (0.0).margin (0.1));

    // At the default 20 Hz the filter leaves musical content untouched
    CHECK (highPassGain (60.0, 20.0f) == Catch::Approx (0.0).margin (0.2));
}

TEST_CASE ("Tilt EQ", "[tonefilters]")
{
    CHECK (tiltGain (50.0, 0.0f) == Catch::Approx (0.0).margin (0.01));
    CHECK (tiltGain (10000.0, 0.0f) == Catch::Approx (0.0).margin (0.01));

    // Bright: highs up, lows down, approaching +/-6 dB far from the 700 Hz pivot
    CHECK (tiltGain (15000.0, 1.0f) == Catch::Approx (6.0).margin (0.5));
    CHECK (tiltGain (30.0, 1.0f) == Catch::Approx (-6.0).margin (0.5));

    // Dark: the mirror image
    CHECK (tiltGain (15000.0, -1.0f) == Catch::Approx (-6.0).margin (0.5));
    CHECK (tiltGain (30.0, -1.0f) == Catch::Approx (6.0).margin (0.5));
}

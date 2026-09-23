#include "ToneFilters.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace chordify
{
    void InputHighPass::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        cutoffAlpha = (float) -std::expm1 (-controlInterval / (smoothingSeconds * sampleRate));
        reset();
    }

    void InputHighPass::reset()
    {
        cutoff = targetCutoff;
        ic1eq = ic2eq = 0.0f;
        samplesUntilUpdate = 0;
    }

    void InputHighPass::updateCoefficients()
    {
        // Glide the cutoff in the log domain, then recompute (tan is too costly per sample)
        cutoff = std::exp (std::log (cutoff) + (std::log (targetCutoff) - std::log (cutoff)) * cutoffAlpha);

        const auto hz = std::clamp ((double) cutoff, 10.0, 0.45 * sampleRate);
        const auto g = std::tan (std::numbers::pi * hz / sampleRate);
        const auto k = std::numbers::sqrt2; // Q = 1/sqrt(2): maximally flat pass band
        const auto d1 = 1.0 / (1.0 + g * (g + k));
        a1 = (float) d1;
        a2 = (float) (g * d1);
        a3 = (float) (g * g * d1);
    }

    void InputHighPass::process (float* samples, int numSamples)
    {
        constexpr auto k = std::numbers::sqrt2_v<float>;

        for (int i = 0; i < numSamples; ++i)
        {
            if (samplesUntilUpdate-- == 0)
            {
                updateCoefficients();
                samplesUntilUpdate = controlInterval - 1;
            }

            const auto x = samples[i];
            const auto v3 = x - ic2eq;
            const auto v1 = a1 * ic1eq + a2 * v3;
            const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            samples[i] = x - k * v1 - v2;
        }
    }

    //==============================================================================
    void TiltEq::prepare (double sampleRate)
    {
        const auto wc = std::tan (std::numbers::pi * pivotHz / sampleRate);
        g = (float) (wc / (1.0 + wc));
        gainAlpha = (float) -std::expm1 (-1.0 / (smoothingSeconds * sampleRate));
        reset();
    }

    void TiltEq::reset()
    {
        const auto db = std::clamp (targetTone, -1.0f, 1.0f) * maxTiltDb;
        lowGain = std::pow (10.0f, -db / 20.0f);
        highGain = std::pow (10.0f, db / 20.0f);
        stateLeft = stateRight = 0.0f;
    }

    void TiltEq::process (float* left, float* right, int numSamples)
    {
        const auto db = std::clamp (targetTone, -1.0f, 1.0f) * maxTiltDb;
        const auto targetLow = std::pow (10.0f, -db / 20.0f);
        const auto targetHigh = std::pow (10.0f, db / 20.0f);

        const auto split = [this] (float x, float& state, float low, float high) {
            const auto v = (x - state) * g;
            const auto lp = v + state;
            state = lp + v;
            return lp * low + (x - lp) * high;
        };

        for (int i = 0; i < numSamples; ++i)
        {
            lowGain += (targetLow - lowGain) * gainAlpha;
            highGain += (targetHigh - highGain) * gainAlpha;
            left[i] = split (left[i], stateLeft, lowGain, highGain);
            right[i] = split (right[i], stateRight, lowGain, highGain);
        }
    }
}

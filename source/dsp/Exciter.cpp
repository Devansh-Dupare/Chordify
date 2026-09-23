#include "Exciter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace chordify
{
    void Exciter::prepare (double sampleRate)
    {
        // Halve the time constants: the level is sqrt(meanSquare), which moves half as fast
        const auto coeff = [sampleRate] (double seconds) { return (float) -std::expm1 (-2.0 / (seconds * sampleRate)); };
        attackCoeff = coeff (attackSeconds);
        releaseCoeff = coeff (releaseSeconds);
        amountStep = (float) (1.0 / (amountRampSeconds * sampleRate));

        // Measure the pink generator's RMS once, so the noise matches the input level exactly
        reset();
        pinkNorm = 1.0f;
        double sum = 0.0;
        constexpr int calibrationSamples = 1 << 17;
        for (int i = 0; i < calibrationSamples; ++i)
        {
            const auto p = nextPink();
            sum += (double) p * p;
        }
        pinkNorm = (float) (1.0 / std::sqrt (sum / calibrationSamples));

        reset();
    }

    void Exciter::reset()
    {
        meanSquare = 0.0f;
        amount = targetAmount;
        updateGains();

        rng = seed;
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    }

    void Exciter::updateGains()
    {
        const auto angle = std::clamp (amount, 0.0f, 1.0f) * std::numbers::pi_v<float> / 2.0f;
        inputGain = std::cos (angle);
        noiseGain = std::sin (angle);
    }

    float Exciter::nextPink()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        const auto white = (float) rng / 4294967295.0f * 2.0f - 1.0f;

        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        const auto pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;

        return pink * pinkNorm;
    }

    void Exciter::process (const float* in, float* out, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (amount < targetAmount || amount > targetAmount)
            {
                amount += std::clamp (targetAmount - amount, -amountStep, amountStep);
                updateGains();
            }

            const auto x = in[i];

            // Fast-attack, slow-release mean-square envelope, so transients stay sharp
            const auto x2 = x * x;
            meanSquare += (x2 - meanSquare) * (x2 > meanSquare ? attackCoeff : releaseCoeff);

            out[i] = inputGain * x + (noiseGain > 0.0f ? noiseGain * std::sqrt (meanSquare) * nextPink() : 0.0f);
        }
    }
}

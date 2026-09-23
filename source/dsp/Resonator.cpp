#include "Resonator.h"

#include <numbers>

namespace chordify
{
    double poleRadiusForDecay (double t60Seconds, double sampleRate)
    {
        return std::pow (0.001, 1.0 / (t60Seconds * sampleRate));
    }

    ResonatorCoefficients makeResonatorCoefficients (double freqHz, double t60Seconds, double sampleRate)
    {
        const auto g = std::tan (std::numbers::pi * freqHz / sampleRate);

        // R^2 = 0.001^(2 / (T60 fs)). For long decays R is within ~1e-5 of 1, so compute
        // 1 - R^2 with expm1 rather than by subtraction to keep full precision.
        const auto logR2 = 2.0 * std::log (0.001) / (t60Seconds * sampleRate);
        const auto oneMinusR2 = -std::expm1 (logR2);
        const auto onePlusR2 = 2.0 - oneMinusR2;

        const auto k = (1.0 + g * g) * oneMinusR2 / (g * onePlusR2);

        const auto a1 = 1.0 / (1.0 + g * (g + k));
        const auto a2 = g * a1;
        const auto a3 = g * a2;

        return { (float) a1, (float) a2, (float) a3, (float) k };
    }
}

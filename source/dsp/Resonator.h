#pragma once

#include <cmath>

namespace chordify
{
    // Coefficients for a TPT (trapezoidal, zero-delay-feedback) state variable filter,
    // after Andrew Simper's "SvfLinearTrapOptimised2" (Cytomic technical papers).
    struct ResonatorCoefficients
    {
        float a1 = 0.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
        float k = 0.0f; // damping; the band-pass output is scaled by k for unity peak gain
    };

    // Pole radius whose envelope falls by 60 dB in t60Seconds: R = 0.001^(1 / (T60 * fs))
    double poleRadiusForDecay (double t60Seconds, double sampleRate);

    // Tuned so the magnitude peak sits exactly at freqHz with a gain of exactly 1 (0 dB),
    // and the impulse response rings down by 60 dB in exactly t60Seconds, at any sample rate.
    //
    //   g = tan(pi * f / fs)                         (pre-warped cutoff)
    //   R = 0.001^(1 / (T60 * fs))                   (pole radius from decay)
    //   k = (1 + g^2)(1 - R^2) / (g (1 + R^2))       (damping giving that pole radius)
    //
    // The last line comes from the bilinear-transformed denominator
    // (1 + gk + g^2) z^2 + 2(g^2 - 1) z + (1 - gk + g^2), whose pole radius satisfies
    // R^2 = (1 - gk + g^2) / (1 + gk + g^2).
    ResonatorCoefficients makeResonatorCoefficients (double freqHz, double t60Seconds, double sampleRate);

    // A single two-pole resonator. ResonatorBank runs the same maths over many resonators at once;
    // this class exists as the readable reference that the unit tests pin down.
    class Resonator
    {
    public:
        void setCoefficients (const ResonatorCoefficients& c) { coeffs = c; }
        void reset() { ic1eq = ic2eq = 0.0f; }

        float process (float x)
        {
            const auto v3 = x - ic2eq;
            const auto v1 = coeffs.a1 * ic1eq + coeffs.a2 * v3;
            const auto v2 = ic2eq + coeffs.a2 * ic1eq + coeffs.a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            return coeffs.k * v1;
        }

    private:
        ResonatorCoefficients coeffs;
        float ic1eq = 0.0f;
        float ic2eq = 0.0f;
    };
}

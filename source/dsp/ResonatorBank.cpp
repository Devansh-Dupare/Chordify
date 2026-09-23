#include "ResonatorBank.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace chordify
{
    namespace
    {
        // A resonator whose state has fallen below this (about -120 dBFS) is treated as silent
        constexpr float silenceThreshold = 1.0e-6f;

        // One-pole smoothing coefficient for a time constant, evaluated once per control interval
        float smoothingAlpha (double intervalSeconds, double timeConstantSeconds)
        {
            return (float) -std::expm1 (-intervalSeconds / timeConstantSeconds);
        }

        float smoothTowards (float current, float targetValue, float alpha)
        {
            const auto next = current + (targetValue - current) * alpha;
            return std::abs (targetValue - next) < 1.0e-4f ? targetValue : next;
        }

        float softLimit (float y)
        {
            constexpr auto t = ResonatorBank::limiterThreshold;
            const auto magnitude = std::abs (y);
            if (magnitude <= t)
                return y;
            return std::copysign (t + (1.0f - t) * std::tanh ((magnitude - t) / (1.0f - t)), y);
        }
    }

    void ResonatorBank::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void ResonatorBank::reset()
    {
        decay = targetDecay;
        norm = 1.0f;
        firstControl = true;
        samplesUntilControl = 0;

        log2Freq.fill (0.0f);
        amplitude.fill (0.0f);
        gate.fill (0.0f);
        ic1.fill (0.0f);
        ic2.fill (0.0f);
        hasFreq.fill (false);
        wasActive.fill (false);
        numLanes = 0;
    }

    void ResonatorBank::process (const float* input, float* outLeft, float* outRight, int numSamples)
    {
        for (int start = 0; start < numSamples;)
        {
            if (samplesUntilControl == 0)
            {
                updateControl();
                samplesUntilControl = controlInterval;
            }

            const auto length = std::min (numSamples - start, samplesUntilControl);
            renderLanes (input + start, outLeft + start, outRight + start, length);
            samplesUntilControl -= length;
            start += length;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            outLeft[i] = softLimit (outLeft[i]);
            outRight[i] = softLimit (outRight[i]);
        }
    }

    void ResonatorBank::updateControl()
    {
        // Hand the lanes' filter state back to their slots
        for (int j = 0; j < numLanes; ++j)
        {
            const auto s = (size_t) laneSlot[(size_t) j];
            ic1[s] = laneIc1[(size_t) j];
            ic2[s] = laneIc2[(size_t) j];
        }

        const auto interval = controlInterval / sampleRate;
        const auto freqAlpha = smoothingAlpha (interval, std::max (glideSeconds, minGlideSeconds));
        const auto ampAlpha = smoothingAlpha (interval, amplitudeSmoothingSeconds);

        // Decay glides in the log domain, so sweeps from 0.05 s to 10 s sound even
        const auto clampedDecay = std::clamp (targetDecay, 0.01f, 60.0f);
        decay = firstControl ? clampedDecay
                             : std::exp (std::log (decay) + (std::log (clampedDecay) - std::log (decay)) * smoothingAlpha (interval, decaySmoothingSeconds));

        // Normalise by the RMS of the held partial amplitudes (incoherent sum of resonators).
        // While nothing is held, keep the last value so ringing tails don't jump in level.
        float heldPower = 0.0f;
        for (size_t s = 0; s < (size_t) maxPartials; ++s)
            if (target.gate[s] > 0.0f)
                heldPower += target.amplitude[s] * target.amplitude[s];

        if (heldPower > 0.0f)
        {
            const auto targetNorm = 1.0f / std::sqrt (std::max (heldPower, 1.0f));
            norm = firstControl ? targetNorm : smoothTowards (norm, targetNorm, smoothingAlpha (interval, normSmoothingSeconds));
        }

        const auto decayBoost = std::min (std::sqrt (decay / referenceDecaySeconds), maxDecayBoost);
        const auto outputGain = norm * decayBoost * makeupGain;
        const auto maxFreq = 0.49 * sampleRate;

        numLanes = 0;

        for (size_t s = 0; s < (size_t) maxPartials; ++s)
        {
            const auto targetAmp = target.amplitude[s];
            const auto targetGate = target.gate[s];
            const auto silent = std::abs (ic1[s]) + std::abs (ic2[s]) < silenceThreshold;

            // Only follow the target frequency while the slot is in use, so a slot fading out
            // keeps its pitch. A silent slot jumps straight to its new pitch instead of gliding.
            if (targetAmp > 0.0f)
            {
                const auto targetLog2 = (float) std::log2 (std::max ((double) target.freqHz[s], minPartialHz));
                if (! hasFreq[s] || (amplitude[s] <= 0.0f && silent))
                    log2Freq[s] = targetLog2;
                else
                    log2Freq[s] += (targetLog2 - log2Freq[s]) * freqAlpha;
                hasFreq[s] = true;
            }

            amplitude[s] = smoothTowards (amplitude[s], targetAmp, ampAlpha);
            gate[s] = smoothTowards (gate[s], targetGate, ampAlpha);

            const auto active = hasFreq[s] && amplitude[s] > 0.0f && ! (gate[s] <= 0.0f && targetGate <= 0.0f && silent);
            if (! active)
            {
                ic1[s] = ic2[s] = 0.0f;
                wasActive[s] = false;
                continue;
            }

            const auto freq = std::clamp ((double) std::exp2 (log2Freq[s]), minPartialHz, maxFreq);
            const auto c = makeResonatorCoefficients (freq, decay, sampleRate);
            const auto panAngle = (target.pan[s] + 1.0f) * std::numbers::pi_v<float> / 4.0f;
            const auto gain = amplitude[s] * outputGain;
            const LaneValues end { c.a1, c.a2, c.a3, c.k, gate[s], gain * std::cos (panAngle), gain * std::sin (panAngle) };
            const auto start = wasActive[s] ? slotEnd[s] : end;

            const auto j = (size_t) numLanes++;
            constexpr auto steps = (float) controlInterval;
            laneSlot[j] = (int) s;
            laneIc1[j] = ic1[s];
            laneIc2[j] = ic2[s];
            laneA1[j] = start.a1;
            laneA2[j] = start.a2;
            laneA3[j] = start.a3;
            laneK[j] = start.k;
            laneGate[j] = start.gate;
            laneGainL[j] = start.gainLeft;
            laneGainR[j] = start.gainRight;
            incA1[j] = (end.a1 - start.a1) / steps;
            incA2[j] = (end.a2 - start.a2) / steps;
            incA3[j] = (end.a3 - start.a3) / steps;
            incK[j] = (end.k - start.k) / steps;
            incGate[j] = (end.gate - start.gate) / steps;
            incGainL[j] = (end.gainLeft - start.gainLeft) / steps;
            incGainR[j] = (end.gainRight - start.gainRight) / steps;

            slotEnd[s] = end;
            wasActive[s] = true;
        }

        firstControl = false;
    }

    void ResonatorBank::renderLanes (const float* input, float* outLeft, float* outRight, int numSamples)
    {
        const auto lanes = (size_t) numLanes;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto x = input[i];
            float left = 0.0f;
            float right = 0.0f;

            // Same maths as Resonator::process, across every active lane
            for (size_t j = 0; j < lanes; ++j)
            {
                laneA1[j] += incA1[j];
                laneA2[j] += incA2[j];
                laneA3[j] += incA3[j];
                laneK[j] += incK[j];
                laneGate[j] += incGate[j];
                laneGainL[j] += incGainL[j];
                laneGainR[j] += incGainR[j];

                const auto v3 = laneGate[j] * x - laneIc2[j];
                const auto v1 = laneA1[j] * laneIc1[j] + laneA2[j] * v3;
                const auto v2 = laneIc2[j] + laneA2[j] * laneIc1[j] + laneA3[j] * v3;
                laneIc1[j] = 2.0f * v1 - laneIc1[j];
                laneIc2[j] = 2.0f * v2 - laneIc2[j];

                const auto y = laneK[j] * v1;
                left += y * laneGainL[j];
                right += y * laneGainR[j];
            }

            outLeft[i] = left;
            outRight[i] = right;
        }
    }
}

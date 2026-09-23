#pragma once

#include "Engine.h"
#include "Resonator.h"

namespace chordify
{
    // Up to maxPartials Resonators in parallel, one per PartialGrid slot, summed to stereo.
    //
    // Every controlInterval samples the bank glides each slot's frequency towards its target
    // (exponentially, in log-frequency), smooths amplitudes and gates, recomputes coefficients,
    // and then linearly interpolates every coefficient and gain across the next interval.
    // Active slots are packed into contiguous lanes so the per-sample loop can auto-vectorise.
    class ResonatorBank final : public Engine
    {
    public:
        static constexpr int controlInterval = 32;
        static constexpr float minGlideSeconds = 0.005f;         // floor, so chord changes never click
        static constexpr float amplitudeSmoothingSeconds = 0.005f;
        static constexpr float decaySmoothingSeconds = 0.02f;
        static constexpr float normSmoothingSeconds = 0.05f;

        // Output is scaled by sqrt(T60 / referenceDecay), capped at maxDecayBoost, so broadband
        // input keeps a similar level as Decay narrows the resonances (noise power ~ 1 / T60)
        static constexpr float referenceDecaySeconds = 1.0f;
        static constexpr float maxDecayBoost = 4.0f; // +12 dB

        // Each resonator passes only a few Hz of a broadband input, so the wet signal needs fixed
        // makeup gain to sit near the input level. Calibrated with the Render harness: pink noise
        // at default settings (Excite 100%) comes out at roughly the same RMS as it goes in.
        static constexpr float makeupGain = 22.0f; // +27 dB

        // The wet signal is soft-limited above this level (-1 dBFS), approaching 1.0 asymptotically
        static constexpr float limiterThreshold = 0.891f;

        void prepare (double sampleRate) override;
        void reset() override;

        void setPartials (const PartialGrid& grid) override { target = grid; }
        void setDecay (float t60Seconds) override { targetDecay = t60Seconds; }
        void setGlide (float seconds) override { glideSeconds = seconds; }

        void process (const float* input, float* outLeft, float* outRight, int numSamples) override;

        int getLatencySamples() const override { return 0; }

        int getNumActiveResonators() const { return numLanes; }

    private:
        void updateControl();
        void renderLanes (const float* input, float* outLeft, float* outRight, int numSamples);

        using SlotArray = std::array<float, maxPartials>;

        double sampleRate = 48000.0;
        PartialGrid target;
        float targetDecay = 1.0f;
        float glideSeconds = 0.0f;

        // Smoothed control state
        float decay = 1.0f;
        float norm = 1.0f;
        bool firstControl = true;
        int samplesUntilControl = 0;

        // Per slot
        SlotArray log2Freq {}, amplitude {}, gate {};
        SlotArray ic1 {}, ic2 {};
        std::array<bool, maxPartials> hasFreq {};

        // Values each slot's lane reaches at the end of the current interval
        struct LaneValues
        {
            float a1, a2, a3, k, gate, gainLeft, gainRight;
        };
        std::array<LaneValues, maxPartials> slotEnd {};
        std::array<bool, maxPartials> wasActive {};

        // Packed active lanes: current value + per-sample increment
        int numLanes = 0;
        std::array<int, maxPartials> laneSlot {};
        SlotArray laneIc1 {}, laneIc2 {};
        SlotArray laneA1 {}, laneA2 {}, laneA3 {}, laneK {}, laneGate {}, laneGainL {}, laneGainR {};
        SlotArray incA1 {}, incA2 {}, incA3 {}, incK {}, incGate {}, incGainL {}, incGainR {};
    };
}

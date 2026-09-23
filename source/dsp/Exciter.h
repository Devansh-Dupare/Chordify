#pragma once

#include <cstdint>

namespace chordify
{
    // Crossfades the resonator excitation from the raw input to pink noise that follows the
    // input's level.
    //
    // Resonators only ring where the input has energy. A pitched input (voice, synth, bass) only
    // has energy at its own harmonics, so it only excites the chord partials that happen to line
    // up with them and the chord collapses to a single note. Noise has energy at every frequency,
    // so every partial rings, while the envelope keeps the input's rhythm and dynamics.
    class Exciter
    {
    public:
        // Time constants of the level envelope (the mean-square follower runs twice as fast)
        static constexpr float attackSeconds = 0.001f;
        static constexpr float releaseSeconds = 0.03f;
        static constexpr float amountRampSeconds = 0.02f;

        void prepare (double sampleRate);
        void reset();

        // 0 = raw input, 1 = envelope-following noise; equal-power crossfade in between
        void setAmount (float newAmount) { targetAmount = newAmount; }

        // in and out may point to the same buffer
        void process (const float* in, float* out, int numSamples);

    private:
        float nextPink();
        void updateGains();

        float attackCoeff = 1.0f;
        float releaseCoeff = 1.0f;
        float amountStep = 1.0f;

        float meanSquare = 0.0f;
        float amount = 0.0f;
        float targetAmount = 0.0f;
        float inputGain = 1.0f;
        float noiseGain = 0.0f;

        // Pink noise: Paul Kellett's refined filter on xorshift white noise, scaled to unit RMS
        static constexpr std::uint32_t seed = 0x2545f491u;
        std::uint32_t rng = seed;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float pinkNorm = 1.0f;
    };
}

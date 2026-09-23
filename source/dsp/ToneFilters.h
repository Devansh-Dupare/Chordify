#pragma once

namespace chordify
{
    // 12 dB/octave Butterworth high-pass (TPT state variable filter). Keeps low-end rumble out of
    // the resonator input, so bass energy doesn't swamp the low chord partials.
    class InputHighPass
    {
    public:
        static constexpr int controlInterval = 32;
        static constexpr float smoothingSeconds = 0.02f;

        void prepare (double sampleRate);
        void reset();
        void setCutoff (float hz) { targetCutoff = hz; }

        void process (float* samples, int numSamples);

    private:
        void updateCoefficients();

        double sampleRate = 48000.0;
        float targetCutoff = 20.0f;
        float cutoff = 20.0f;
        float cutoffAlpha = 1.0f;
        int samplesUntilUpdate = 0;

        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float ic1eq = 0.0f, ic2eq = 0.0f;
    };

    // Stereo tilt EQ around a fixed pivot: tone -1 tilts dark (lows up, highs down by up to
    // maxTiltDb each), +1 tilts bright, 0 is exactly flat. Built from a one-pole TPT low-pass,
    // whose low and high parts (lp and x - lp) sum back to the input.
    class TiltEq
    {
    public:
        static constexpr float pivotHz = 700.0f;
        static constexpr float maxTiltDb = 6.0f;
        static constexpr float smoothingSeconds = 0.02f;

        void prepare (double sampleRate);
        void reset();
        void setTone (float tone) { targetTone = tone; }

        void process (float* left, float* right, int numSamples);

    private:
        float g = 0.0f;
        float gainAlpha = 1.0f;
        float targetTone = 0.0f;
        float lowGain = 1.0f, highGain = 1.0f;
        float stateLeft = 0.0f, stateRight = 0.0f;
    };
}

#pragma once

#include "ChordMapper.h"

namespace chordify
{
    // Common interface for the sound-to-chord engines (resonator bank now, FFT spectral in Phase 4).
    // All methods are called on the audio thread and must be realtime safe, except prepare().
    class Engine
    {
    public:
        virtual ~Engine() = default;

        virtual void prepare (double sampleRate) = 0;
        virtual void reset() = 0;

        virtual void setPartials (const PartialGrid&) = 0;
        virtual void setDecay (float t60Seconds) = 0;
        virtual void setGlide (float seconds) = 0;

        // Mono input, stereo wet output. Buffers must not alias.
        virtual void process (const float* input, float* outLeft, float* outRight, int numSamples) = 0;

        virtual int getLatencySamples() const = 0;
    };
}

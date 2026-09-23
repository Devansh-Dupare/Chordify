#pragma once

#include <juce_core/juce_core.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace test
{
    // Bit-level check, so it keeps working when -ffast-math assumes NaN/inf never happen
    inline bool isFinite (float x)
    {
        return (std::bit_cast<std::uint32_t> (x) & 0x7f800000u) != 0x7f800000u;
    }

    // Peak level in dB of a window of samples
    inline double peakDb (std::span<const float> signal, size_t start, size_t length)
    {
        float peak = 0.0f;
        for (auto i = start; i < std::min (signal.size(), start + length); ++i)
            peak = std::max (peak, std::abs (signal[i]));
        return 20.0 * std::log10 (std::max ((double) peak, 1.0e-12));
    }

    inline double rms (std::span<const float> signal, size_t start, size_t length)
    {
        double sum = 0.0;
        const auto end = std::min (signal.size(), start + length);
        for (auto i = start; i < end; ++i)
            sum += (double) signal[i] * signal[i];
        return std::sqrt (sum / (double) std::max<size_t> (1, end - start));
    }

    // Frequency from linearly-interpolated upward zero crossings
    inline double zeroCrossingFrequency (std::span<const float> signal, double sampleRate)
    {
        double first = -1.0;
        double last = -1.0;
        int crossings = 0;

        for (size_t i = 1; i < signal.size(); ++i)
        {
            if (signal[i - 1] < 0.0f && signal[i] >= 0.0f)
            {
                const auto t = (double) (i - 1) + signal[i - 1] / (signal[i - 1] - signal[i]);
                if (first < 0.0)
                    first = t;
                last = t;
                ++crossings;
            }
        }

        return crossings < 2 ? 0.0 : (crossings - 1) * sampleRate / (last - first);
    }
}

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets, exposed to hosts as programs. Values are in real-world units (as shown in
// the UI); any parameter a preset doesn't list is reset to its default.
namespace params
{
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    const std::vector<Preset>& factoryPresets();

    // Call on the message thread
    void applyPreset (juce::AudioProcessorValueTreeState&, const Preset&);
}

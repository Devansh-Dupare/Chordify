#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets, exposed to hosts as programs. They set the sound only: values are in real-world
// units (as shown in the UI), any sound parameter a preset doesn't list is reset to its default, and
// the chord (Chord Slot, the slots and the piano selection) is left alone.
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

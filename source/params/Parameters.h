#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs and layout for the AudioProcessorValueTreeState.
// IDs are saved in host sessions and presets, so never rename one once it has shipped.
namespace params
{
    namespace id
    {
        inline constexpr auto chordSlot = "chordSlot";
        inline constexpr auto harmonics = "harmonics";
        inline constexpr auto detune = "detune";
        inline constexpr auto spread = "spread";
        inline constexpr auto decay = "decay";
        inline constexpr auto brightness = "brightness";
        inline constexpr auto timbre = "timbre";
        inline constexpr auto glide = "glide";
        inline constexpr auto excite = "excite";
        inline constexpr auto mix = "mix";
        inline constexpr auto inputHpf = "inputHpf";
        inline constexpr auto tone = "tone";
        inline constexpr auto output = "output";
    }

    // Chord Slot: 0 plays the notes selected on the piano, 1..numSlots play a saved slot
    inline constexpr int numSlots = 8;
    inline constexpr int pianoSlot = 0;

    // Choice lists; the order is part of the saved state, so only ever append
    inline const juce::StringArray timbreNames { "All Harmonics", "Odd Harmonics" };
    inline const juce::StringArray chordSlotNames { "Piano", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6", "Slot 7", "Slot 8" };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}

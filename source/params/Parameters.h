#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs and layout for the AudioProcessorValueTreeState.
// IDs are saved in host sessions and presets, so never rename one once it has shipped.
namespace params
{
    namespace id
    {
        inline constexpr auto engine = "engine";
        inline constexpr auto chordSource = "chordSource";
        inline constexpr auto root = "root";
        inline constexpr auto chordType = "chordType";
        inline constexpr auto harmonics = "harmonics";
        inline constexpr auto detune = "detune";
        inline constexpr auto spread = "spread";
        inline constexpr auto decay = "decay";
        inline constexpr auto brightness = "brightness";
        inline constexpr auto timbre = "timbre";
        inline constexpr auto glide = "glide";
        inline constexpr auto mix = "mix";
        inline constexpr auto inputHpf = "inputHpf";
        inline constexpr auto tone = "tone";
    }

    enum class Engine
    {
        resonator,
        spectral
    };

    enum class ChordSource
    {
        internal, // root + chordType parameters
        midi      // notes held in the host's MIDI track
    };

    // Choice lists; the order is part of the saved state, so only ever append
    inline const juce::StringArray engineNames { "Resonator", "Spectral" };
    inline const juce::StringArray chordSourceNames { "Internal", "MIDI" };
    inline const juce::StringArray timbreNames { "All Harmonics", "Odd Harmonics" };
    inline const juce::StringArray chordTypeNames { "Major", "Minor", "Diminished", "Augmented", "Sus2", "Sus4", "Major 7", "Minor 7", "Dominant 7", "Power" };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}

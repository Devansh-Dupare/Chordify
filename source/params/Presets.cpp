#include "Presets.h"
#include "Parameters.h"

namespace params
{
    const std::vector<Preset>& factoryPresets()
    {
        // Roots are MIDI notes (60 = C3). Chord types index chordTypeNames, chord sources chordSourceNames.
        static const std::vector<Preset> presets {
            { "Default", {} },
            { "Glass Pad",
                { { id::root, 60 }, { id::chordType, 6 }, { id::harmonics, 6 }, { id::brightness, 70 }, { id::decay, 6 },
                    { id::detune, 8 }, { id::spread, 80 }, { id::glide, 200 }, { id::tone, 20 } } },
            { "Drum Plucks", // keeps some of the drums' own colour, so kicks and hats ring differently
                { { id::root, 45 }, { id::chordType, 1 }, { id::harmonics, 10 }, { id::brightness, 60 }, { id::decay, 0.35f },
                    { id::excite, 40 }, { id::inputHpf, 60 }, { id::mix, 80 } } },
            { "Vocal Choir",
                { { id::root, 57 }, { id::chordType, 7 }, { id::harmonics, 5 }, { id::brightness, 40 }, { id::decay, 2.5f },
                    { id::detune, 12 }, { id::spread, 100 }, { id::inputHpf, 150 }, { id::tone, -25 } } },
            { "Hollow Square",
                { { id::root, 55 }, { id::chordType, 5 }, { id::timbre, 1 }, { id::harmonics, 8 }, { id::brightness, 70 }, { id::decay, 1.5f } } },
            { "Dark Drone",
                { { id::root, 36 }, { id::chordType, 9 }, { id::harmonics, 16 }, { id::brightness, 25 }, { id::decay, 10 },
                    { id::spread, 60 }, { id::tone, -40 } } },
            { "Bright Bells",
                { { id::root, 72 }, { id::chordType, 0 }, { id::harmonics, 4 }, { id::brightness, 90 }, { id::decay, 3 }, { id::detune, 3 } } },
            { "MIDI Root Chords", // play single keys; Chord Type builds the chord
                { { id::chordSource, 2 }, { id::chordType, 7 }, { id::decay, 2 }, { id::glide, 60 } } },
            { "MIDI Voicings", // play your own voicings, one resonator group per key
                { { id::chordSource, 1 }, { id::decay, 1.5f } } },
        };
        return presets;
    }

    void applyPreset (juce::AudioProcessorValueTreeState& tree, const Preset& preset)
    {
        for (auto* p : tree.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

        for (const auto& [paramId, value] : preset.values)
        {
            auto* param = tree.getParameter (paramId);
            jassert (param != nullptr); // unknown ID in a preset
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (value));
        }
    }
}

#include "Presets.h"
#include "Parameters.h"

namespace params
{
    const std::vector<Preset>& factoryPresets()
    {
        // Sound only: presets never touch the chord slots or the piano selection
        static const std::vector<Preset> presets {
            { "Default", {} },
            { "Glass Pad",
                { { id::harmonics, 6 }, { id::brightness, 70 }, { id::decay, 6 },
                    { id::detune, 8 }, { id::spread, 80 }, { id::glide, 200 }, { id::tone, 20 } } },
            { "Drum Plucks", // keeps some of the drums' own colour, so kicks and hats ring differently
                { { id::harmonics, 10 }, { id::brightness, 60 }, { id::decay, 0.35f },
                    { id::excite, 40 }, { id::inputHpf, 60 }, { id::mix, 80 } } },
            { "Vocal Choir",
                { { id::harmonics, 5 }, { id::brightness, 40 }, { id::decay, 2.5f },
                    { id::detune, 12 }, { id::spread, 100 }, { id::inputHpf, 150 }, { id::tone, -25 } } },
            { "Hollow Square",
                { { id::timbre, 1 }, { id::harmonics, 8 }, { id::brightness, 70 }, { id::decay, 1.5f } } },
            { "Dark Drone",
                { { id::harmonics, 16 }, { id::brightness, 25 }, { id::decay, 10 },
                    { id::spread, 60 }, { id::tone, -40 } } },
            { "Bright Bells",
                { { id::harmonics, 4 }, { id::brightness, 90 }, { id::decay, 3 }, { id::detune, 3 } } },
        };
        return presets;
    }

    void applyPreset (juce::AudioProcessorValueTreeState& tree, const Preset& preset)
    {
        for (auto* p : tree.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() != id::chordSlot)
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

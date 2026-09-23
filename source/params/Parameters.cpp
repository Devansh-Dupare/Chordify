#include "Parameters.h"

namespace params
{
    namespace
    {
        // Every parameter is version 1; bump the version hint for parameters added in later releases
        juce::ParameterID pid (const char* paramId)
        {
            return { paramId, 1 };
        }

        juce::NormalisableRange<float> skewedRange (float min, float max, float centre, float interval = 0.0f)
        {
            juce::NormalisableRange<float> range { min, max, interval };
            range.setSkewForCentre (centre);
            return range;
        }

        juce::AudioParameterFloatAttributes withUnit (const juce::String& unit, int decimals)
        {
            return juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([decimals] (float value, int) { return juce::String (value, decimals); });
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Chord: which chord plays; automate it to sequence slots
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (id::chordSlot), "Chord Slot", chordSlotNames, pianoSlot));

        // Voicing
        layout.add (std::make_unique<juce::AudioParameterInt> (pid (id::harmonics), "Harmonics", 1, 16, 8));
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::detune), "Detune",
            juce::NormalisableRange<float> { 0.0f, 50.0f }, 5.0f, withUnit ("ct", 1)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::spread), "Spread",
            juce::NormalisableRange<float> { 0.0f, 100.0f }, 50.0f, withUnit ("%", 0)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::glide), "Glide",
            skewedRange (0.0f, 1000.0f, 150.0f), 30.0f, withUnit ("ms", 0)));

        // Resonance
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::decay), "Decay",
            skewedRange (0.05f, 10.0f, 1.0f), 1.5f, withUnit ("s", 2)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::excite), "Excite",
            juce::NormalisableRange<float> { 0.0f, 100.0f }, 100.0f, withUnit ("%", 0)));

        // Timbre
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::brightness), "Brightness",
            juce::NormalisableRange<float> { 0.0f, 100.0f }, 50.0f, withUnit ("%", 0)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (id::timbre), "Timbre", timbreNames, 0));

        // Output
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::mix), "Mix",
            juce::NormalisableRange<float> { 0.0f, 100.0f }, 100.0f, withUnit ("%", 0)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::inputHpf), "Input HPF",
            skewedRange (20.0f, 1000.0f, 150.0f), 20.0f, withUnit ("Hz", 0)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::tone), "Tone",
            juce::NormalisableRange<float> { -100.0f, 100.0f }, 0.0f, withUnit ("%", 0)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id::output), "Output",
            juce::NormalisableRange<float> { -24.0f, 12.0f }, 0.0f, withUnit ("dB", 1)));

        return layout;
    }
}

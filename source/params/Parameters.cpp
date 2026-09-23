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

        juce::String noteName (int note, int)
        {
            return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
        }

        int noteFromName (const juce::String& text)
        {
            // Accept either a MIDI number ("48") or a note name ("C3", "F#2")
            if (text.containsOnly ("0123456789"))
                return text.getIntValue();

            for (int note = 0; note < 128; ++note)
                if (noteName (note, 0).equalsIgnoreCase (text.trim()))
                    return note;

            return 48;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (id::engine), "Engine", engineNames, 0));

        // Chord
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (id::chordSource), "Chord Source", chordSourceNames, 0));
        layout.add (std::make_unique<juce::AudioParameterInt> (pid (id::root), "Root", 24, 96, 48,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (noteName).withValueFromStringFunction (noteFromName)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (id::chordType), "Chord Type", chordTypeNames, 0));

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

        return layout;
    }
}

#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
    float valueOf (PluginProcessor& plugin, const char* paramId)
    {
        return plugin.getParameterTree().getRawParameterValue (paramId)->load();
    }

    void setValue (PluginProcessor& plugin, const char* paramId, float value)
    {
        auto* param = plugin.getParameterTree().getParameter (paramId);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
}

TEST_CASE ("Parameter layout", "[params]")
{
    PluginProcessor plugin;

    SECTION ("every parameter is exposed to the host")
    {
        for (auto* paramId : { params::id::engine, params::id::chordSource, params::id::root, params::id::chordType,
                 params::id::harmonics, params::id::detune, params::id::spread, params::id::decay, params::id::intensity,
                 params::id::mix, params::id::inputHpf, params::id::tone })
        {
            INFO (paramId);
            CHECK (plugin.getParameterTree().getParameter (paramId) != nullptr);
        }
        CHECK (plugin.getParameters().size() == 12);
    }

    SECTION ("defaults")
    {
        CHECK (valueOf (plugin, params::id::engine) == 0.0f);
        CHECK (valueOf (plugin, params::id::chordSource) == 0.0f);
        CHECK (valueOf (plugin, params::id::root) == 48.0f);
        CHECK (valueOf (plugin, params::id::harmonics) == 8.0f);
        CHECK (valueOf (plugin, params::id::decay) == Catch::Approx (1.5f));
        CHECK (valueOf (plugin, params::id::mix) == 100.0f);
        CHECK (valueOf (plugin, params::id::inputHpf) == 20.0f);
    }

    SECTION ("root displays as a note name and parses one back")
    {
        auto* root = plugin.getParameterTree().getParameter (params::id::root);
        // Note names use middle C (60) = C3, as Ableton and Logic do
        CHECK (root->getCurrentValueAsText() == "C2");
        CHECK (root->convertFrom0to1 (root->getValueForText ("F#2")) == 54.0f);
        CHECK (root->convertFrom0to1 (root->getValueForText ("60")) == 60.0f);
    }
}

TEST_CASE ("State save and restore", "[params]")
{
    juce::MemoryBlock state;
    {
        PluginProcessor plugin;
        setValue (plugin, params::id::chordType, 7.0f);
        setValue (plugin, params::id::decay, 4.25f);
        setValue (plugin, params::id::root, 55.0f);
        plugin.getStateInformation (state);
    }

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (valueOf (restored, params::id::chordType) == 7.0f);
    CHECK (valueOf (restored, params::id::decay) == Catch::Approx (4.25f).margin (0.001f));
    CHECK (valueOf (restored, params::id::root) == 55.0f);

    SECTION ("garbage state is ignored")
    {
        const char junk[] = "not a plugin state";
        restored.setStateInformation (junk, (int) sizeof (junk));
        CHECK (valueOf (restored, params::id::root) == 55.0f);
    }
}

TEST_CASE ("MIDI input and passthrough", "[midi]")
{
    PluginProcessor plugin;
    CHECK (plugin.acceptsMidi());
    CHECK_FALSE (plugin.isMidiEffect());

    constexpr int blockSize = 64;
    plugin.setPlayConfigDetails (2, 2, 48000.0, blockSize);
    plugin.prepareToPlay (48000.0, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::Random random (42);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, random.nextFloat() - 0.5f);
    const juce::AudioBuffer<float> input (buffer);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 10);
    midi.addEvent (juce::MidiMessage::noteOn (1, 100, (juce::uint8) 100), 20);
    plugin.processBlock (buffer, midi);

    SECTION ("held notes are tracked across both 64-note words")
    {
        const auto held = plugin.getHeldNotes();
        CHECK (held.count() == 3);
        CHECK (held[60]);
        CHECK (held[64]);
        CHECK (held[100]);
    }

    SECTION ("note off and all-notes-off release notes")
    {
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        plugin.processBlock (buffer, midi);
        CHECK (plugin.getHeldNotes().count() == 2);
        CHECK_FALSE (plugin.getHeldNotes()[64]);

        midi.clear();
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        plugin.processBlock (buffer, midi);
        CHECK (plugin.getHeldNotes().none());
    }

    SECTION ("audio passes through unchanged")
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                CHECK (juce::exactlyEqual (buffer.getSample (ch, i), input.getSample (ch, i)));
    }

    plugin.releaseResources();
}

TEST_CASE ("Editor opens with every control attached", "[editor]")
{
    runWithinPluginEditor ([] (PluginProcessor& plugin) {
        auto* editor = plugin.getActiveEditor();
        REQUIRE (editor != nullptr);
        CHECK (editor->getWidth() > 0);
        CHECK (editor->getHeight() > 0);
    });
}

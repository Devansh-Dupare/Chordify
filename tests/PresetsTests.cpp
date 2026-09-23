#include <PluginProcessor.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Factory presets", "[presets]")
{
    PluginProcessor plugin;
    auto& tree = plugin.getParameterTree();
    const auto& presets = params::factoryPresets();

    CHECK (plugin.getNumPrograms() == (int) presets.size());
    CHECK (plugin.getNumPrograms() == 7);
    CHECK (plugin.getProgramName (0) == "Default");

    SECTION ("every preset names real parameters with in-range values")
    {
        for (int i = 0; i < plugin.getNumPrograms(); ++i)
        {
            INFO (presets[(size_t) i].name);
            CHECK (plugin.getProgramName (i).isNotEmpty());

            plugin.setCurrentProgram (i);
            CHECK (plugin.getCurrentProgram() == i);

            for (const auto& [paramId, value] : presets[(size_t) i].values)
            {
                INFO (paramId);
                auto* param = tree.getParameter (paramId);
                REQUIRE (param != nullptr);
                const auto range = tree.getParameterRange (paramId);
                CHECK (value >= range.start);
                CHECK (value <= range.end);
                CHECK (tree.getRawParameterValue (paramId)->load() == Catch::Approx (value).margin (0.01));
            }
        }
    }

    SECTION ("loading a preset resets parameters it doesn't mention")
    {
        plugin.setCurrentProgram (5); // Dark Drone: decay 10
        CHECK (tree.getRawParameterValue (params::id::decay)->load() == Catch::Approx (10.0f));
        plugin.setCurrentProgram (0);
        CHECK (tree.getRawParameterValue (params::id::decay)->load() == Catch::Approx (1.5f));
    }

    SECTION ("presets change the sound, never the chord")
    {
        ChordSlots::Notes notes;
        notes[62] = notes[65] = notes[69] = true;
        plugin.getChordSlots().setPiano (notes);
        plugin.getChordSlots().setSlot (0, notes);
        tree.getParameter (params::id::chordSlot)->setValueNotifyingHost (tree.getParameter (params::id::chordSlot)->convertTo0to1 (1.0f));

        plugin.setCurrentProgram (1);
        CHECK (plugin.getChordSlots().getPiano() == notes);
        CHECK (plugin.getChordSlots().getSlot (0) == notes);
        CHECK (juce::roundToInt (tree.getRawParameterValue (params::id::chordSlot)->load()) == 1);
    }

    SECTION ("the chosen preset survives save and restore")
    {
        plugin.setCurrentProgram (3);
        juce::MemoryBlock state;
        plugin.getStateInformation (state);

        PluginProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (restored.getCurrentProgram() == 3);
    }
}

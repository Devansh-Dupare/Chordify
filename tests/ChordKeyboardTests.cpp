#include "helpers/dsp_test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <ui/ChordKeyboard.h>

namespace
{
    void setValue (PluginProcessor& plugin, const char* paramId, float value)
    {
        auto* param = plugin.getParameterTree().getParameter (paramId);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    int activeSlot (PluginProcessor& plugin)
    {
        return juce::roundToInt (plugin.getParameterTree().getRawParameterValue (params::id::chordSlot)->load());
    }

    ChordSlots::Notes chord (std::initializer_list<int> notes)
    {
        ChordSlots::Notes result;
        for (auto n : notes)
            result[(size_t) n] = true;
        return result;
    }

    // Renders noise through the plugin in 256-sample blocks without re-preparing it
    std::vector<float> render (PluginProcessor& plugin, size_t numSamples)
    {
        constexpr int blockSize = 256;
        juce::Random random (5);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (size_t start = 0; start < numSamples; start += blockSize)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, 0.2f * (random.nextFloat() * 2.0f - 1.0f));
            plugin.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        return out;
    }

    void prepare (PluginProcessor& plugin)
    {
        plugin.setPlayConfigDetails (2, 2, 48000.0, 256);
        plugin.prepareToPlay (48000.0, 256);
    }
}

TEST_CASE ("Chord slot storage", "[slots]")
{
    SECTION ("a chord packs into one word and back")
    {
        const auto notes = chord ({ 0, 36, 60, 64, 67, 127 });
        CHECK (ChordSlots::unpack (ChordSlots::pack (notes)) == notes);
        CHECK (ChordSlots::unpack (ChordSlots::pack ({})).none());
    }

    SECTION ("more than eight notes keeps the lowest eight")
    {
        const auto packed = ChordSlots::unpack (ChordSlots::pack (chord ({ 40, 41, 42, 43, 44, 45, 46, 47, 48, 49 })));
        CHECK (packed == chord ({ 40, 41, 42, 43, 44, 45, 46, 47 }));
    }

    SECTION ("text round trip ignores junk")
    {
        CHECK (ChordSlots::fromString (ChordSlots::toString (chord ({ 48, 52, 55 }))) == chord ({ 48, 52, 55 }));
        CHECK (ChordSlots::fromString ("60 x -3 200 64") == chord ({ 60, 64 }));
        CHECK (ChordSlots::noteNames (chord ({ 48, 52, 55 })) == "C2 E2 G2");
    }

    SECTION ("new instances start with C2 E2 G2 on the piano and empty slots")
    {
        ChordSlots slots;
        CHECK (slots.getPiano() == chord ({ 48, 52, 55 }));
        for (int i = 0; i < ChordSlots::numSlots; ++i)
            CHECK (slots.getSlot (i).none());
        CHECK (slots.notesFor (params::pianoSlot) == slots.getPiano());
    }
}

TEST_CASE ("Picking notes on the keyboard", "[keyboard]")
{
    PluginProcessor plugin;
    auto& slots = plugin.getChordSlots();
    ui::ChordKeyboard keyboard (plugin.getParameterTree(), slots);

    SECTION ("clicking toggles notes in the piano selection")
    {
        keyboard.toggleNote (59); // add B2
        keyboard.toggleNote (52); // remove E2
        CHECK (slots.getPiano() == chord ({ 48, 55, 59 }));
        CHECK (keyboard.getShownNotes() == chord ({ 48, 55, 59 }));
    }

    SECTION ("a chord holds at most eight notes")
    {
        keyboard.clearNotes();
        bool limitHit = false;
        keyboard.onNoteLimit = [&] { limitHit = true; };
        for (int note = 60; note < 68; ++note)
            CHECK (keyboard.toggleNote (note));
        CHECK_FALSE (keyboard.toggleNote (70));
        CHECK (limitHit);
        CHECK (slots.getPiano().count() == 8);
    }

    SECTION ("clear removes every note")
    {
        keyboard.clearNotes();
        CHECK (slots.getPiano().none());
    }

    SECTION ("editing while a slot plays switches to the piano and leaves the slot alone")
    {
        slots.setSlot (0, chord ({ 57, 60, 64 }));
        setValue (plugin, params::id::chordSlot, 1.0f);
        CHECK (keyboard.getShownNotes() == chord ({ 57, 60, 64 })); // the slot's chord is shown

        keyboard.toggleNote (67);
        CHECK (activeSlot (plugin) == params::pianoSlot);
        CHECK (slots.getPiano() == chord ({ 57, 60, 64, 67 })); // edited from the slot's chord
        CHECK (slots.getSlot (0) == chord ({ 57, 60, 64 }));     // slot unchanged until saved over
    }

    SECTION ("clicking keys in the upper octaves doesn't scroll the keyboard")
    {
        REQUIRE (keyboard.getLowestNote() == 48); // showing C2 - B4
        keyboard.toggleNote (83);                 // B4, the last key
        CHECK (keyboard.getLowestNote() == 48);
    }

    SECTION ("the view follows a slot chord that would be off screen")
    {
        slots.setSlot (1, chord ({ 88, 91, 95 })); // E5 G5 B5
        setValue (plugin, params::id::chordSlot, 2.0f);
        CHECK (keyboard.getLowestNote() == 84);
    }

    SECTION ("the view scrolls down to a slot chord's lowest note")
    {
        slots.setSlot (1, chord ({ 45, 52, 57, 60 })); // A1 E2 A2 C3: A1 is below C2
        setValue (plugin, params::id::chordSlot, 2.0f);
        CHECK (keyboard.getLowestNote() == 36);
    }

    SECTION ("octave buttons scroll the view without touching the chord")
    {
        keyboard.shiftOctave (-1);
        CHECK (keyboard.getLowestNote() == 36);
        CHECK (slots.getPiano() == chord ({ 48, 52, 55 }));
    }

    SECTION ("keys map to the right notes on screen")
    {
        keyboard.setBounds (0, 0, 21 * 30, 100); // three octaves of 30 px white keys, starting at C2
        CHECK (keyboard.noteAt ({ 5.0f, 90.0f }) == 48);   // C2
        CHECK (keyboard.noteAt ({ 30.0f, 20.0f }) == 49);  // C#2, top of the first boundary
        CHECK (keyboard.noteAt ({ 215.0f, 90.0f }) == 60); // C3
        CHECK (keyboard.noteAt ({ 625.0f, 90.0f }) == 83); // B4, last key
        CHECK_FALSE (keyboard.noteAt ({ 700.0f, 90.0f }).has_value());
    }
}

TEST_CASE ("Saving and playing slots", "[keyboard]")
{
    PluginProcessor plugin;
    auto& slots = plugin.getChordSlots();
    ui::ChordKeyboard keyboard (plugin.getParameterTree(), slots);
    ui::ChordSlotButtons slotButtons (plugin.getParameterTree(), slots);

    SECTION ("clicking an empty slot saves the keyboard's chord and plays it")
    {
        slotButtons.clickSlot (2);
        CHECK (slots.getSlot (2) == chord ({ 48, 52, 55 }));
        CHECK (activeSlot (plugin) == 3);
    }

    SECTION ("clicking a filled slot plays it and shows it on the keyboard")
    {
        slots.setSlot (4, chord ({ 53, 57, 60 }));
        slotButtons.clickSlot (4);
        CHECK (activeSlot (plugin) == 5);
        CHECK (slots.getSlot (4) == chord ({ 53, 57, 60 })); // not overwritten
        CHECK (keyboard.getShownNotes() == chord ({ 53, 57, 60 }));
    }

    SECTION ("an empty slot can't be filled with nothing")
    {
        keyboard.clearNotes();
        bool told = false;
        slotButtons.onNothingToSave = [&] { told = true; };
        slotButtons.clickSlot (0);
        CHECK (told);
        CHECK (slots.getSlot (0).none());
        CHECK (activeSlot (plugin) == params::pianoSlot);
    }

    SECTION ("replace and clear")
    {
        slots.setSlot (1, chord ({ 50, 53, 57 }));
        slotButtons.saveToSlot (1); // the keyboard shows the piano selection, C2 E2 G2
        CHECK (slots.getSlot (1) == chord ({ 48, 52, 55 }));
        slotButtons.clearSlot (1);
        CHECK (slots.getSlot (1).none());
    }

    SECTION ("automating Chord Slot relights the keyboard")
    {
        slots.setSlot (6, chord ({ 55, 59, 62, 65 }));
        setValue (plugin, params::id::chordSlot, 7.0f);
        CHECK (keyboard.getShownNotes() == chord ({ 55, 59, 62, 65 }));
        setValue (plugin, params::id::chordSlot, 0.0f);
        CHECK (keyboard.getShownNotes() == chord ({ 48, 52, 55 }));
    }
}

TEST_CASE ("A slot click and the same automation sound identical", "[keyboard]")
{
    PluginProcessor clicked, automated;
    for (auto* plugin : { &clicked, &automated })
        plugin->getChordSlots().setSlot (0, chord ({ 57, 60, 64 }));

    ui::ChordSlotButtons buttons (clicked.getParameterTree(), clicked.getChordSlots());
    buttons.clickSlot (0);
    setValue (automated, params::id::chordSlot, 1.0f);

    prepare (clicked);
    prepare (automated);
    const auto a = render (clicked, 48000);
    const auto b = render (automated, 48000);

    REQUIRE (a.size() == b.size());
    bool identical = true;
    for (size_t i = 0; i < a.size(); ++i)
        identical = identical && juce::exactlyEqual (a[i], b[i]);
    CHECK (identical);
}

TEST_CASE ("Chords and slots survive a session reload", "[keyboard]")
{
    juce::MemoryBlock state;
    {
        PluginProcessor plugin;
        auto& slots = plugin.getChordSlots();
        ui::ChordKeyboard keyboard (plugin.getParameterTree(), slots);
        keyboard.toggleNote (59);
        slots.setSlot (3, chord ({ 57, 60, 64, 67 }));
        setValue (plugin, params::id::chordSlot, 4.0f);
        plugin.getStateInformation (state);
    }

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.getChordSlots().getPiano() == chord ({ 48, 52, 55, 59 }));
    CHECK (restored.getChordSlots().getSlot (3) == chord ({ 57, 60, 64, 67 }));
    CHECK (activeSlot (restored) == 4);

    ui::ChordKeyboard keyboard (restored.getParameterTree(), restored.getChordSlots());
    CHECK (keyboard.getShownNotes() == chord ({ 57, 60, 64, 67 }));
}

TEST_CASE ("Switching slots mid-note doesn't click", "[keyboard]")
{
    PluginProcessor plugin;
    setValue (plugin, params::id::harmonics, 1.0f); // fundamentals only, so the output is smooth
    plugin.getChordSlots().setPiano (chord ({ 60, 64, 67 }));
    plugin.getChordSlots().setSlot (0, chord ({ 65, 69, 72 }));
    prepare (plugin);

    const auto before = render (plugin, 48000);
    setValue (plugin, params::id::chordSlot, 1.0f);
    const auto after = render (plugin, 24000);

    const auto maxSecondDifference = [] (const std::vector<float>& y, size_t start, size_t length) {
        double peak = 0.0;
        for (auto i = std::max<size_t> (start, 2); i < std::min (y.size(), start + length); ++i)
            peak = std::max (peak, (double) std::abs (y[i] - 2.0f * y[i - 1] + y[i - 2]));
        return peak;
    };

    const auto steady = maxSecondDifference (before, 24000, 24000);
    const auto change = maxSecondDifference (after, 0, 4800);
    INFO ("steady " << steady << " change " << change);
    CHECK (change < 2.0 * steady);
    CHECK (plugin.getChordNotes() == chord ({ 65, 69, 72 }));
}

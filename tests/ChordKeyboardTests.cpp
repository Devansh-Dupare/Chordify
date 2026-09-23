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

    float valueOf (PluginProcessor& plugin, const char* paramId)
    {
        return plugin.getParameterTree().getRawParameterValue (paramId)->load();
    }

    ui::ChordKeyboard makeKeyboard (PluginProcessor& plugin)
    {
        return ui::ChordKeyboard (plugin.getParameterTree(), [&plugin] {
            return ui::ChordKeyboard::SoundingChord { plugin.getChordNotes(), plugin.getChordRoot() };
        });
    }

    std::vector<int> highlightedNotes (const ui::ChordKeyboard& keyboard)
    {
        std::vector<int> notes;
        for (int n = 0; n < 128; ++n)
            if (keyboard.getHighlightedNotes()[(size_t) n])
                notes.push_back (n);
        return notes;
    }

    // Renders noise through the plugin in 256-sample blocks without re-preparing it
    std::vector<float> render (PluginProcessor& plugin, size_t numSamples, const juce::MidiBuffer& firstBlockMidi = {})
    {
        constexpr int blockSize = 256;
        juce::Random random (5);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (size_t start = 0; start < numSamples; start += blockSize)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, 0.2f * (random.nextFloat() * 2.0f - 1.0f));
            juce::MidiBuffer midi;
            if (start == 0)
                midi = firstBlockMidi;
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

TEST_CASE ("Piano keyboard follows the parameters", "[keyboard]")
{
    PluginProcessor plugin;
    auto keyboard = makeKeyboard (plugin);

    SECTION ("the default chord is lit on open")
    {
        CHECK (highlightedNotes (keyboard) == std::vector<int> { 48, 52, 55 }); // C2 major
        CHECK (keyboard.getHighlightedRoot() == 48);
        CHECK (keyboard.getLowestNote() == 48);
    }

    SECTION ("host automation of Root and Chord Type relights the keys")
    {
        setValue (plugin, params::id::root, 57.0f);
        setValue (plugin, params::id::chordType, 7.0f); // A minor 7
        CHECK (highlightedNotes (keyboard) == std::vector<int> { 57, 60, 64, 67 });
        CHECK (keyboard.getHighlightedRoot() == 57);
    }

    SECTION ("the window follows a root in another octave")
    {
        setValue (plugin, params::id::root, 75.0f); // D#4
        CHECK (keyboard.getLowestNote() == 72);
    }

    SECTION ("a keyboard opened after automation shows the current chord")
    {
        setValue (plugin, params::id::root, 62.0f);
        setValue (plugin, params::id::chordType, 1.0f);
        auto reopened = makeKeyboard (plugin);
        CHECK (highlightedNotes (reopened) == std::vector<int> { 62, 65, 69 });
    }
}

TEST_CASE ("Piano keyboard writes the parameters", "[keyboard]")
{
    PluginProcessor plugin;
    auto keyboard = makeKeyboard (plugin);

    SECTION ("selecting a key sets Root")
    {
        keyboard.selectRoot (53);
        CHECK (juce::roundToInt (valueOf (plugin, params::id::root)) == 53);
        CHECK (keyboard.getHighlightedRoot() == 53);
    }

    SECTION ("clicking keys in the upper octave doesn't scroll the keyboard")
    {
        REQUIRE (keyboard.getLowestNote() == 48); // showing C2 - B3
        keyboard.selectRoot (71);                 // B3, the last key
        CHECK (keyboard.getLowestNote() == 48);
        keyboard.selectRoot (60);                 // C3
        CHECK (keyboard.getLowestNote() == 48);
        keyboard.selectRoot (49);
        CHECK (keyboard.getLowestNote() == 48);
    }

    SECTION ("selecting a key in a MIDI mode switches back to Internal")
    {
        setValue (plugin, params::id::chordSource, (float) params::ChordSource::midi);
        keyboard.selectRoot (55);
        CHECK (juce::roundToInt (valueOf (plugin, params::id::chordSource)) == (int) params::ChordSource::internal);
        CHECK (juce::roundToInt (valueOf (plugin, params::id::root)) == 55);
    }

    SECTION ("octave buttons move Root within its range")
    {
        keyboard.shiftOctave (1);
        CHECK (juce::roundToInt (valueOf (plugin, params::id::root)) == 60);
        CHECK (keyboard.getLowestNote() == 60); // the view moves with it
        keyboard.shiftOctave (-1);
        keyboard.shiftOctave (-1);
        CHECK (juce::roundToInt (valueOf (plugin, params::id::root)) == 36);
        keyboard.shiftOctave (-1);
        keyboard.shiftOctave (-1); // 12 would be below the range: stays at 24
        CHECK (juce::roundToInt (valueOf (plugin, params::id::root)) == 24);
    }

    SECTION ("keys map to the right notes on screen")
    {
        keyboard.setBounds (0, 0, 14 * 30, 100); // two octaves of 30 px white keys, starting at C2
        CHECK (keyboard.noteAt ({ 5.0f, 90.0f }) == 48);   // C2, bottom of the first white key
        CHECK (keyboard.noteAt ({ 30.0f, 20.0f }) == 49);  // C#2, top of the first boundary
        CHECK (keyboard.noteAt ({ 45.0f, 90.0f }) == 50);  // D2
        CHECK (keyboard.noteAt ({ 215.0f, 90.0f }) == 60); // C3, first white key of the second octave
        CHECK (keyboard.noteAt ({ 415.0f, 90.0f }) == 71); // B3, last key
        CHECK_FALSE (keyboard.noteAt ({ 500.0f, 90.0f }).has_value());
    }
}

TEST_CASE ("Chord type buttons follow the parameter", "[keyboard]")
{
    PluginProcessor plugin;
    ui::ChordTypeButtons buttons (plugin.getParameterTree());

    auto toggled = [&] {
        juce::StringArray on;
        for (auto* child : buttons.getChildren())
            if (auto* button = dynamic_cast<juce::Button*> (child); button != nullptr && button->getToggleState())
                on.add (button->getButtonText());
        return on;
    };

    CHECK (toggled() == juce::StringArray { "Maj" });
    setValue (plugin, params::id::chordType, 7.0f);
    CHECK (toggled() == juce::StringArray { "Min7" });

    for (auto* child : buttons.getChildren())
        if (auto* button = dynamic_cast<juce::Button*> (child); button != nullptr && button->getButtonText() == "Sus4")
            button->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50); // triggerClick is asynchronous
    CHECK (juce::roundToInt (valueOf (plugin, params::id::chordType)) == 5);
}

TEST_CASE ("A key click and the same automation sound identical", "[keyboard]")
{
    PluginProcessor clicked, automated;
    auto keyboard = makeKeyboard (clicked);
    keyboard.selectRoot (57);
    setValue (automated, params::id::root, 57.0f);

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

TEST_CASE ("A chord picked on the keyboard survives a session reload", "[keyboard]")
{
    juce::MemoryBlock state;
    {
        PluginProcessor plugin;
        auto keyboard = makeKeyboard (plugin);
        keyboard.selectRoot (63);
        setValue (plugin, params::id::chordType, 6.0f);
        plugin.getStateInformation (state);
    }

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    auto keyboard = makeKeyboard (restored);
    CHECK (highlightedNotes (keyboard) == std::vector<int> { 63, 67, 70, 74 }); // D#3 major 7
}

TEST_CASE ("Switching Chord Source mid-note doesn't click", "[keyboard]")
{
    PluginProcessor plugin;
    setValue (plugin, params::id::harmonics, 1.0f); // fundamentals only, so the output is smooth
    setValue (plugin, params::id::root, 60.0f);
    prepare (plugin);

    // F major held on MIDI while the internal C major plays
    juce::MidiBuffer notes;
    for (auto note : { 65, 69, 72 })
        notes.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
    const auto before = render (plugin, 48000, notes);

    setValue (plugin, params::id::chordSource, (float) params::ChordSource::midi);
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
    CHECK (plugin.getChordNotes()[65]); // now playing the MIDI chord
}

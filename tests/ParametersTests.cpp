#include "helpers/dsp_test_helpers.h"
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

    ChordSlots::Notes chord (std::initializer_list<int> notes)
    {
        ChordSlots::Notes result;
        for (auto n : notes)
            result[(size_t) n] = true;
        return result;
    }
}

TEST_CASE ("Parameter layout", "[params]")
{
    PluginProcessor plugin;

    SECTION ("every parameter is exposed to the host")
    {
        for (auto* paramId : { params::id::chordSlot, params::id::harmonics, params::id::detune, params::id::spread, params::id::glide,
                 params::id::decay, params::id::excite, params::id::brightness, params::id::timbre, params::id::mix, params::id::inputHpf,
                 params::id::tone, params::id::output })
        {
            INFO (paramId);
            CHECK (plugin.getParameterTree().getParameter (paramId) != nullptr);
        }
        CHECK (plugin.getParameters().size() == 13);
    }

    SECTION ("defaults")
    {
        CHECK (juce::roundToInt (valueOf (plugin, params::id::chordSlot)) == params::pianoSlot);
        CHECK (valueOf (plugin, params::id::harmonics) == 8.0f);
        CHECK (valueOf (plugin, params::id::decay) == Catch::Approx (1.5f));
        CHECK (valueOf (plugin, params::id::mix) == 100.0f);
        CHECK (valueOf (plugin, params::id::inputHpf) == 20.0f);
    }
}

TEST_CASE ("State save and restore", "[params]")
{
    juce::MemoryBlock state;
    {
        PluginProcessor plugin;
        setValue (plugin, params::id::chordSlot, 3.0f);
        setValue (plugin, params::id::decay, 4.25f);
        setValue (plugin, params::id::harmonics, 12.0f);
        plugin.getStateInformation (state);
    }

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (valueOf (restored, params::id::chordSlot) == 3.0f);
    CHECK (valueOf (restored, params::id::decay) == Catch::Approx (4.25f).margin (0.001f));
    CHECK (valueOf (restored, params::id::harmonics) == 12.0f);

    SECTION ("garbage state is ignored")
    {
        const char junk[] = "not a plugin state";
        restored.setStateInformation (junk, (int) sizeof (junk));
        CHECK (valueOf (restored, params::id::harmonics) == 12.0f);
    }
}

TEST_CASE ("Chordify is a plain audio effect", "[params]")
{
    PluginProcessor plugin;
    CHECK_FALSE (plugin.acceptsMidi());
    CHECK_FALSE (plugin.isMidiEffect());
    CHECK_FALSE (plugin.producesMidi());
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

TEST_CASE ("Mix at 0% passes the input through unchanged", "[mix]")
{
    PluginProcessor plugin;
    setValue (plugin, params::id::mix, 0.0f);

    constexpr int blockSize = 256;
    plugin.setPlayConfigDetails (2, 2, 48000.0, blockSize);
    plugin.prepareToPlay (48000.0, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::Random random (3);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, random.nextFloat() - 0.5f);
    const juce::AudioBuffer<float> input (buffer);

    juce::MidiBuffer midi;
    plugin.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            CHECK (juce::exactlyEqual (buffer.getSample (ch, i), input.getSample (ch, i)));
}

TEST_CASE ("Processor reports zero latency and a decay-length tail", "[latency]")
{
    PluginProcessor plugin;
    plugin.prepareToPlay (48000.0, 512);
    CHECK (plugin.getLatencySamples() == 0);
    CHECK (plugin.getTailLengthSeconds() == Catch::Approx (1.5));
}

namespace
{
    // Renders a mono signal through the plugin (stereo in/out) and returns the left output
    std::vector<float> renderThroughPlugin (PluginProcessor& plugin, const std::vector<float>& input)
    {
        constexpr int blockSize = 512;
        plugin.setPlayConfigDetails (2, 2, 48000.0, blockSize);
        plugin.prepareToPlay (48000.0, blockSize);

        std::vector<float> out (input.size());
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (size_t start = 0; start < input.size(); start += blockSize)
        {
            const auto n = (int) std::min ((size_t) blockSize, input.size() - start);
            buffer.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom (ch, 0, input.data() + start, n);

            juce::MidiBuffer midi;
            plugin.processBlock (buffer, midi);
            std::copy_n (buffer.getReadPointer (0), n, out.begin() + (std::ptrdiff_t) start);
        }
        return out;
    }

    // Level of each chord tone's fundamental, as mean power over 1 s windows after the first second.
    // Noise-excited resonators fluctuate a lot within any one window, so averaging matters.
    std::vector<double> fundamentalLevels (const std::vector<float>& out, std::initializer_list<int> notes)
    {
        constexpr size_t window = 48000;
        std::vector<double> levels;
        for (auto note : notes)
        {
            double power = 0.0;
            int windows = 0;
            for (size_t start = window; start + window <= out.size(); start += window, ++windows)
                power += std::pow (10.0, test::toneDb (out, start, window, 440.0 * std::exp2 ((note - 69) / 12.0), 48000.0) / 10.0);
            levels.push_back (10.0 * std::log10 (power / std::max (windows, 1)));
        }
        return levels;
    }

    double spreadDb (const std::vector<double>& levels)
    {
        return *std::max_element (levels.begin(), levels.end()) - *std::min_element (levels.begin(), levels.end());
    }

    // Band-limited sawtooth: a pitched input like a held synth or sung note
    std::vector<float> sawtooth (double freq, double seconds)
    {
        std::vector<float> x ((size_t) (seconds * 48000.0));
        for (size_t i = 0; i < x.size(); ++i)
            for (int k = 1; k * freq < 20000.0; ++k)
                x[i] += 0.1f * (float) (std::sin (2.0 * std::numbers::pi * freq * k * (double) i / 48000.0) / k);
        return x;
    }
}

TEST_CASE ("A pitched input produces the whole chord", "[chord]")
{
    // An A2 sawtooth into a C2 major chord: A2's 3rd harmonic (330 Hz) lines up with E2's 2nd (329.6 Hz),
    // so without excitation that one partial dominates and the chord collapses to a single note
    const auto input = sawtooth (110.0, 8.0);

    SECTION ("raw input: one note dominates")
    {
        PluginProcessor plugin;
        setValue (plugin, params::id::excite, 0.0f);
        setValue (plugin, params::id::detune, 0.0f);
        const auto out = renderThroughPlugin (plugin, input);

        const auto e2Harmonic = test::toneDb (out, 48000, out.size() - 48000, 329.63, 48000.0);
        const auto fundamentals = fundamentalLevels (out, { 48, 52, 55 });
        CHECK (e2Harmonic - *std::max_element (fundamentals.begin(), fundamentals.end()) > 20.0);
    }

    SECTION ("excited: every chord tone rings at a similar level")
    {
        PluginProcessor plugin; // Excite defaults to 100%
        setValue (plugin, params::id::detune, 0.0f);
        setValue (plugin, params::id::decay, 0.3f); // wider resonances average the noise faster
        const auto out = renderThroughPlugin (plugin, input);

        const auto levels = fundamentalLevels (out, { 48, 52, 55 });
        INFO ("C2 " << levels[0] << " dB, E2 " << levels[1] << " dB, G2 " << levels[2] << " dB");
        CHECK (spreadDb (levels) < 8.0);
        CHECK (levels[1] > -60.0);
    }
}

TEST_CASE ("The notes picked are the notes that sound", "[chord]")
{
    // The same input through A minor (A2 C3 E3) and A major (A2 C#3 E3), picked as notes
    auto render = [] (const ChordSlots::Notes& notes) {
        PluginProcessor plugin;
        plugin.getChordSlots().setPiano (notes);
        setValue (plugin, params::id::detune, 0.0f);
        setValue (plugin, params::id::decay, 0.3f);
        return renderThroughPlugin (plugin, sawtooth (110.0, 8.0));
    };

    const auto minor = render (chord ({ 57, 60, 64 }));
    const auto major = render (chord ({ 57, 61, 64 }));

    const auto minorTones = fundamentalLevels (minor, { 57, 60, 64 });
    INFO ("A2 " << minorTones[0] << " C3 " << minorTones[1] << " E3 " << minorTones[2]);
    CHECK (spreadDb (minorTones) < 8.0);

    // The third follows the notes picked: C3 in minor, C#3 in major. The absent third still
    // measures some level, spilling from the neighbouring third's resonator a semitone away
    // (wide at this short decay), so 10 dB is a clear margin rather than a silence check.
    const auto c3 = 60, cSharp3 = 61;
    CHECK (fundamentalLevels (minor, { c3 })[0] - fundamentalLevels (major, { c3 })[0] > 10.0);
    CHECK (fundamentalLevels (major, { cSharp3 })[0] - fundamentalLevels (minor, { cSharp3 })[0] > 10.0);
}

TEST_CASE ("Output gain scales the final output", "[output]")
{
    PluginProcessor plugin;
    setValue (plugin, params::id::mix, 0.0f);
    setValue (plugin, params::id::output, -6.0f);

    std::vector<float> input (4800, 0.5f);
    const auto out = renderThroughPlugin (plugin, input);
    CHECK (out.back() == Catch::Approx (0.5f * juce::Decibels::decibelsToGain (-6.0f)).margin (1e-4));

    const auto peaks = plugin.takeOutputPeaks();
    CHECK (peaks[0] == Catch::Approx (0.5f * juce::Decibels::decibelsToGain (-6.0f)).margin (1e-3));
    CHECK (plugin.takeOutputPeaks()[0] == 0.0f); // reading resets the meter
}

TEST_CASE ("Chord Slot picks what plays", "[chord]")
{
    PluginProcessor plugin;
    auto& slots = plugin.getChordSlots();
    slots.setPiano (chord ({ 57, 60, 64 }));
    slots.setSlot (2, chord ({ 53, 57, 60, 64 }));

    SECTION ("Piano plays the piano selection")
    {
        renderThroughPlugin (plugin, std::vector<float> (1024, 0.0f));
        CHECK (plugin.getChordNotes() == chord ({ 57, 60, 64 }));
    }

    SECTION ("a filled slot plays its chord")
    {
        setValue (plugin, params::id::chordSlot, 3.0f);
        renderThroughPlugin (plugin, std::vector<float> (1024, 0.0f));
        CHECK (plugin.getChordNotes() == chord ({ 53, 57, 60, 64 }));
    }

    SECTION ("an empty slot plays nothing")
    {
        setValue (plugin, params::id::chordSlot, 5.0f);
        renderThroughPlugin (plugin, std::vector<float> (1024, 0.0f));
        CHECK (plugin.getChordNotes().none());
    }
}

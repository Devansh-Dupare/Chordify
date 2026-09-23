#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <dsp/ChordMapper.h>

using namespace chordify;

namespace
{
    size_t slot (int voice, int harmonicIndex)
    {
        return (size_t) (voice * maxHarmonics + harmonicIndex);
    }

    int countUsed (const Voices& voices)
    {
        int n = 0;
        for (const auto& v : voices)
            n += v.used ? 1 : 0;
        return n;
    }

    PartialSettings plainSettings()
    {
        PartialSettings s;
        s.detuneCents = 0.0f;
        s.spread = 0.0f;
        return s;
    }
}

TEST_CASE ("MIDI note to frequency", "[chordmapper]")
{
    CHECK (midiNoteToHz (69.0) == Catch::Approx (440.0));
    CHECK (midiNoteToHz (60.0) == Catch::Approx (261.6256).epsilon (1e-6));
    CHECK (midiNoteToHz (81.0) == Catch::Approx (880.0));
    CHECK (midiNoteToHz (69.5) == Catch::Approx (440.0 * std::exp2 (0.5 / 12.0)));
}

TEST_CASE ("Internal chords", "[chordmapper]")
{
    const Voices none {};

    SECTION ("major triad on C3")
    {
        const auto voices = internalChord (60, 0, none);
        CHECK (countUsed (voices) == 3);
        CHECK (voices[0].note == 60.0f);
        CHECK (voices[1].note == 64.0f);
        CHECK (voices[2].note == 67.0f);
        CHECK (voices[0].gated);
    }

    SECTION ("minor 7th has four tones")
    {
        const auto voices = internalChord (57, 7, none);
        CHECK (countUsed (voices) == 4);
        CHECK (voices[1].note == 60.0f);
        CHECK (voices[3].note == 67.0f);
    }

    SECTION ("tones dropped by a chord change ring out")
    {
        const auto seventh = internalChord (60, 6, none); // major 7: C E G B
        const auto triad = internalChord (60, 0, seventh);
        CHECK (triad[3].used);
        CHECK_FALSE (triad[3].gated);
        CHECK (triad[3].note == 71.0f);
    }

    SECTION ("out of range chord types are clamped")
    {
        CHECK (chordIntervals (-1).size() == 3);
        CHECK (chordIntervals (1000).size() == 2); // power chord, the last entry
    }
}

TEST_CASE ("Voice allocation", "[chordmapper]")
{
    VoiceAllocator allocator;

    SECTION ("notes are held and released")
    {
        allocator.noteOn (60);
        allocator.noteOn (64);
        auto voices = allocator.getVoices();
        CHECK (countUsed (voices) == 2);

        allocator.noteOff (60);
        voices = allocator.getVoices();
        CHECK (voices[0].used);
        CHECK_FALSE (voices[0].gated);
        CHECK (voices[0].note == 60.0f); // keeps its pitch while ringing out
        CHECK (voices[1].gated);
    }

    SECTION ("a ninth note steals the voice held the longest")
    {
        for (int n = 0; n < maxVoices; ++n)
            allocator.noteOn (60 + n);
        allocator.noteOn (80);

        const auto voices = allocator.getVoices();
        CHECK (voices[0].note == 80.0f);
        CHECK (voices[1].note == 61.0f);
    }

    SECTION ("new notes prefer unused voices, then the longest-released")
    {
        allocator.noteOn (60);
        allocator.noteOn (62);
        allocator.noteOff (60);
        allocator.noteOn (64);
        CHECK (allocator.getVoices()[2].note == 64.0f); // unused voice, voice 0 keeps ringing

        for (int n = 3; n < maxVoices; ++n)
            allocator.noteOn (70 + n);
        allocator.noteOff (62);
        allocator.noteOn (90);
        CHECK (allocator.getVoices()[0].note == 90.0f); // released before voice 1
    }

    SECTION ("repeating a held note reuses its voice")
    {
        allocator.noteOn (60);
        allocator.noteOn (60);
        CHECK (countUsed (allocator.getVoices()) == 1);
    }

    SECTION ("all notes off releases everything")
    {
        allocator.noteOn (60);
        allocator.noteOn (67);
        allocator.allNotesOff();
        for (const auto& v : allocator.getVoices())
            CHECK_FALSE (v.gated);
    }

    SECTION ("pitch bend shifts every voice by up to two semitones")
    {
        allocator.noteOn (60);
        allocator.setPitchBend (1.0f);
        CHECK (allocator.getVoices()[0].note == 62.0f);
        allocator.setPitchBend (-0.5f);
        CHECK (allocator.getVoices()[0].note == 59.0f);
    }
}

TEST_CASE ("Partials", "[chordmapper]")
{
    Voices voices {};
    voices[0] = { 33.0f, true, true }; // A1 = 55 Hz

    SECTION ("harmonic series with 1/k roll-off at default brightness")
    {
        auto settings = plainSettings();
        settings.numHarmonics = 4;
        const auto grid = computePartials (voices, settings);

        for (int h = 0; h < 4; ++h)
        {
            CHECK (grid.freqHz[slot (0, h)] == Catch::Approx (55.0 * (h + 1)));
            CHECK (grid.amplitude[slot (0, h)] == Catch::Approx (1.0 / (h + 1)));
            CHECK (grid.gate[slot (0, h)] == 1.0f);
        }
        CHECK (grid.amplitude[slot (0, 4)] == 0.0f);
        CHECK (grid.amplitude[slot (1, 0)] == 0.0f);
    }

    SECTION ("brightness sets the roll-off exponent")
    {
        auto settings = plainSettings();
        settings.brightness = 0.0f; // 1/k^2
        CHECK (computePartials (voices, settings).amplitude[slot (0, 2)] == Catch::Approx (1.0 / 9.0));
        settings.brightness = 1.0f; // flat
        CHECK (computePartials (voices, settings).amplitude[slot (0, 7)] == Catch::Approx (1.0));
    }

    SECTION ("odd harmonics only")
    {
        auto settings = plainSettings();
        settings.oddOnly = true;
        const auto grid = computePartials (voices, settings);
        CHECK (grid.freqHz[slot (0, 1)] == Catch::Approx (165.0));
        CHECK (grid.freqHz[slot (0, 2)] == Catch::Approx (275.0));
    }

    SECTION ("partials above 0.45 fs are dropped")
    {
        voices[0].note = 105.0f; // ~3520 Hz
        auto settings = plainSettings();
        settings.numHarmonics = 16;
        settings.sampleRate = 44100.0;
        const auto grid = computePartials (voices, settings);

        for (int h = 0; h < maxHarmonics; ++h)
        {
            const auto expectedKept = 3520.0 * (h + 1) <= 0.45 * 44100.0;
            CHECK ((grid.amplitude[slot (0, h)] > 0.0f) == expectedKept);
        }
    }

    SECTION ("shared harmonics of a fifth are merged into one resonator")
    {
        voices[1] = { 40.0f, true, true }; // E2, a fifth above: its 2nd harmonic ~= A1's 3rd (2 cents apart)
        const auto grid = computePartials (voices, plainSettings());

        CHECK (grid.amplitude[slot (0, 2)] == Catch::Approx (0.5)); // A1 x3 keeps the louder amplitude (E2 x2)
        CHECK (grid.amplitude[slot (1, 1)] == 0.0f);
        CHECK (grid.amplitude[slot (1, 0)] > 0.0f); // E2's fundamental is untouched
    }

    SECTION ("released partials are not merged")
    {
        voices[1] = { 40.0f, true, false };
        const auto grid = computePartials (voices, plainSettings());
        CHECK (grid.amplitude[slot (1, 1)] > 0.0f);
        CHECK (grid.gate[slot (1, 1)] == 0.0f);
    }

    SECTION ("detune and spread stay within range and are deterministic")
    {
        auto settings = plainSettings();
        settings.detuneCents = 20.0f;
        settings.spread = 1.0f;
        const auto a = computePartials (voices, settings);
        const auto b = computePartials (voices, settings);

        bool anyDetuned = false;
        for (int h = 0; h < settings.numHarmonics; ++h)
        {
            const auto cents = 1200.0 * std::log2 (a.freqHz[slot (0, h)] / (55.0 * (h + 1)));
            CHECK (std::abs (cents) <= 20.001);
            CHECK (std::abs (a.pan[slot (0, h)]) <= 1.0f);
            CHECK (a.freqHz[slot (0, h)] == Catch::Approx (b.freqHz[slot (0, h)]).epsilon (0.0));
            anyDetuned = anyDetuned || std::abs (cents) > 1.0;
        }
        CHECK (anyDetuned);
    }
}

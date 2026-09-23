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

TEST_CASE ("Chords from notes", "[chordmapper]")
{
    const auto notes = [] (std::initializer_list<int> list) {
        std::bitset<128> result;
        for (auto n : list)
            result[(size_t) n] = true;
        return result;
    };

    SECTION ("one gated voice per note, lowest first")
    {
        const auto voices = chordFromNotes (notes ({ 67, 60, 64 }), {});
        CHECK (countUsed (voices) == 3);
        CHECK (voices[0].note == 60.0f);
        CHECK (voices[1].note == 64.0f);
        CHECK (voices[2].note == 67.0f);
        CHECK (voices[2].gated);
    }

    SECTION ("a chord change moves each voice to the note in the same position")
    {
        const auto cMajor = chordFromNotes (notes ({ 60, 64, 67 }), {});
        const auto fMajor = chordFromNotes (notes ({ 60, 65, 69 }), cMajor);
        CHECK (fMajor[1].note == 65.0f); // E3 glides to F3
        CHECK (fMajor[2].note == 69.0f); // G3 glides to A3
    }

    SECTION ("notes dropped by a chord change ring out at their pitch")
    {
        const auto seventh = chordFromNotes (notes ({ 60, 64, 67, 71 }), {});
        const auto triad = chordFromNotes (notes ({ 60, 64, 67 }), seventh);
        CHECK (triad[3].used);
        CHECK_FALSE (triad[3].gated);
        CHECK (triad[3].note == 71.0f);

        const auto silence = chordFromNotes ({}, triad);
        CHECK (countUsed (silence) == 4);
        for (const auto& voice : silence)
            CHECK_FALSE (voice.gated);
    }

    SECTION ("no more than maxChordNotes notes")
    {
        const auto voices = chordFromNotes (notes ({ 48, 50, 52, 53, 55, 57, 59, 60, 62, 64 }), {});
        CHECK (countUsed (voices) == maxChordNotes);
        CHECK (voices[(size_t) maxChordNotes - 1].note == 60.0f); // the lowest eight are kept
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

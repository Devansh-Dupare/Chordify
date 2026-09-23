// Offline render harness: feeds a test signal (or a WAV file) through PluginProcessor
// and writes the result to a WAV, so DSP changes can be auditioned without a DAW.
// Also reports latency, CPU cost (realtime factor) and output levels.

#include <PluginProcessor.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <bit>

namespace
{
    constexpr auto usage = R"(Usage: Render [options]
  --input   noise|pink|impulse|sine|<file.wav>   test signal or input file (default: noise)
  --out     <file.wav>                           output path (default: render.wav)
  --seconds <n>                                  length of generated signal (default: 4)
  --sr      <hz>                                 sample rate for generated signal (default: 48000)
  --block   <n>                                  processing block size (default: 512)
  --gain    <db>                                 level of generated signal (default: -12)
  --notes   <n,n,...>                            chord to play (MIDI note numbers, up to 8), e.g. 60,64,67
  --preset  <index|name>                         load a factory preset first (--set still overrides)
  --set     <paramID>=<value>                    set a parameter (real-world value), repeatable
  --list                                         list the plugin's parameters and presets, then exit
)";

    struct Options
    {
        juce::String input = "noise";
        juce::String out = "render.wav";
        double seconds = 4.0;
        double sampleRate = 48000.0;
        int blockSize = 512;
        float gainDb = -12.0f;
        juce::Array<int> notes;
        juce::String preset;
        juce::StringPairArray params;
        bool listParams = false;
    };

    std::optional<Options> parseArgs (const juce::StringArray& args)
    {
        Options o;

        for (int i = 0; i < args.size(); ++i)
        {
            const auto& arg = args[i];
            const auto next = [&]() -> juce::String {
                if (i + 1 >= args.size())
                    throw std::runtime_error ("missing value for " + arg.toStdString());
                return args[++i];
            };

            if (arg == "--input")
                o.input = next();
            else if (arg == "--out")
                o.out = next();
            else if (arg == "--seconds")
                o.seconds = next().getDoubleValue();
            else if (arg == "--sr")
                o.sampleRate = next().getDoubleValue();
            else if (arg == "--block")
                o.blockSize = next().getIntValue();
            else if (arg == "--gain")
                o.gainDb = next().getFloatValue();
            else if (arg == "--notes")
            {
                for (const auto& n : juce::StringArray::fromTokens (next(), ",", ""))
                    o.notes.add (n.trim().getIntValue());
            }
            else if (arg == "--set")
            {
                const auto kv = next();
                if (! kv.contains ("="))
                    throw std::runtime_error ("--set expects paramID=value");
                o.params.set (kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false));
            }
            else if (arg == "--preset")
                o.preset = next();
            else if (arg == "--list")
                o.listParams = true;
            else if (arg == "--help" || arg == "-h")
                return std::nullopt;
            else
                throw std::runtime_error ("unknown option " + arg.toStdString());
        }

        if (o.seconds <= 0.0 || o.sampleRate <= 0.0 || o.blockSize <= 0)
            throw std::runtime_error ("--seconds, --sr and --block must be positive");

        return o;
    }

    juce::AudioBuffer<float> generateSignal (const Options& o)
    {
        const auto numSamples = (int) std::lround (o.seconds * o.sampleRate);
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        const auto gain = juce::Decibels::decibelsToGain (o.gainDb);
        juce::Random random (1234); // fixed seed so renders are reproducible

        if (o.input == "noise")
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < numSamples; ++i)
                    buffer.setSample (ch, i, gain * (random.nextFloat() * 2.0f - 1.0f));
        }
        else if (o.input == "pink")
        {
            // Paul Kellett's refined pink noise filter, normalised to roughly unit peak
            for (int ch = 0; ch < 2; ++ch)
            {
                float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto white = random.nextFloat() * 2.0f - 1.0f;
                    b0 = 0.99886f * b0 + white * 0.0555179f;
                    b1 = 0.99332f * b1 + white * 0.0750759f;
                    b2 = 0.96900f * b2 + white * 0.1538520f;
                    b3 = 0.86650f * b3 + white * 0.3104856f;
                    b4 = 0.55000f * b4 + white * 0.5329522f;
                    b5 = -0.7616f * b5 - white * 0.0168980f;
                    const auto pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
                    b6 = white * 0.115926f;
                    buffer.setSample (ch, i, gain * pink * 0.2f);
                }
            }
        }
        else if (o.input == "impulse")
        {
            // One single-sample click per second
            const auto interval = (int) o.sampleRate;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < numSamples; i += interval)
                    buffer.setSample (ch, i, gain);
        }
        else if (o.input == "sine")
        {
            // 440 Hz reference tone
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < numSamples; ++i)
                    buffer.setSample (ch, i, gain * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / o.sampleRate));
        }
        else
            throw std::runtime_error ("unknown input signal " + o.input.toStdString());

        return buffer;
    }

    juce::AudioBuffer<float> readFile (const juce::File& file, double& sampleRate)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            throw std::runtime_error ("could not read " + file.getFullPathName().toStdString());

        const auto numSamples = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> buffer (2, numSamples);
        reader->read (&buffer, 0, numSamples, 0, true, true); // mono files fill both channels
        sampleRate = reader->sampleRate;
        return buffer;
    }

    juce::RangedAudioParameter* findParameter (juce::AudioProcessor& processor, const juce::String& id)
    {
        for (auto* p : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == id)
                return ranged;
        return nullptr;
    }

    int run (const Options& o)
    {
        PluginProcessor processor;

        if (o.listParams)
        {
            if (processor.getParameters().isEmpty())
                std::cout << "(no parameters)\n";
            for (auto* p : processor.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                    std::cout << ranged->getParameterID() << "  \"" << ranged->getName (64) << "\"  default "
                              << ranged->getCurrentValueAsText() << "\n";
            std::cout << "\npresets:\n";
            for (int i = 0; i < processor.getNumPrograms(); ++i)
                std::cout << "  " << i << "  " << processor.getProgramName (i) << "\n";
            return 0;
        }

        auto sampleRate = o.sampleRate;
        const juce::File inputFile = juce::File::getCurrentWorkingDirectory().getChildFile (o.input);
        auto input = inputFile.existsAsFile() ? readFile (inputFile, sampleRate) : generateSignal (o);
        const auto numSamples = input.getNumSamples();

        if (o.preset.isNotEmpty())
        {
            auto index = o.preset.containsOnly ("0123456789") ? o.preset.getIntValue() : -1;
            for (int i = 0; i < processor.getNumPrograms() && index < 0; ++i)
                if (processor.getProgramName (i).equalsIgnoreCase (o.preset))
                    index = i;
            if (index < 0 || index >= processor.getNumPrograms())
                throw std::runtime_error ("unknown preset " + o.preset.toStdString() + " (see --list)");
            processor.setCurrentProgram (index);
        }

        for (const auto& id : o.params.getAllKeys())
        {
            auto* param = findParameter (processor, id);
            if (param == nullptr)
                throw std::runtime_error ("unknown parameter " + id.toStdString() + " (see --list)");
            param->setValueNotifyingHost (param->convertTo0to1 (o.params[id].getFloatValue()));
        }

        if (! o.notes.isEmpty())
        {
            ChordSlots::Notes chord;
            for (auto n : o.notes)
                if (n >= 0 && n < 128)
                    chord[(size_t) n] = true;
            processor.getChordSlots().setPiano (chord);
        }

        processor.setPlayConfigDetails (2, 2, sampleRate, o.blockSize);
        processor.prepareToPlay (sampleRate, o.blockSize);

        juce::AudioBuffer<float> block (2, o.blockSize);
        juce::MidiBuffer midi;
        double processSeconds = 0.0;

        for (int start = 0; start < numSamples; start += o.blockSize)
        {
            const auto len = std::min (o.blockSize, numSamples - start);
            block.setSize (2, len, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, input, ch, start, len);

            midi.clear();
            const auto t0 = juce::Time::getHighResolutionTicks();
            processor.processBlock (block, midi);
            processSeconds += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);

            for (int ch = 0; ch < 2; ++ch)
                input.copyFrom (ch, start, block, ch, 0, len); // render in place
        }

        processor.releaseResources();

        // Stats
        bool nonFinite = false;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                nonFinite |= (std::bit_cast<std::uint32_t> (input.getSample (ch, i)) & 0x7f800000u) == 0x7f800000u; // survives -ffast-math

        const auto audioSeconds = numSamples / sampleRate;
        const auto peak = input.getMagnitude (0, numSamples);
        const auto rms = std::max (input.getRMSLevel (0, 0, numSamples), input.getRMSLevel (1, 0, numSamples));

        std::cout << "sample rate   " << sampleRate << " Hz, block " << o.blockSize << ", " << audioSeconds << " s\n"
                  << "latency       " << processor.getLatencySamples() << " samples\n"
                  << "cpu           " << juce::String (100.0 * processSeconds / audioSeconds, 3) << "% of realtime ("
                  << juce::String (1.0e6 * processSeconds / std::ceil ((double) numSamples / o.blockSize), 2) << " us/block)\n"
                  << "output peak   " << juce::Decibels::gainToDecibels (peak, -120.0f) << " dBFS, rms "
                  << juce::Decibels::gainToDecibels (rms, -120.0f) << " dBFS\n";

        if (nonFinite)
            std::cout << "WARNING: output contains NaN/inf\n";

        // Write 32-bit float so levels above 0 dBFS are preserved for inspection
        const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile (o.out);
        outFile.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = outFile.createOutputStream();
        if (stream == nullptr)
            throw std::runtime_error ("could not open " + outFile.getFullPathName().toStdString());

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                       .withSampleRate (sampleRate)
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (32)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (input, 0, numSamples))
            throw std::runtime_error ("could not write " + outFile.getFullPathName().toStdString());

        std::cout << "wrote         " << outFile.getFullPathName() << "\n";
        return nonFinite ? 2 : 0;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;

    try
    {
        const auto options = parseArgs (juce::StringArray (argv + 1, argc - 1));
        if (! options)
        {
            std::cout << usage;
            return 0;
        }
        return run (*options);
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n\n" << usage;
        return 1;
    }
}

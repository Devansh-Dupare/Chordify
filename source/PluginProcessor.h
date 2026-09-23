#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ChordMapper.h"
#include "dsp/Exciter.h"
#include "dsp/ResonatorBank.h"
#include "dsp/ToneFilters.h"
#include "params/Parameters.h"
#include "params/Presets.h"
#include "state/ChordSlots.h"
#include <bitset>

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameterTree() { return parameters; }

    // The piano selection and saved chord slots; the Chord Slot parameter picks which one plays
    ChordSlots& getChordSlots() { return chordSlots; }

    // The chord the audio thread is currently playing. Safe to call from any thread.
    std::bitset<128> getChordNotes() const;

    // Output peak per channel since the last call (resets it). For meters on the message thread.
    std::array<float, 2> takeOutputPeaks();

private:
    void updateEngine();

    juce::AudioProcessorValueTreeState parameters { *this, nullptr, "Chordify", params::createLayout() };

    // Last factory preset chosen, saved with the state so hosts show the right name
    static inline const juce::Identifier programProperty { "program" };
    int currentProgram = 0;

    struct ParameterValues
    {
        explicit ParameterValues (juce::AudioProcessorValueTreeState&);
        std::atomic<float>& chordSlot;
        std::atomic<float>& harmonics;
        std::atomic<float>& detune;
        std::atomic<float>& spread;
        std::atomic<float>& glide;
        std::atomic<float>& decay;
        std::atomic<float>& excite;
        std::atomic<float>& brightness;
        std::atomic<float>& timbre;
        std::atomic<float>& mix;
        std::atomic<float>& inputHpf;
        std::atomic<float>& tone;
        std::atomic<float>& output;
    };
    ParameterValues parameterValues { parameters };

    ChordSlots chordSlots;
    chordify::ResonatorBank resonatorBank;

    chordify::InputHighPass inputHighPass;
    chordify::Exciter exciter;
    chordify::TiltEq tiltEq;
    chordify::Voices chordVoices {}; // voices of the chord playing now (released ones ring out)

    // Partials are only recomputed when the chord or voicing actually changes
    struct PartialInputs
    {
        chordify::Voices voices;
        chordify::PartialSettings settings;
        bool operator== (const PartialInputs&) const = default;
    };
    std::optional<PartialInputs> lastPartialInputs;

    double currentSampleRate = 48000.0;
    juce::AudioBuffer<float> monoInput, wetOutput;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGainSmoothed;

    std::atomic<std::uint64_t> playingChord { ChordSlots::pack ({}) }; // packed like ChordSlots
    std::array<std::atomic<float>, 2> outputPeaks {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

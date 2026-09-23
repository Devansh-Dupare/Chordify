#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ChordMapper.h"
#include "dsp/ResonatorBank.h"
#include "params/Parameters.h"
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

    // Notes currently held on the incoming MIDI stream. Safe to call from any thread.
    std::bitset<128> getHeldNotes() const;

private:
    void updateHeldNotes (const juce::MidiBuffer& midi);
    void handleMidiEvent (const juce::MidiMessage& message);
    void updateEngine();
    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int numSamples);

    juce::AudioProcessorValueTreeState parameters { *this, nullptr, "Chordify", params::createLayout() };

    struct ParameterValues
    {
        explicit ParameterValues (juce::AudioProcessorValueTreeState&);
        std::atomic<float>& chordSource;
        std::atomic<float>& root;
        std::atomic<float>& chordType;
        std::atomic<float>& harmonics;
        std::atomic<float>& detune;
        std::atomic<float>& spread;
        std::atomic<float>& glide;
        std::atomic<float>& decay;
        std::atomic<float>& brightness;
        std::atomic<float>& timbre;
        std::atomic<float>& mix;
    };
    ParameterValues parameterValues { parameters };

    // The Spectral engine arrives in Phase 4; until then both modes use the resonator bank
    chordify::ResonatorBank resonatorBank;
    chordify::Engine* engine = &resonatorBank;

    chordify::VoiceAllocator midiVoices;
    chordify::Voices internalVoices {};

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

    // Written by the audio thread, read by the UI: bit n of word n / 64 is MIDI note n
    std::array<std::atomic<std::uint64_t>, 2> heldNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

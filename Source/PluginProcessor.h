#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Params.h"
#include "Presets.h"

class PhatFXProcessor : public juce::AudioProcessor
{
public:
    PhatFXProcessor();
    ~PhatFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return std::max (0, currentPreset); }
    void setCurrentProgram (int index) override { loadPreset (currentBank, index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Original preset banks found in the user's CamelPhatData folder.
    const std::vector<phat::PresetBank>& getBanks() const { return banks; }
    void loadPreset (int bank, int index);
    void stepPreset (int delta);
    int getCurrentBank() const { return currentBank; }
    int getCurrentPresetIndex() const { return currentPreset; }
    juce::String getCurrentPresetName() const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor (message thread) for meters / LFO lights.
    std::atomic<float> meterL { 0 }, meterR { 0 }, lfoOut1 { 0 }, lfoOut2 { 0 };

    // Randomise every continuous control and the type/shape choices.
    void randomize();

private:
    phat::Engine engine;
    std::array<std::atomic<float>*, phat::P_COUNT> raw {};
    juce::AudioBuffer<float> monoScratch;
    std::vector<phat::PresetBank> banks;
    int currentBank = 0, currentPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhatFXProcessor)
};

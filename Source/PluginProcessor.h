#pragma once
#include <JuceHeader.h>
#include "DSP/Compressor.h"
#include "License/NFLicenseManager.h"

class NFGlueAudioProcessor final : public juce::AudioProcessor
{
public:
    NFLicenseManager licenseManager { "NF_GLUE" };
    NFGlueAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState apvts;
    // Current gain reduction in dB (>= 0), read by the editor's GR meter.
    std::atomic<float> gainReductionDb { 0.0f };

private:
    nfglue::Compressor compressor;
    std::atomic<float> *thresholdParam = nullptr, *ratioParam = nullptr, *attackParam = nullptr,
                       *releaseParam = nullptr, *ratio2xParam = nullptr, *outputGainParam = nullptr, *powerParam = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainLinear;
    bool wasPowered = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFGlueAudioProcessor)
};

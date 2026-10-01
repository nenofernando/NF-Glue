#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Output is a makeup-gain knob: 0 dB (far left) up to +24 dB.
float outputDbToGain(float db) noexcept { return juce::Decibels::decibelsToGain(juce::jmax(0.0f, db)); }
}

NFGlueAudioProcessor::NFGlueAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      apvts(*this,nullptr,"NF_GLUE_STATE",createParameters())
{
    thresholdParam = apvts.getRawParameterValue("threshold");
    ratioParam = apvts.getRawParameterValue("ratio");
    attackParam = apvts.getRawParameterValue("attack");
    releaseParam = apvts.getRawParameterValue("release");
    ratio2xParam = apvts.getRawParameterValue("ratio2x");
    outputGainParam = apvts.getRawParameterValue("outputGain");
    powerParam = apvts.getRawParameterValue("power");
}

void NFGlueAudioProcessor::prepareToPlay(double sr,int)
{
    compressor.prepare(sr);
    outputGainLinear.reset(sr, 0.020);
    outputGainLinear.setCurrentAndTargetValue(outputDbToGain(outputGainParam->load()));
    wasPowered = true;
}

bool NFGlueAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo())&&in==out;
}

void NFGlueAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    struct LicenseMuteGuard { NFLicenseManager& lm; juce::AudioBuffer<float>& b; ~LicenseMuteGuard(){ if(!lm.isActivated()) b.clear(); } } licenseGuard{licenseManager, buffer};

    outputGainLinear.setTargetValue(outputDbToGain(outputGainParam->load()));

    const bool powered = powerParam->load() > 0.5f;
    if (!powered) { if (wasPowered) compressor.reset(); wasPowered = false; gainReductionDb.store(0.0f); return; }
    wasPowered = true;

    nfglue::Parameters p;
    p.thresholdDb = thresholdParam->load();
    p.ratio = (double) nfglue::kRatioSteps[juce::jlimit(0, nfglue::kNumRatioSteps - 1, juce::roundToInt(ratioParam->load()))];
    p.attackMs = attackParam->load();
    p.releaseMs = releaseParam->load() * 1000.0; // parameter is in seconds
    p = nfglue::applyBoost(p, ratio2xParam->load() > 0.5f); // "2x": double ratio + lower threshold
    compressor.setParameters(p);

    const int numCh = std::min(2, buffer.getNumChannels());
    if (numCh <= 0) return;
    auto* l = buffer.getWritePointer(0);
    auto* r = numCh > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float left = l[n], right = r != nullptr ? r[n] : l[n];
        compressor.processSample(left, right);
        const float gain = outputGainLinear.getNextValue();
        l[n] = left * gain;
        if (r != nullptr) r[n] = right * gain;
    }
    gainReductionDb.store((float) compressor.gainReductionDb());
}

juce::AudioProcessorValueTreeState::ParameterLayout NFGlueAudioProcessor::createParameters()
{
    using ID=juce::ParameterID;std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    // Threshold -40..0 dB (centre -20 dB at 12 o'clock) and Ratio as fixed steps (2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 20).
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"threshold",1},"Threshold",juce::NormalisableRange<float>(-40.0f,0.0f,1.0f),-20.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    juce::StringArray ratioNames;
    for (int r : nfglue::kRatioSteps) ratioNames.add(juce::String(r) + ":1");
    p.push_back(std::make_unique<juce::AudioParameterChoice>(ID{"ratio",1},"Ratio",ratioNames,nfglue::kDefaultRatioIndex));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"ratio2x",1},"Ratio 2x",false));
    // Attack 0.5-10 ms and Release 0.25-2.5 s, with the knob's centre (12 o'clock) at 5.5 ms / 1.25 s.
    juce::NormalisableRange<float> attackRange(0.5f,10.0f,0.1f); attackRange.setSkewForCentre(5.5f);
    juce::NormalisableRange<float> releaseRange(0.25f,2.5f,0.01f); releaseRange.setSkewForCentre(1.25f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"attack",1},"Attack",attackRange,5.5f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"release",1},"Release",releaseRange,1.25f,
        juce::AudioParameterFloatAttributes().withLabel("s")));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"power",1},"Power",true));
    // Output (makeup gain): 0 dB at the far left up to +24 dB.
    juce::NormalisableRange<float> outputRange(0.0f,24.0f,0.1f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"outputGain",1},"Output",
        outputRange,0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    return {p.begin(),p.end()};
}

void NFGlueAudioProcessor::getStateInformation(juce::MemoryBlock& dest){if(auto xml=apvts.copyState().createXml())copyXmlToBinary(*xml,dest);}
void NFGlueAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size);xml&&xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* NFGlueAudioProcessor::createEditor(){return new NFGlueAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NFGlueAudioProcessor();}

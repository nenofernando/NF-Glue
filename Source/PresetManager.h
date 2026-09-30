#pragma once
#include <JuceHeader.h>

namespace nfglue
{
// Presets are just the existing APVTS state (same XML as DAW session recall) plus two
// validation attributes. Same scheme as NF Q3.
struct PresetManager
{
    static juce::File getPresetsDirectory();
    static juce::Result savePreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
    static void applyFactoryPreset(juce::AudioProcessorValueTreeState& apvts, int index);
    static juce::Result loadPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
};
}

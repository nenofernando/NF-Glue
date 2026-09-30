#pragma once
// Factory presets for NF Glue. JUCE-free so the unit tests can validate the values.
// They are starting points (not tuned by ear): Output holds a rough makeup gain.
namespace nfglue
{
struct FactoryPreset
{
    const char* name;
    float thresholdDb;  // -40..0
    int   ratioIndex;   // 0..10 -> 2,3,4,5,6,7,8,10,12,15,20 (:1), see nfglue::kRatioSteps
    float attackMs;     // 0.5..10
    float releaseSec;   // 0.25..2.5
    float outputDb;     // 0..24 (makeup gain)
};

inline constexpr FactoryPreset kFactoryPresets[] = {
    { "Drums",           -18.0f, 6, 10.0f, 0.25f, 2.0f },
    { "Vocals",          -22.0f, 1,  5.5f, 0.50f, 3.0f },
    { "Vocal Rock",      -26.0f, 6,  3.0f, 0.40f, 4.0f },
    { "Vocal Metal",     -30.0f, 8,  1.5f, 0.30f, 5.0f },
    { "Vocal Podcast",   -24.0f, 1,  4.0f, 0.80f, 4.0f },
    { "Vocal Power",     -28.0f, 8,  2.0f, 0.35f, 5.0f },
    { "Guitars",         -20.0f, 2,  8.0f, 0.50f, 2.0f },
    { "Strings",         -16.0f, 0, 10.0f, 1.50f, 1.0f },
    { "Mix Glue",        -14.0f, 0, 10.0f, 0.60f, 1.5f },
    { "Mix Glue 2",      -18.0f, 1,  6.0f, 1.00f, 2.5f },
    { "Bass",            -20.0f, 2,  6.0f, 0.40f, 2.0f },
    { "Acoustic Guitar", -18.0f, 0,  7.0f, 0.70f, 1.5f },
    { "Piano",           -16.0f, 0,  8.0f, 1.20f, 1.0f },
    { "Synths",          -20.0f, 2,  4.0f, 0.50f, 2.0f },
    { "Master Bus",      -10.0f, 0, 10.0f, 1.00f, 1.0f },
};
inline constexpr int kNumFactoryPresets = (int)(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
}

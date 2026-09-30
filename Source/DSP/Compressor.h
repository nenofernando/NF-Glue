#pragma once
#include <algorithm>
#include <cmath>

// NF Glue compressor core. Header-only and JUCE-free so the unit tests can build it alone.
// Panel controls: Threshold, Ratio, Attack, Release (makeup is the Output fader).
namespace nfglue
{
constexpr double kKneeDb = 6.0;

// Ratio knob positions (X:1). The knob is a stepped choice; index 2 (4:1) is the default.
inline constexpr int kRatioSteps[] = { 2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 20 };
inline constexpr int kNumRatioSteps = (int)(sizeof(kRatioSteps) / sizeof(kRatioSteps[0]));
inline constexpr int kDefaultRatioIndex = 2;

struct Parameters
{
    double thresholdDb = -20.0; // -40..0
    double ratio       = 4.0;   // 1.5..20
    double attackMs    = 5.5;   // 0.5..10
    double releaseMs   = 1250.0;// 250..2500
};

// "2x" button: heavier compression at any ratio. Doubling the ratio alone is barely audible at high
// ratios (8:1 -> 16:1), so 2x also lowers the threshold by kBoostThresholdDropDb.
constexpr double kBoostThresholdDropDb = 6.0;
inline Parameters applyBoost(Parameters p, bool on)
{
    if (on) { p.ratio *= 2.0; p.thresholdDb -= kBoostThresholdDropDb; }
    return p;
}

// Soft-knee static curve: returns the gain change in dB (<= 0) for an input level in dB.
inline double gainChangeDb(double inputDb, double thresholdDb, double ratio)
{
    const double over = inputDb - thresholdDb;
    double outDb = inputDb;
    if (2.0 * over > kKneeDb)
        outDb = thresholdDb + over / ratio;
    else if (2.0 * over >= -kKneeDb)
    {
        const double x = over + kKneeDb * 0.5;
        outDb = inputDb + (1.0 / ratio - 1.0) * x * x / (2.0 * kKneeDb);
    }
    return outDb - inputDb;
}

class Compressor
{
public:
    void prepare(double sampleRate) { sr = sampleRate; reset(); setParameters(params); }
    void reset() { reductionDb = 0.0; }

    void setParameters(const Parameters& p)
    {
        params = p;
        ratio = std::max(1.0, p.ratio);
        attackCoeff  = std::exp(-1.0 / (std::max(0.05, p.attackMs)  * 0.001 * sr));
        releaseCoeff = std::exp(-1.0 / (std::max(1.0,  p.releaseMs) * 0.001 * sr));
    }

    // Stereo-linked: both channels get the same gain. For mono pass the same value twice
    // and only use the left result.
    void processSample(float& left, float& right)
    {
        const double level = std::max(std::abs((double) left), std::abs((double) right));
        const double inDb  = 20.0 * std::log10(std::max(level, 1.0e-9));
        const double target = -gainChangeDb(inDb, params.thresholdDb, ratio); // >= 0
        const double c = target > reductionDb ? attackCoeff : releaseCoeff;
        reductionDb = c * reductionDb + (1.0 - c) * target;
        const auto gain = (float) std::pow(10.0, -reductionDb / 20.0);
        left *= gain; right *= gain;
    }

    double gainReductionDb() const { return reductionDb; }

private:
    Parameters params;
    double sr = 44100.0, ratio = 2.0, attackCoeff = 0.0, releaseCoeff = 0.0, reductionDb = 0.0;
};
}

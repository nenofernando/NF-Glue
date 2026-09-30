#include "../Source/DSP/Compressor.h"
#include "../Source/FactoryPresets.h"
#include <cstring>
#include <cassert>
#include <iostream>

static double runTone(nfglue::Compressor& c, double sr, double ampDb, double seconds, float& lastL)
{
    const int n = (int)(sr * seconds);
    const float a = (float) std::pow(10.0, ampDb / 20.0);
    for (int i = 0; i < n; ++i)
    {
        float l = a, r = a;
        c.processSample(l, r);
        lastL = l;
    }
    return c.gainReductionDb();
}

int main()
{
    using namespace nfglue;
    // Ratio steps: 11 increasing values from 2:1 to 20:1, default is 4:1.
    assert(kNumRatioSteps == 11 && kRatioSteps[0] == 2 && kRatioSteps[kNumRatioSteps - 1] == 20 && kRatioSteps[kDefaultRatioIndex] == 4);
    for (int i = 1; i < kNumRatioSteps; ++i) assert(kRatioSteps[i] > kRatioSteps[i - 1]);

    // Static curve: below threshold nothing happens, far above it follows the ratio.
    assert(std::abs(gainChangeDb(-40.0, -20.0, 4.0)) < 1e-12);
    assert(std::abs(gainChangeDb(0.0, -20.0, 4.0) - (-20.0 + 20.0 / 4.0)) < 1e-9);
    assert(gainChangeDb(-20.0, -20.0, 4.0) < 0.0 && gainChangeDb(-20.0, -20.0, 4.0) > -1.0); // gentle soft knee at threshold
    assert(std::abs(gainChangeDb(-10.0, -20.0, 1.0)) < 1e-12);                               // 1:1 = transparent
    // Lower threshold = more reduction; higher ratio = more reduction.
    assert(gainChangeDb(-10.0, -30.0, 4.0) < gainChangeDb(-10.0, -20.0, 4.0));
    assert(gainChangeDb(-10.0, -20.0, 8.0) < gainChangeDb(-10.0, -20.0, 2.0));

    // "2x" doubles the ratio: 20:1 becomes 40:1 and reduces more; 2:1 -> 4:1 matches the plain 4:1 curve.
    assert(gainChangeDb(0.0, -20.0, 40.0) < gainChangeDb(0.0, -20.0, 20.0));
    assert(std::abs(gainChangeDb(-5.0, -20.0, 2.0 * 2.0) - gainChangeDb(-5.0, -20.0, 4.0)) < 1e-12);

    // 2x = double ratio + threshold 6 dB lower: clearly heavier at every ratio (>= 3 dB extra for a -6 dB tone
    // with threshold -20 up to 20:1), and off = untouched.
    for (int r : kRatioSteps)
    {
        Parameters base; base.thresholdDb = -20.0; base.ratio = r;
        const auto boosted = applyBoost(base, true);
        assert(boosted.ratio == 2.0 * r && boosted.thresholdDb == -26.0);
        assert(applyBoost(base, false).ratio == base.ratio && applyBoost(base, false).thresholdDb == base.thresholdDb);
        const double extra = gainChangeDb(-6.0, base.thresholdDb, base.ratio) - gainChangeDb(-6.0, boosted.thresholdDb, boosted.ratio);
        assert(extra >= 3.0);
    }

    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        // Signal below the threshold is untouched.
        Compressor c; Parameters p; c.prepare(sr); c.setParameters(p);
        float last = 0; runTone(c, sr, -40.0, 0.5, last);
        assert(std::abs(last - std::pow(10.0, -40.0 / 20.0)) < 1e-6f);

        // Loud signal settles at the static-curve reduction.
        p.thresholdDb = -30.0; p.ratio = 8.0; p.attackMs = 0.5; p.releaseMs = 250; c.setParameters(p);
        const double gr = runTone(c, sr, 0.0, 1.0, last);
        assert(std::abs(gr + gainChangeDb(0.0, -30.0, 8.0)) < 0.05);
        assert(std::isfinite(last) && last < 1.0f);

        // Threshold at 0 dB: a full-scale signal is barely touched (only the soft knee).
        Compressor t; Parameters pt; pt.thresholdDb = 0.0; t.prepare(sr); t.setParameters(pt);
        assert(runTone(t, sr, -6.0, 0.5, last) < 0.01);

        // Attack: faster attack reduces more within the same short window.
        Compressor fast, slow; Parameters pf, ps; pf.attackMs = 0.5; ps.attackMs = 10.0;
        fast.prepare(sr); fast.setParameters(pf); slow.prepare(sr); slow.setParameters(ps);
        assert(runTone(fast, sr, 0.0, 0.002, last) > runTone(slow, sr, 0.0, 0.002, last));

        // Release: after the loud burst ends, slow release keeps more reduction than fast.
        Compressor rf, rs; Parameters a, b; a.releaseMs = 250; b.releaseMs = 2500;
        rf.prepare(sr); rf.setParameters(a); rs.prepare(sr); rs.setParameters(b);
        runTone(rf, sr, 0.0, 0.5, last); runTone(rs, sr, 0.0, 0.5, last);
        assert(runTone(rs, sr, -60.0, 0.3, last) > runTone(rf, sr, -60.0, 0.3, last));
    }

    // Factory presets: 15 of them, unique names, every value inside its knob's range.
    assert(nfglue::kNumFactoryPresets == 15);
    for (int i = 0; i < nfglue::kNumFactoryPresets; ++i)
    {
        const auto& f = nfglue::kFactoryPresets[i];
        assert(f.thresholdDb >= -40.0f && f.thresholdDb <= 0.0f);
        assert(f.ratioIndex >= 0 && f.ratioIndex < nfglue::kNumRatioSteps);
        assert(f.attackMs >= 0.5f && f.attackMs <= 10.0f);
        assert(f.releaseSec >= 0.25f && f.releaseSec <= 2.5f);
        assert(f.outputDb >= 0.0f && f.outputDb <= 24.0f);
        for (int j = i + 1; j < nfglue::kNumFactoryPresets; ++j)
            assert(std::strcmp(f.name, nfglue::kFactoryPresets[j].name) != 0);
    }

    // Silence and stereo link.
    Compressor c; c.prepare(48000.0);
    float l = 0.f, r = 0.f; c.processSample(l, r); assert(l == 0.f && r == 0.f);
    float ll = 1.0f, rr = 0.1f; for (int i = 0; i < 48000; ++i) { ll = 1.0f; rr = 0.1f; c.processSample(ll, rr); }
    assert(std::abs(rr / 0.1f - ll / 1.0f) < 1e-6f);

    std::cout << "NF Glue DSP tests passed\n";
}

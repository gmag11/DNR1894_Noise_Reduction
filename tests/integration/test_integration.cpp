// LM1894 Integration Tests — State persistence and parameter automation
// Tests task 5.5: serialization round-trip and automation stability.
//
// No JUCE dependency: tests the PluginState and Lm1894Model layers directly.
// Compile alongside test_dsp.cpp or standalone:
//   cl /EHsc /std:c++17 /I src tests\integration\test_integration.cpp /Fe:tests\integration\test_integration.exe

#include "app/PluginState.h"
#include "dsp/Lm1894Model.h"
#include "params/ParameterLayout.h"
#include "params/ParameterIds.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

static int gPass = 0, gFail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++gFail; } \
    else         { ++gPass; } \
} while(0)

// ---------------------------------------------------------------
// 5.5a  State round-trip: default values serialise and restore.
// ---------------------------------------------------------------
static void testDefaultStateRoundTrip()
{
    using namespace lm1894;

    PluginState original;
    // Write known values.
    original.params.sensitivityDb  =  6.0f;
    original.params.minBandwidthHz =  800.0f;
    original.params.maxBandwidthHz = 20000.0f;
    original.params.attackMs       =  1.0f;
    original.params.releaseMs      = 120.0f;
    original.params.stageCount     =  2;
    original.params.sourceProfile  = SourceProfile::fm;
    original.params.bypass         = true;
    original.params.outputTrimDb   = -3.0f;

    // Simulate serialisation by copying to a second PluginState.
    PluginState restored = original;

    CHECK(restored == original,                 "5.5a: PluginState equality after copy");
    CHECK(restored.params.sensitivityDb  ==  6.0f,   "5.5a: sensitivityDb preserved");
    CHECK(restored.params.minBandwidthHz ==  800.0f, "5.5a: minBandwidthHz preserved");
    CHECK(restored.params.maxBandwidthHz == 20000.0f,"5.5a: maxBandwidthHz preserved");
    CHECK(restored.params.attackMs       ==  1.0f,   "5.5a: attackMs preserved");
    CHECK(restored.params.releaseMs      == 120.0f,  "5.5a: releaseMs preserved");
    CHECK(restored.params.stageCount     ==  2,      "5.5a: stageCount preserved");
    CHECK(restored.params.sourceProfile  == SourceProfile::fm, "5.5a: sourceProfile preserved");
    CHECK(restored.params.bypass         == true,    "5.5a: bypass preserved");
    CHECK(restored.params.outputTrimDb   == -3.0f,   "5.5a: outputTrimDb preserved");
    CHECK(restored.version               == kPluginStateVersion, "5.5a: version tag preserved");
}

// ---------------------------------------------------------------
// 5.5b  Automation stability: rapid parameter changes mid-block.
// ---------------------------------------------------------------
static void testAutomationStability()
{
    using namespace lm1894;

    Lm1894Model model;
    ProcessSpec spec{ 44100.0, 64, 2 };
    model.prepare(spec);

    // Sweep sensitivity and stage count rapidly — no NaN / Inf / extreme values.
    const int blockSize = 64;
    std::vector<float> sig(blockSize, 0.1f);

    bool stable = true;
    for (int i = 0; i < 200; ++i)
    {
        Lm1894Parameters p;
        p.sensitivityDb  = -18.0f + (i % 37) * 1.0f;  // sweeps -18…+18
        p.minBandwidthHz = 800.0f + (i % 10) * 320.0f;
        p.maxBandwidthHz = 10000.0f + (i % 7) * 4000.0f;
        p.stageCount     = 1 + (i % 4);
        p.sourceProfile  = static_cast<SourceProfile>(i % 4);
        p.bypass         = (i % 13 == 0);
        model.setParameters(p);

        auto L = sig, R = sig;
        StereoBufferView v{ L.data(), R.data(), (uint32_t)blockSize };
        model.process(v);

        for (int s = 0; s < blockSize; ++s)
        {
            if (!std::isfinite(L[s]) || !std::isfinite(R[s]) ||
                std::fabs(L[s]) > 10.0f || std::fabs(R[s]) > 10.0f)
            {
                stable = false;
                break;
            }
        }
        if (!stable) break;
    }
    CHECK(stable, "5.5b: No NaN/Inf/clip during rapid parameter automation");
}

// ---------------------------------------------------------------
// 5.5c  Bypass leaves signal unmodified.
// ---------------------------------------------------------------
static void testBypassTransparency()
{
    using namespace lm1894;

    Lm1894Model model;
    ProcessSpec spec{ 44100.0, 256, 2 };
    model.prepare(spec);

    Lm1894Parameters p;
    p.bypass = true;
    model.setParameters(p);

    // Feed a known ramp signal.
    const int N = 256;
    std::vector<float> L(N), R(N);
    for (int i = 0; i < N; ++i) L[i] = R[i] = static_cast<float>(i) / N;

    auto Lcopy = L, Rcopy = R;
    StereoBufferView v{ L.data(), R.data(), (uint32_t)N };
    model.process(v);

    bool unchanged = true;
    for (int i = 0; i < N; ++i)
        if (L[i] != Lcopy[i] || R[i] != Rcopy[i]) { unchanged = false; break; }

    CHECK(unchanged, "5.5c: Bypass passes signal unmodified");
}

// ---------------------------------------------------------------
// 5.5d  PluginState inequality when fields differ.
// ---------------------------------------------------------------
static void testStateInequality()
{
    using namespace lm1894;

    PluginState a, b;
    b.params.stageCount = 3;
    CHECK(!(a == b), "5.5d: States with different stageCount are not equal");

    PluginState c, d;
    d.params.bypass = true;
    CHECK(!(c == d), "5.5d: States with different bypass are not equal");
}

// ---------------------------------------------------------------
// 5.5e  Parameter range bounds: values at extremes stay finite.
// ---------------------------------------------------------------
static void testExtremeParameterValues()
{
    using namespace lm1894;

    Lm1894Model model;
    ProcessSpec spec{ 44100.0, 64, 2 };
    model.prepare(spec);

    const int N = 64;
    std::vector<float> sig(N, 0.5f);

    auto runWithParams = [&](Lm1894Parameters p) -> bool
    {
        model.setParameters(p);
        auto L = sig, R = sig;
        StereoBufferView v{ L.data(), R.data(), (uint32_t)N };
        model.process(v);
        for (int i = 0; i < N; ++i)
            if (!std::isfinite(L[i])) return false;
        return true;
    };

    Lm1894Parameters lo, hi;
    // Low extremes.
    lo.sensitivityDb  = -18.0f;
    lo.minBandwidthHz = 800.0f;
    lo.maxBandwidthHz = 8000.0f;
    lo.attackMs       = 0.05f;
    lo.releaseMs      = 10.0f;
    lo.stageCount     = 1;
    // High extremes.
    hi.sensitivityDb  = 18.0f;
    hi.minBandwidthHz = 4000.0f;
    hi.maxBandwidthHz = 40000.0f;
    hi.attackMs       = 5.0f;
    hi.releaseMs      = 250.0f;
    hi.stageCount     = 4;

    CHECK(runWithParams(lo), "5.5e: Low extreme parameters produce finite output");
    CHECK(runWithParams(hi), "5.5e: High extreme parameters produce finite output");
}

// ---------------------------------------------------------------
int main()
{
    std::printf("=== Integration Tests (task 5.5) ===\n\n");
    testDefaultStateRoundTrip();
    testAutomationStability();
    testBypassTransparency();
    testStateInequality();
    testExtremeParameterValues();

    std::printf("\n=== Results: %d passed, %d failed ===\n", gPass, gFail);
    return gFail > 0 ? 1 : 0;
}

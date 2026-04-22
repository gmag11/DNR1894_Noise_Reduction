// LM1894 DSP Unit Tests
// Standalone test runner — no framework dependency.
// Compile: cl /EHsc /std:c++17 /I../../src tests/dsp/test_dsp.cpp /Fe:test_dsp.exe
//          or g++ -std=c++17 -I../../src tests/dsp/test_dsp.cpp -o test_dsp -lm

#include "dsp/Lm1894Model.h"
#include "dsp/SidechainDetector.h"
#include "dsp/VariableLpStage.h"
#include "dsp/CascadedFilterBank.h"
#include "params/ParameterLayout.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int gPass = 0, gFail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++gFail; } \
    else { ++gPass; } \
} while(0)

#define APPROX(a, b, tol) (std::fabs((a) - (b)) < (tol))

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
static std::vector<float> generateSine(float freq, double sr, int samples, float amp = 0.5f)
{
    std::vector<float> buf(samples);
    for (int i = 0; i < samples; ++i)
        buf[i] = amp * std::sin(2.0f * 3.14159265f * freq * i / static_cast<float>(sr));
    return buf;
}

static std::vector<float> generateWhiteNoise(int samples, float amp = 0.3f)
{
    std::vector<float> buf(samples);
    // Simple LCG for reproducibility.
    uint32_t seed = 42;
    for (int i = 0; i < samples; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        buf[i] = amp * (static_cast<float>(seed) / 2147483648.0f - 1.0f);
    }
    return buf;
}

static float rmsLevel(const float* data, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += data[i] * data[i];
    return std::sqrt(static_cast<float>(sum / n));
}

// ---------------------------------------------------------------
// 5.1  Attack, release and spectral response
// ---------------------------------------------------------------
static void testEnvelopeTiming()
{
    using namespace lm1894;
    Lm1894Model model;
    ProcessSpec spec{44100.0, 64, 2};
    model.prepare(spec);

    Lm1894Parameters p;
    p.attackMs  = 0.5f;
    p.releaseMs = 60.0f;
    p.sensitivityDb = 0.0f;
    p.minBandwidthHz = 1000.0f;
    p.maxBandwidthHz = 35000.0f;
    p.stageCount = 1;
    p.bypass = false;
    model.setParameters(p);

    const int blockSize = 64;
    const int sr = 44100;

    // Feed silence to settle, then a burst.
    auto silence = std::vector<float>(blockSize, 0.0f);
    auto burst   = generateSine(1000.0f, sr, blockSize, 0.4f);

    StereoBufferView view;

    // 500 blocks of silence (~730 ms).
    for (int b = 0; b < 500; ++b)
    {
        auto L = silence, R = silence;
        view = {L.data(), R.data(), (uint32_t)blockSize};
        model.process(view);
    }

    MeterState m;
    model.publishMeters(m);
    float openBefore = m.normalizedBandwidthOpen.load();

    // Feed burst for ~10 ms.
    for (int b = 0; b < 7; ++b)
    {
        auto L = burst, R = burst;
        view = {L.data(), R.data(), (uint32_t)blockSize};
        model.process(view);
    }
    model.publishMeters(m);
    float openAfter = m.normalizedBandwidthOpen.load();

    CHECK(openAfter > openBefore + 0.05f,
          "5.1: Detector should open on burst (attack)");

    // Feed silence again for ~200 ms and check release.
    for (int b = 0; b < 140; ++b)
    {
        auto L = silence, R = silence;
        view = {L.data(), R.data(), (uint32_t)blockSize};
        model.process(view);
    }
    model.publishMeters(m);
    float openReleased = m.normalizedBandwidthOpen.load();
    CHECK(openReleased < openAfter,
          "5.1: Detector should close during release");
}

// ---------------------------------------------------------------
// 5.2  Stereo image: bright signal on one channel
// ---------------------------------------------------------------
static void testStereoLink()
{
    using namespace lm1894;
    Lm1894Model model;
    ProcessSpec spec{44100.0, 256, 2};
    model.prepare(spec);

    Lm1894Parameters p;
    p.stageCount = 1;
    p.bypass = false;
    model.setParameters(p);

    const int N = 256;
    auto left  = generateSine(10000.0f, 44100.0, N, 0.4f);
    auto right = std::vector<float>(N, 0.0f); // silence on right

    // Settle.
    for (int i = 0; i < 50; ++i)
    {
        auto L = left, R = right;
        StereoBufferView v{L.data(), R.data(), (uint32_t)N};
        model.process(v);
    }

    // Process and measure.
    auto L = left, R = right;
    StereoBufferView v{L.data(), R.data(), (uint32_t)N};
    model.process(v);

    // Right channel should still be near-silent (shared sidechain opens both channels equally).
    float rmsR = rmsLevel(R.data(), N);
    CHECK(rmsR < 0.01f,
          "5.2: Silent channel should remain silent with stereo link");
}

// ---------------------------------------------------------------
// 5.3  Consistency across sample rates and block sizes
// ---------------------------------------------------------------
static void testSampleRateConsistency()
{
    using namespace lm1894;

    auto runAtRate = [](double sr, int blockSz) -> float
    {
        Lm1894Model model;
        ProcessSpec spec{sr, (uint32_t)blockSz, 2};
        model.prepare(spec);
        Lm1894Parameters p;
        p.stageCount = 1;
        model.setParameters(p);

        // Feed 0.5 s of 1 kHz sine.
        int total = static_cast<int>(sr * 0.5);
        auto sig = generateSine(1000.0f, sr, total, 0.3f);
        for (int off = 0; off < total; off += blockSz)
        {
            int n = std::min(blockSz, total - off);
            auto L = std::vector<float>(sig.begin() + off, sig.begin() + off + n);
            auto R = L;
            StereoBufferView v{L.data(), R.data(), (uint32_t)n};
            model.process(v);
        }
        MeterState m;
        model.publishMeters(m);
        return m.normalizedBandwidthOpen.load();
    };

    float d44  = runAtRate(44100.0, 128);
    float d48  = runAtRate(48000.0, 256);
    float d96  = runAtRate(96000.0, 64);

    CHECK(APPROX(d44, d48, 0.15f), "5.3: 44.1k vs 48k detector within tolerance");
    CHECK(APPROX(d44, d96, 0.15f), "5.3: 44.1k vs 96k detector within tolerance");
}

// ---------------------------------------------------------------
// 5.4  Cascade stages: more attenuation at HF, correct compensation
// ---------------------------------------------------------------
static void testCascadeStages()
{
    using namespace lm1894;

    auto measureHfAtten = [](int stages) -> float
    {
        Lm1894Model model;
        ProcessSpec spec{44100.0, 256, 2};
        model.prepare(spec);
        Lm1894Parameters p;
        p.stageCount = stages;
        p.minBandwidthHz = 5000.0f;
        p.maxBandwidthHz = 5000.0f; // fixed cutoff
        p.sensitivityDb = 0.0f;
        model.setParameters(p);

        int N = 256;
        auto sig = generateSine(15000.0f, 44100.0, N, 0.5f);
        float rmsIn = rmsLevel(sig.data(), N);

        // Run several blocks to settle.
        for (int i = 0; i < 20; ++i)
        {
            auto L = sig, R = sig;
            StereoBufferView v{L.data(), R.data(), (uint32_t)N};
            model.process(v);
            if (i == 19)
            {
                float rmsOut = rmsLevel(L.data(), N);
                return 20.0f * std::log10(rmsOut / rmsIn);
            }
        }
        return 0.0f;
    };

    float atten1 = measureHfAtten(1);
    float atten2 = measureHfAtten(2);
    float atten4 = measureHfAtten(4);

    CHECK(atten2 < atten1, "5.4: 2 stages should attenuate more than 1");
    CHECK(atten4 < atten2, "5.4: 4 stages should attenuate more than 2");
}

// ---------------------------------------------------------------
// 5.7  Detector-to-cutoff mapping validation
// ---------------------------------------------------------------
static void testDetectorToCutoffMapping()
{
    using namespace lm1894;
    // We test the full model in a roundabout way: fix min/max BW and check
    // that silence (detector~0) results in minimal bandwidth (heavy filtering)
    // while loud signal opens it up.

    Lm1894Model model;
    ProcessSpec spec{44100.0, 256, 2};
    model.prepare(spec);

    Lm1894Parameters p;
    p.minBandwidthHz = 1000.0f;
    p.maxBandwidthHz = 35000.0f;
    p.stageCount = 1;
    p.sourceProfile = SourceProfile::tape;
    p.sensitivityDb = 0.0f;
    model.setParameters(p);

    int N = 256;

    // Feed silence → should converge to closed (low cutoff).
    for (int i = 0; i < 300; ++i)
    {
        auto L = std::vector<float>(N, 0.0f);
        auto R = L;
        StereoBufferView v{L.data(), R.data(), (uint32_t)N};
        model.process(v);
    }
    MeterState m;
    model.publishMeters(m);
    float closedNorm = m.normalizedBandwidthOpen.load();
    CHECK(closedNorm < 0.05f, "5.7: Silence should result in near-zero detector");

    // Feed broadband noise → should open.
    auto noise = generateWhiteNoise(N, 0.4f);
    for (int i = 0; i < 100; ++i)
    {
        auto L = noise, R = noise;
        StereoBufferView v{L.data(), R.data(), (uint32_t)N};
        model.process(v);
    }
    model.publishMeters(m);
    float openNorm = m.normalizedBandwidthOpen.load();
    CHECK(openNorm > 0.2f, "5.7: Broadband noise should open detector significantly");
}

// ---------------------------------------------------------------
// VariableLpStage basic sanity
// ---------------------------------------------------------------
static void testVariableLpStage()
{
    using namespace lm1894;
    VariableLpStage lpf;
    lpf.prepare(44100.0);
    lpf.setCutoffHz(1000.0f);

    // Feed 10 kHz sine, expect strong attenuation.
    int N = 512;
    auto sig = generateSine(10000.0f, 44100.0, N, 0.5f);
    float rmsIn = rmsLevel(sig.data(), N);
    lpf.processBlock(sig.data(), N);
    float rmsOut = rmsLevel(sig.data(), N);
    float attenDb = 20.0f * std::log10(rmsOut / rmsIn);
    CHECK(attenDb < -6.0f, "VariableLpStage: 10 kHz through 1 kHz LPF attenuated >6 dB");
}

// ---------------------------------------------------------------
// CascadedFilterBank compensation
// ---------------------------------------------------------------
static void testCascadeCompensation()
{
    using namespace lm1894;
    CascadedFilterBank fb;
    fb.prepare(44100.0, 256);
    fb.setStageCount(2);
    fb.setGlobalCutoffHz(5000.0f);

    // Just verify it runs without crashing.
    int N = 256;
    auto L = generateSine(1000.0f, 44100.0, N, 0.3f);
    auto R = L;
    fb.processBlock(L.data(), R.data(), N);
    float rms = rmsLevel(L.data(), N);
    CHECK(rms > 0.0f, "CascadedFilterBank: output not silent for 1 kHz through 5 kHz cutoff");
}

// ---------------------------------------------------------------
int main()
{
    testEnvelopeTiming();
    testStereoLink();
    testSampleRateConsistency();
    testCascadeStages();
    testDetectorToCutoffMapping();
    testVariableLpStage();
    testCascadeCompensation();

    std::printf("\n=== Results: %d passed, %d failed ===\n", gPass, gFail);
    return gFail > 0 ? 1 : 0;
}

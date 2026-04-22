#pragma once

#include "SidechainDetector.h"
#include "CascadedFilterBank.h"
#include "../params/ParameterLayout.h"
#include "../params/PresetProfiles.h"
#include "../app/MeterState.h"
#include <cmath>
#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// ProcessSpec — passed to prepare(), no JUCE dependency.
// -------------------------------------------------------------------
struct ProcessSpec
{
    double   sampleRate;
    uint32_t maximumBlockSize;
    uint32_t numChannels;  // expected: 2
};

// -------------------------------------------------------------------
// StereoBufferView — non-owning view of interleaved or split buffers.
// -------------------------------------------------------------------
struct StereoBufferView
{
    float*   left;
    float*   right;
    uint32_t numSamples;
};

// -------------------------------------------------------------------
// Lm1894Model — top-level DSP engine.
//
// Usage:
//   model.prepare(spec);
//   model.setParameters(params);
//   model.process(buffer);
//   auto m = model.getMeterValues();
// -------------------------------------------------------------------
class Lm1894Model
{
public:
    void prepare(const ProcessSpec& spec)
    {
        sr_ = spec.sampleRate;
        detector_.prepare(spec.sampleRate, spec.maximumBlockSize);
        filterBank_.prepare(spec.sampleRate, spec.maximumBlockSize);
    }

    void reset()
    {
        detector_.reset();
        filterBank_.reset();
        currentDetectorNorm_ = 0.0f;
    }

    void setParameters(const Lm1894Parameters& p)
    {
        params_ = p;

        // Apply profile sensitivity offset.
        Lm1894Parameters adjusted = p;
        adjusted.sensitivityDb += getProfileCalibration(p.sourceProfile).sensitivityOffsetDb;
        detector_.setParameters(adjusted);

        filterBank_.setStageCount(p.stageCount);
    }

    void process(StereoBufferView buf)
    {
        if (params_.bypass)
            return;  // Pass-through: leave buffers untouched.

        // --- Sidechain: derive cutoff from the *input* signal. ---
        float detNorm = detector_.processBlock(buf.left, buf.right, buf.numSamples);
        currentDetectorNorm_ = detNorm;

        // --- Map detector → global cutoff via the non-linear curve. ---
        float fcGlobal = mapDetectorToCutoff(detNorm,
                                              params_.minBandwidthHz,
                                              params_.maxBandwidthHz);

        // --- Apply cascaded filter bank. ---
        filterBank_.setGlobalCutoffHz(fcGlobal);
        filterBank_.processBlock(buf.left, buf.right, buf.numSamples);

        // --- Output trim. ---
        if (params_.outputTrimDb != 0.0f)
        {
            float gain = std::pow(10.0f, params_.outputTrimDb / 20.0f);
            for (uint32_t i = 0; i < buf.numSamples; ++i)
            {
                buf.left[i]  *= gain;
                buf.right[i] *= gain;
            }
        }
    }

    // Thread-safe snapshot for the editor.
    void publishMeters(MeterState& m) const
    {
        m.normalizedBandwidthOpen.store(currentDetectorNorm_, std::memory_order_relaxed);
        m.detectorActivity.store(currentDetectorNorm_, std::memory_order_relaxed);
        float fcGlobal = mapDetectorToCutoff(currentDetectorNorm_,
                                              params_.minBandwidthHz,
                                              params_.maxBandwidthHz);
        // Rough reduction estimate: ratio of current BW to max BW in dB.
        float reductionDb = 0.0f;
        if (params_.maxBandwidthHz > 0.0f && fcGlobal > 0.0f)
            reductionDb = 20.0f * std::log10(fcGlobal / params_.maxBandwidthHz);
        m.estimatedReductionDb.store(reductionDb, std::memory_order_relaxed);
        m.activeStageCount.store(params_.stageCount, std::memory_order_relaxed);
        m.bypass.store(params_.bypass, std::memory_order_relaxed);
    }

private:
    // Non-linear mapping: normalised detector (0–1) → cutoff in Hz.
    // Uses the shape function s(u) in log-frequency derived from the
    // reference table in the design doc.
    static float mapDetectorToCutoff(float detNorm, float fMin, float fMax)
    {
        // Anchor points: u (normalised detector) → s (normalised log-freq position).
        // Derived from v_eq table: u = (v_eq - 1.1) / 2.7
        //                          s = (log10(fc) - log10(1000)) / (log10(35000) - log10(1000))
        static constexpr float kU[] = { 0.0f,    0.0556f, 0.0926f, 0.1481f,
                                         0.2407f, 0.3519f, 0.4815f, 0.6296f,
                                         0.7778f, 1.0f };
        static constexpr float kS[] = { 0.0f,    0.0735f, 0.1655f, 0.2577f,
                                         0.3899f, 0.5480f, 0.6993f, 0.8433f,
                                         0.9372f, 1.0f };
        static constexpr int kN = 10;

        // Clamp.
        float u = detNorm;
        if (u <= 0.0f) u = 0.0f;
        if (u >= 1.0f) u = 1.0f;

        // Piecewise-linear lookup in the shape table.
        float s = 0.0f;
        for (int i = 0; i < kN - 1; ++i)
        {
            if (u <= kU[i + 1] || i == kN - 2)
            {
                float t = (kU[i + 1] > kU[i])
                    ? (u - kU[i]) / (kU[i + 1] - kU[i])
                    : 0.0f;
                s = kS[i] + t * (kS[i + 1] - kS[i]);
                break;
            }
        }

        // Convert s to Hz via log-frequency interpolation.
        float logMin = std::log10(fMin);
        float logMax = std::log10(fMax);
        float logFc  = logMin + s * (logMax - logMin);
        return std::pow(10.0f, logFc);
    }

    double sr_ = 44100.0;
    Lm1894Parameters params_;
    SidechainDetector detector_;
    CascadedFilterBank filterBank_;
    float currentDetectorNorm_ = 0.0f;
};

} // namespace lm1894

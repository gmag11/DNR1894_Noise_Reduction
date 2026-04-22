#pragma once

#include "../params/ParameterLayout.h"
#include <cmath>
#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// SidechainDetector
//
// Stereo-summing, frequency-weighted envelope detector.
// Produces a normalised detector value (0–1) that maps to cutoff.
// -------------------------------------------------------------------
class SidechainDetector
{
public:
    void prepare(double sampleRate, uint32_t /*maxBlockSize*/)
    {
        sr_ = sampleRate;
        reset();
    }

    void reset()
    {
        hpState_[0] = hpState_[1] = 0.0;
        emphState_ = 0.0;
        envState_  = 0.0;
    }

    // Call once per parameter update.
    void setParameters(const Lm1894Parameters& p)
    {
        // Attack / release coefficients (single-pole, 90% time).
        attackCoeff_  = timeToCoeff(p.attackMs  * 0.001, sr_);
        releaseCoeff_ = timeToCoeff(p.releaseMs * 0.001, sr_);

        // Sensitivity as linear gain applied to the sidechain signal.
        sensitivityGain_ = std::pow(10.0f, p.sensitivityDb / 20.0f);

        profile_ = p.sourceProfile;

        // HPF at ~200 Hz (2nd-order via two cascaded 1-pole sections).
        hpCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979 * 200.0 / sr_);

        // Emphasis shelf: 1st-order LPF used in a difference topology.
        // Boost onset ~1.5 kHz.
        emphCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979 * 1500.0 / sr_);

        // Notch filters for FM / TV profiles.
        updateNotch();
    }

    // Process one sample pair, return normalised detector value 0–1.
    float processSample(float left, float right)
    {
        // 1. Stereo sum (unity gain per channel).
        double mono = 0.5 * (static_cast<double>(left) + static_cast<double>(right));

        // 2. High-pass: two cascaded 1-pole HP sections (~200 Hz, ~12 dB/oct LF rejection).
        hpState_[0] += hpCoeff_ * (mono - hpState_[0]);
        double hp1 = mono - hpState_[0];
        hpState_[1] += hpCoeff_ * (hp1 - hpState_[1]);
        double hp2 = hp1 - hpState_[1];

        // 3. Emphasis: boost HF by subtracting a LPF version and scaling.
        emphState_ += emphCoeff_ * (hp2 - emphState_);
        double emphasised = hp2 + 3.0 * (hp2 - emphState_); // ~+12 dB at 6 kHz

        // 4. Profile notch (FM 19 kHz / TV 15.734 kHz) — simple biquad.
        double afterNotch = emphasised;
        if (profile_ == SourceProfile::fm || profile_ == SourceProfile::tv)
            afterNotch = processNotch(emphasised);

        // 5. Sensitivity gain.
        double scaled = afterNotch * static_cast<double>(sensitivityGain_);

        // 6. Peak rectifier.
        double rectified = std::fabs(scaled);

        // 7. Envelope follower (asymmetric attack / release).
        double coeff = (rectified > envState_) ? attackCoeff_ : releaseCoeff_;
        envState_ += coeff * (rectified - envState_);

        // 8. Map envelope to 0–1.
        //    The reference range is roughly 0 … 0.5 Vrms at the control path.
        //    We normalise so that a mid-level signal sits around 0.5.
        constexpr double kRefPeak = 0.35;
        double norm = envState_ / kRefPeak;
        if (norm > 1.0) norm = 1.0;
        if (norm < 0.0) norm = 0.0;
        return static_cast<float>(norm);
    }

    // Convenience: process a block in-place, return final detector value.
    float processBlock(const float* left, const float* right, uint32_t numSamples)
    {
        float det = 0.0f;
        for (uint32_t i = 0; i < numSamples; ++i)
            det = processSample(left[i], right[i]);
        return det;
    }

private:
    static double timeToCoeff(double timeSec, double sampleRate)
    {
        // Coefficient for single-pole to reach 90% in `timeSec`.
        // 1 - exp(-2.3 / (timeSec * sr)) — 2.3 ≈ -ln(0.1).
        if (timeSec <= 0.0 || sampleRate <= 0.0) return 1.0;
        return 1.0 - std::exp(-2.302585 / (timeSec * sampleRate));
    }

    void updateNotch()
    {
        double fc = 0.0;
        if (profile_ == SourceProfile::fm)
            fc = 19000.0;
        else if (profile_ == SourceProfile::tv)
            fc = 15734.0;
        else {
            notchB0_ = 1.0; notchB1_ = 0.0; notchB2_ = 0.0;
            notchA1_ = 0.0; notchA2_ = 0.0;
            notchZ1_ = notchZ2_ = 0.0;
            return;
        }
        // 2nd-order notch (direct form II transposed).
        double w0 = 2.0 * 3.14159265358979 * fc / sr_;
        double Q  = 5.0;
        double alpha = std::sin(w0) / (2.0 * Q);
        double cosW0 = std::cos(w0);
        double a0 = 1.0 + alpha;
        notchB0_ = 1.0 / a0;
        notchB1_ = -2.0 * cosW0 / a0;
        notchB2_ = 1.0 / a0;
        notchA1_ = -2.0 * cosW0 / a0;
        notchA2_ = (1.0 - alpha) / a0;
        notchZ1_ = notchZ2_ = 0.0;
    }

    double processNotch(double x)
    {
        double y = notchB0_ * x + notchZ1_;
        notchZ1_ = notchB1_ * x - notchA1_ * y + notchZ2_;
        notchZ2_ = notchB2_ * x - notchA2_ * y;
        return y;
    }

    double sr_ = 44100.0;
    double hpCoeff_ = 0.0;
    double hpState_[2] = {};
    double emphCoeff_ = 0.0;
    double emphState_ = 0.0;
    double envState_ = 0.0;
    double attackCoeff_ = 1.0;
    double releaseCoeff_ = 0.001;
    float  sensitivityGain_ = 1.0f;
    SourceProfile profile_ = SourceProfile::tape;

    // Notch biquad state.
    double notchB0_ = 1.0, notchB1_ = 0.0, notchB2_ = 0.0;
    double notchA1_ = 0.0, notchA2_ = 0.0;
    double notchZ1_ = 0.0, notchZ2_ = 0.0;
};

} // namespace lm1894

#pragma once

#include <cmath>
#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// VariableLpStage
//
// Single 1st-order low-pass filter with continuously variable cutoff.
// Uses a one-pole topology for smooth, zipper-free modulation.
// -------------------------------------------------------------------
class VariableLpStage
{
public:
    void prepare(double sampleRate)
    {
        sr_ = sampleRate;
        reset();
    }

    void reset()
    {
        state_ = 0.0;
    }

    void setCutoffHz(float fc)
    {
        // Clamp to Nyquist / 2 for stability.
        double maxFc = sr_ * 0.499;
        double f = (fc > maxFc) ? maxFc : static_cast<double>(fc);
        if (f < 1.0) f = 1.0;
        coeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979 * f / sr_);
    }

    float processSample(float x)
    {
        state_ += coeff_ * (static_cast<double>(x) - state_);
        return static_cast<float>(state_);
    }

    void processBlock(float* data, uint32_t numSamples)
    {
        for (uint32_t i = 0; i < numSamples; ++i)
            data[i] = processSample(data[i]);
    }

private:
    double sr_    = 44100.0;
    double coeff_ = 1.0;
    double state_ = 0.0;
};

} // namespace lm1894

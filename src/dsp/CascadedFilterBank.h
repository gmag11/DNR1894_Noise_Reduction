#pragma once

#include "VariableLpStage.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// CascadedFilterBank
//
// 1..kMaxStages identical VariableLpStage per channel, sharing
// the same cutoff. Applies cascade compensation so the combined
// −3 dB point matches the requested global cutoff.
// -------------------------------------------------------------------
class CascadedFilterBank
{
public:
    static constexpr int kMaxStages = 4;

    void prepare(double sampleRate, uint32_t /*maxBlockSize*/)
    {
        sr_ = sampleRate;
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < kMaxStages; ++s)
                stages_[ch][s].prepare(sampleRate);
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < kMaxStages; ++s)
                stages_[ch][s].reset();
    }

    void setStageCount(int n)
    {
        activeStages_ = std::clamp(n, 1, kMaxStages);
    }

    // Set the desired *global* cutoff; internally compensates per-stage.
    void setGlobalCutoffHz(float fcGlobal)
    {
        float fcStage = compensatedStageCutoff(fcGlobal, activeStages_);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < activeStages_; ++s)
                stages_[ch][s].setCutoffHz(fcStage);
    }

    void processBlock(float* left, float* right, uint32_t numSamples)
    {
        for (int s = 0; s < activeStages_; ++s)
        {
            stages_[0][s].processBlock(left,  numSamples);
            stages_[1][s].processBlock(right, numSamples);
        }
    }

    int activeStages() const { return activeStages_; }

private:
    // fc_stage = fc_global / sqrt(10^(0.3/N) - 1)
    static float compensatedStageCutoff(float fcGlobal, int N)
    {
        if (N <= 1) return fcGlobal;
        double factor = std::sqrt(std::pow(10.0, 0.3 / static_cast<double>(N)) - 1.0);
        if (factor < 1e-9) return fcGlobal;
        return static_cast<float>(static_cast<double>(fcGlobal) / factor);
    }

    double sr_ = 44100.0;
    int    activeStages_ = 1;
    VariableLpStage stages_[2][kMaxStages];
};

} // namespace lm1894

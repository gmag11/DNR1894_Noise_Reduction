#pragma once

#include <atomic>
#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// MeterState — lock-free data published by the audio thread
// and consumed by the editor for display.
// All fields are atomic so the editor can read without locks.
// -------------------------------------------------------------------
struct MeterState
{
    // 0.0 = fully closed (min bandwidth), 1.0 = fully open (max bandwidth)
    std::atomic<float> normalizedBandwidthOpen { 0.0f };

    // Smoothed detector activity level, 0.0–1.0
    std::atomic<float> detectorActivity        { 0.0f };

    // Estimated noise reduction in dB (negative = reduction)
    std::atomic<float> estimatedReductionDb    { 0.0f };

    // Current stage count (mirrors parameter, useful for display)
    std::atomic<int>   activeStageCount        { 1 };

    // Bypass state
    std::atomic<bool>  bypass                  { false };
};

} // namespace lm1894

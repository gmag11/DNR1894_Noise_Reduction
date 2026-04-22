#pragma once

// Stable string identifiers for all plugin parameters.
// Used by both the DSP core (via Lm1894Parameters) and the JUCE host bridge.

namespace lm1894 {
namespace param {

constexpr const char* kSensitivityDb   = "sensitivityDb";
constexpr const char* kMinBandwidthHz  = "minBandwidthHz";
constexpr const char* kMaxBandwidthHz  = "maxBandwidthHz";
constexpr const char* kAttackMs        = "attackMs";
constexpr const char* kReleaseMs       = "releaseMs";
constexpr const char* kStageCount      = "stageCount";
constexpr const char* kSourceProfile   = "sourceProfile";
constexpr const char* kBypass          = "bypass";
constexpr const char* kOutputTrimDb    = "outputTrimDb";

} // namespace param
} // namespace lm1894

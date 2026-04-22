#pragma once

#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// Source profile enum — shared between params, DSP and serialisation.
// -------------------------------------------------------------------
enum class SourceProfile : int
{
    generic = 0,
    tape    = 1,
    fm      = 2,
    tv      = 3,
};

constexpr int kSourceProfileCount = 4;

inline const char* sourceProfileName(SourceProfile p)
{
    switch (p) {
        case SourceProfile::generic: return "Generic";
        case SourceProfile::tape:    return "Tape";
        case SourceProfile::fm:      return "FM";
        case SourceProfile::tv:      return "TV";
    }
    return "Unknown";
}

// -------------------------------------------------------------------
// Parameter snapshot consumed by the DSP core.
// No JUCE types — plain C++ only.
// -------------------------------------------------------------------
struct Lm1894Parameters
{
    float sensitivityDb   =  0.0f;
    float minBandwidthHz  =  1000.0f;
    float maxBandwidthHz  = 35000.0f;
    float attackMs        =  0.5f;
    float releaseMs       = 60.0f;
    int   stageCount      =  1;
    SourceProfile sourceProfile = SourceProfile::tape;
    bool  bypass          = false;
    float outputTrimDb    =  0.0f;
};

// -------------------------------------------------------------------
// ParameterLayout — range, default and display metadata.
// One entry per parameter, indexed by a stable string id.
// -------------------------------------------------------------------
struct ParameterRange
{
    float min;
    float max;
    float defaultValue;
    float step;          // 0 = continuous
};

// Canonical ranges (V1).
namespace defaults {

constexpr ParameterRange sensitivityDb   { -18.0f,  18.0f,   0.0f,  0.1f };
constexpr ParameterRange minBandwidthHz  { 800.0f, 4000.0f, 1000.0f, 1.0f };
constexpr ParameterRange maxBandwidthHz  { 8000.0f, 40000.0f, 35000.0f, 1.0f };
constexpr ParameterRange attackMs        { 0.05f,   5.0f,    0.5f,  0.01f };
constexpr ParameterRange releaseMs       { 10.0f, 250.0f,   60.0f,  0.1f  };
constexpr ParameterRange stageCount      { 1.0f,    4.0f,    1.0f,  1.0f  };
constexpr ParameterRange outputTrimDb    { -12.0f, 12.0f,    0.0f,  0.1f  };

} // namespace defaults
} // namespace lm1894

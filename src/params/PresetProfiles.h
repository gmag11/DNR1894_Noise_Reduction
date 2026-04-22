#pragma once

#include "../params/ParameterLayout.h"

namespace lm1894 {

// -------------------------------------------------------------------
// PresetProfiles
//
// Each source profile is just a modifier on the shared parameter
// set. The sidechain weighting and notch filters are already handled
// inside SidechainDetector; profiles only need to set a sensitivity
// offset so that the tape-noise calibration point sits near 2 kHz.
// -------------------------------------------------------------------
struct ProfileCalibration
{
    float sensitivityOffsetDb;  // Added to user's sensitivityDb
};

inline ProfileCalibration getProfileCalibration(SourceProfile p)
{
    switch (p) {
        case SourceProfile::generic: return {  0.0f };
        case SourceProfile::tape:    return { +2.0f };  // Slightly higher to open past 1 kHz with hiss
        case SourceProfile::fm:      return { -1.0f };  // FM tends to have higher levels
        case SourceProfile::tv:      return { +1.0f };  // TV audio is noisier
    }
    return { 0.0f };
}

} // namespace lm1894

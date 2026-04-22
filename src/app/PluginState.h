#pragma once

#include "../params/ParameterLayout.h"
#include <cstdint>

namespace lm1894 {

// -------------------------------------------------------------------
// Serialisation version — bump when the state format changes.
// -------------------------------------------------------------------
constexpr uint32_t kPluginStateVersion = 1;

// -------------------------------------------------------------------
// PluginState — serialisable snapshot of plugin state.
// Framework-independent; the JUCE layer converts to/from XML/binary.
// -------------------------------------------------------------------
struct PluginState
{
    uint32_t          version = kPluginStateVersion;
    Lm1894Parameters  params;

    // Equality for dirty-check / undo.
    bool operator==(const PluginState& o) const
    {
        return version                == o.version
            && params.sensitivityDb   == o.params.sensitivityDb
            && params.minBandwidthHz  == o.params.minBandwidthHz
            && params.maxBandwidthHz  == o.params.maxBandwidthHz
            && params.attackMs        == o.params.attackMs
            && params.releaseMs       == o.params.releaseMs
            && params.stageCount      == o.params.stageCount
            && params.sourceProfile   == o.params.sourceProfile
            && params.bypass          == o.params.bypass
            && params.outputTrimDb    == o.params.outputTrimDb;
    }
    bool operator!=(const PluginState& o) const { return !(*this == o); }
};

} // namespace lm1894

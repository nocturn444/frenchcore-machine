// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/BreakpointEnvelope.h"

#include <string>
#include <vector>

namespace fcm::dsp
{

// Named ready-made shapes, one table per envelope kind (docs/specs/envelope-breakpoint.md §4.5).
// Adding a preset is one entry in the table in the .cpp. The first preset of each kind is the default.
class EnvelopePresets
{
  public:
    struct Preset
    {
        std::string name;
        BreakpointEnvelope envelope;
    };

    // Built on first use; call from a non-audio thread.
    static const std::vector<Preset>& all(EnvelopeKind kind);

    static BreakpointEnvelope makeUnchecked(EnvelopeKind kind, std::vector<EnvelopePoint> points,
                                            float releaseMs)
    {
        return BreakpointEnvelope(kind, std::move(points), releaseMs);
    }
};

} // namespace fcm::dsp

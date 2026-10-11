// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

namespace fcm::dsp
{

// A short mono sample for the Tock's Sample mode (docs/specs/tock.md). Immutable once published to
// the audio thread (ADR 0008).
struct TockSample
{
    std::vector<float> samples;
    double sampleRate = 48000.0;
};

} // namespace fcm::dsp

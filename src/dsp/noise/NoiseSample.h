// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

namespace fcm::dsp
{

// A mono sound for the noise oscillator (docs/specs/noise-oscillator.md). Immutable once published to
// the audio thread (ADR 0008).
struct NoiseSample
{
    std::vector<float> samples;
    double sampleRate = 48000.0;
    // A short hit (Air Burst, Thump) that never loops, whatever the One Shot switch says.
    bool playOnce = false;
};

} // namespace fcm::dsp

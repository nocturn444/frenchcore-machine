// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace fcm::core
{

// The waveform the oscillator reads at a given Position, for display (docs/specs/wavetable-display.md).
// `frames` holds numFrames * frameSize samples; `position` runs 0..1 across all frames, with a
// linear blend between the two neighbouring frames, like the oscillator does. A position that is
// not a finite number counts as 0. Returns frameSize samples (empty when the input is unusable).
inline std::vector<float> interpolatedFrame(std::span<const float> frames, size_t frameSize, float position)
{
    if (frameSize == 0 || frames.empty() || frames.size() % frameSize != 0)
        return {};

    const size_t numFrames = frames.size() / frameSize;
    const float clamped = std::isfinite(position) ? std::clamp(position, 0.0f, 1.0f) : 0.0f;
    const float scaled = clamped * static_cast<float>(numFrames - 1);
    const size_t lower = std::min(static_cast<size_t>(scaled), numFrames - 1);
    const size_t upper = std::min(lower + 1, numFrames - 1);
    const float fraction = scaled - static_cast<float>(lower);

    std::vector<float> out(frameSize);
    for (size_t i = 0; i < frameSize; ++i)
        out[i] = frames[lower * frameSize + i] * (1.0f - fraction) + frames[upper * frameSize + i] * fraction;
    return out;
}

} // namespace fcm::core

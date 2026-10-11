// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

// Last stage of the output: a soft clipper that leaves everything below the knee untouched and
// bends the signal smoothly toward, never above, 0 dBFS (docs/specs/voice-note-handling.md §4.6).
// The curve and its first derivative are continuous at the knee, so it adds no click.
struct SafetyLimiter
{
    static constexpr float kKnee = 0.8f;

    static float process(float x) noexcept
    {
        if (!std::isfinite(x))
            return 0.0f;

        const float magnitude = std::abs(x);
        if (magnitude <= kKnee)
            return x;

        constexpr float headroom = 1.0f - kKnee;
        const float shaped = kKnee + headroom * std::tanh((magnitude - kKnee) / headroom);
        return std::copysign(std::min(shaped, 1.0f), x);
    }
};

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cmath>

namespace fcm::dsp
{

inline constexpr int kMaxEnvelopePoints = 32;
inline constexpr float kMaxEnvelopeTimeMs = 2000.0f;
// How strongly a curvature of +-1 bends a segment: f(x) = (e^(k c x) - 1) / (e^(k c) - 1), k = 5.
inline constexpr float kCurveSteepness = 5.0f;

// `curve` belongs to the segment that leaves the point towards the next one.
struct EnvelopePoint
{
    float timeMs = 0.0f;
    float value = 0.0f;
    float curve = 0.0f;  // -1 (fast first, then slow), 0 linear, +1 (slow first, then fast)
    bool isMain = false; // moved by the knobs; the editor cannot delete it
};

// Progress through a segment (0..1) after applying its curvature.
inline float curveProgress(float x, float curve) noexcept
{
    if (std::abs(curve) < 1.0e-4f)
        return x;

    const float k = kCurveSteepness * curve;
    return std::expm1(k * x) / std::expm1(k);
}

// The immutable, fixed-size copy of an envelope that the audio thread reads (ADR 0006).
// Nothing in it allocates. Times are in milliseconds before the time scale is applied.
struct EnvelopeShape
{
    std::array<EnvelopePoint, kMaxEnvelopePoints> points{};
    int count = 0;
    float releaseMs = 30.0f;

    float lastTimeMs() const noexcept
    {
        return count > 0 ? points[static_cast<size_t>(count - 1)].timeMs : 0.0f;
    }
    float lastValue() const noexcept
    {
        return count > 0 ? points[static_cast<size_t>(count - 1)].value : 0.0f;
    }

    // Level at `timeMs`. `segment` is a lookup cache that only has to be correct when the time
    // moves forward: start it at 0 and keep passing it back (O(1) amortised per sample).
    float valueAt(float timeMs, int& segment) const noexcept
    {
        if (count == 0)
            return 0.0f;

        const int last = count - 1;
        segment = segment < 0 ? 0 : (segment > last ? last : segment);
        while (segment < last && points[static_cast<size_t>(segment + 1)].timeMs <= timeMs)
            ++segment;

        // The last point's level holds for ever after it. With equal times the later point wins.
        if (segment == last)
            return points[static_cast<size_t>(last)].value;

        const auto& from = points[static_cast<size_t>(segment)];
        const auto& to = points[static_cast<size_t>(segment + 1)];
        const float x =
            (timeMs - from.timeMs) / (to.timeMs - from.timeMs); // to.timeMs > timeMs >= from.timeMs
        return from.value + (to.value - from.value) * curveProgress(x < 0.0f ? 0.0f : x, from.curve);
    }

    float valueAt(float timeMs) const noexcept
    {
        int segment = 0;
        return valueAt(timeMs, segment);
    }
};

} // namespace fcm::dsp

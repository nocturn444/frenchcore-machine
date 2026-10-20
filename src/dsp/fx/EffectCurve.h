// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/EnvelopeShape.h"

#include <array>
#include <cmath>
#include <span>

namespace fcm::dsp
{

inline constexpr int kMaxCurvePoints = kMaxEnvelopePoints; // 32
inline constexpr int kMinCurvePoints = 2;

// One point of an effect's drawn curve (docs/decisions/0012 item 3): `x` is the position in the
// cycle (0..1), `y` the level (0..1) and `curve` bends the segment that leaves the point towards the
// next one, like the envelopes' segments (-1 fast first, 0 straight, +1 slow first).
struct CurvePoint
{
    float x = 0.0f;
    float y = 0.0f;
    float curve = 0.0f;
};

// The immutable, fixed-size curve an effect reads on the audio thread, published like the envelope
// shapes (ADR 0006, 0012). Nothing in it allocates. A valid curve has 2 to 32 points, finite numbers,
// `x` from 0 to 1 and never decreasing (two points may share an `x`: a vertical edge), the first at
// `x` = 0 and the last at `x` = 1, `y` in 0..1 and `curve` in -1..1.
struct EffectCurve
{
    std::array<CurvePoint, kMaxCurvePoints> points{};
    int count = 0;

    // A curve from a list of points. It is the caller's job to give a valid one (isValid()).
    static EffectCurve from(std::span<const CurvePoint> list) noexcept
    {
        EffectCurve result;
        result.count = list.size() > static_cast<size_t>(kMaxCurvePoints) ? kMaxCurvePoints
                                                                          : static_cast<int>(list.size());
        for (int i = 0; i < result.count; ++i)
            result.points[static_cast<size_t>(i)] = list[static_cast<size_t>(i)];
        return result;
    }

    bool isValid() const noexcept
    {
        if (count < kMinCurvePoints || count > kMaxCurvePoints)
            return false;
        float previousX = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const auto& p = points[static_cast<size_t>(i)];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.curve))
                return false;
            if (p.x < 0.0f || p.x > 1.0f || p.x < previousX || p.y < 0.0f || p.y > 1.0f || p.curve < -1.0f ||
                p.curve > 1.0f)
                return false;
            previousX = p.x;
        }
        return points[0].x == 0.0f && points[static_cast<size_t>(count - 1)].x == 1.0f;
    }

    // Level at `x` (0..1; anything else is clamped). `segment` is a lookup cache that only has to be
    // right while `x` moves forward: start it at 0 and keep passing it back, and start it again at 0
    // when `x` wraps. With equal `x` the later point wins.
    float valueAt(float x, int& segment) const noexcept
    {
        if (count == 0)
            return 0.0f;
        if (!(x > 0.0f)) // also a number that is not a number
            x = 0.0f;
        else if (x > 1.0f)
            x = 1.0f;

        const int last = count - 1;
        segment = segment < 0 ? 0 : (segment > last ? last : segment);
        // A cache that is ahead of x (the cycle wrapped) is brought back.
        while (segment > 0 && points[static_cast<size_t>(segment)].x > x)
            --segment;
        while (segment < last && points[static_cast<size_t>(segment + 1)].x <= x)
            ++segment;

        if (segment == last)
            return points[static_cast<size_t>(last)].y;

        const auto& from = points[static_cast<size_t>(segment)];
        const auto& to = points[static_cast<size_t>(segment + 1)];
        const float progress = (x - from.x) / (to.x - from.x); // to.x > x >= from.x
        return from.y + (to.y - from.y) * curveProgress(progress < 0.0f ? 0.0f : progress, from.curve);
    }

    float valueAt(float x) const noexcept
    {
        int segment = 0;
        return valueAt(x, segment);
    }
};

// A ready-made shape in an effect's editor menu: static data of the effect type.
struct NamedCurve
{
    const char* name;
    EffectCurve curve;
};

} // namespace fcm::dsp

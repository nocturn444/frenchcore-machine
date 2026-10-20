// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EffectCurveEdit.h"

#include <algorithm>
#include <cmath>

namespace fcm::core::curve_edit
{

namespace
{
float unit(float v) noexcept
{
    return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}
} // namespace

float snapped(float x, float snap) noexcept
{
    x = unit(x);
    if (std::isfinite(snap) && snap > 0.0f)
        x = unit(std::round(x / snap) * snap);
    return x;
}

bool movePoint(dsp::EffectCurve& curve, int index, float x, float y, float snap) noexcept
{
    if (index < 0 || index >= curve.count)
        return false;

    auto& point = curve.points[static_cast<size_t>(index)];
    point.y = unit(y);
    if (index == 0 || index == curve.count - 1)
        return true; // the ends stay at the start and the end of the cycle

    const float low = curve.points[static_cast<size_t>(index - 1)].x;
    const float high = curve.points[static_cast<size_t>(index + 1)].x;
    point.x = std::clamp(snapped(x, snap), low, high);
    return true;
}

int addPoint(dsp::EffectCurve& curve, float x, float y, float snap) noexcept
{
    if (curve.count >= dsp::kMaxCurvePoints)
        return -1;

    const float position = snapped(x, snap);
    // After the last point that is not further along than the new one, so points at one position keep
    // their order, and never before the first or after the last.
    int at = 1;
    while (at < curve.count - 1 && curve.points[static_cast<size_t>(at)].x <= position)
        ++at;

    for (int i = curve.count; i > at; --i)
        curve.points[static_cast<size_t>(i)] = curve.points[static_cast<size_t>(i - 1)];
    curve.points[static_cast<size_t>(at)] = {position, unit(y), 0.0f};
    ++curve.count;
    return at;
}

bool removePoint(dsp::EffectCurve& curve, int index) noexcept
{
    if (index <= 0 || index >= curve.count - 1 || curve.count <= dsp::kMinCurvePoints)
        return false;

    for (int i = index; i < curve.count - 1; ++i)
        curve.points[static_cast<size_t>(i)] = curve.points[static_cast<size_t>(i + 1)];
    --curve.count;
    return true;
}

bool setBend(dsp::EffectCurve& curve, int index, float bend) noexcept
{
    if (index < 0 || index >= curve.count - 1)
        return false;
    curve.points[static_cast<size_t>(index)].curve =
        std::isfinite(bend) ? std::clamp(bend, -1.0f, 1.0f) : 0.0f;
    return true;
}

} // namespace fcm::core::curve_edit

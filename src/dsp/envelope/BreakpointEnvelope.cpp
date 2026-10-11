// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/BreakpointEnvelope.h"

#include "dsp/envelope/EnvelopePresets.h"
#include "dsp/envelope/MusicalTime.h"

#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

namespace fcm::dsp
{

namespace
{
float clamp01(float x) noexcept
{
    return std::clamp(x, 0.0f, 1.0f);
}
} // namespace

BreakpointEnvelope::BreakpointEnvelope(EnvelopeKind kind)
    : BreakpointEnvelope(EnvelopePresets::all(kind).front().envelope)
{
}

BreakpointEnvelope::BreakpointEnvelope(EnvelopeKind kind, std::vector<EnvelopePoint> points, float releaseMs,
                                       float syncRefBpm)
    : envelopeKind(kind), pointList(std::move(points)), release(releaseMs), syncRef(syncRefBpm)
{
}

bool BreakpointEnvelope::isValid(EnvelopeKind kind, const std::vector<EnvelopePoint>& points, float releaseMs,
                                 float syncRefBpm)
{
    // 0 means not synced; otherwise a tempo the editor can offer.
    if (!std::isfinite(syncRefBpm) ||
        (!juce::exactlyEqual(syncRefBpm, 0.0f) && (syncRefBpm < kMinTempoBpm || syncRefBpm > kMaxTempoBpm)))
        return false;

    if (static_cast<int>(points.size()) < minPoints(kind) ||
        static_cast<int>(points.size()) > kMaxEnvelopePoints)
        return false;
    if (!std::isfinite(releaseMs) || releaseMs < kMinReleaseMs || releaseMs > kMaxEnvelopeTimeMs)
        return false;

    int mains = 0;
    float previousTime = 0.0f;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& point = points[i];
        if (!std::isfinite(point.timeMs) || !std::isfinite(point.value) || !std::isfinite(point.curve))
            return false;
        if (point.timeMs < previousTime || point.timeMs > kMaxEnvelopeTimeMs)
            return false;
        if (point.value < 0.0f || point.value > 1.0f || point.curve < -1.0f || point.curve > 1.0f)
            return false;
        previousTime = point.timeMs;
        mains += point.isMain ? 1 : 0;
    }

    return juce::exactlyEqual(points.front().timeMs, 0.0f) && points.front().isMain && points.back().isMain &&
           mains == numMainPoints(kind) &&
           (kind != EnvelopeKind::amp || juce::exactlyEqual(points.front().value, 0.0f));
}

std::optional<BreakpointEnvelope> BreakpointEnvelope::tryCreate(EnvelopeKind kind,
                                                                std::vector<EnvelopePoint> points,
                                                                float releaseMs, float syncRefBpm)
{
    if (!isValid(kind, points, releaseMs, syncRefBpm))
        return std::nullopt;
    return BreakpointEnvelope(kind, std::move(points), releaseMs, syncRefBpm);
}

namespace
{
float clampTempo(float bpm) noexcept
{
    return std::clamp(bpm, static_cast<float>(kMinTempoBpm), static_cast<float>(kMaxTempoBpm));
}
} // namespace

void BreakpointEnvelope::enableSync(float bpm)
{
    if (std::isfinite(bpm))
        syncRef = clampTempo(bpm);
}

void BreakpointEnvelope::rebaseTo(float bpm)
{
    if (!isSynced() || !std::isfinite(bpm))
        return;

    const float newReference = clampTempo(bpm);
    const float factor = syncRef / newReference;

    // Every time keeps its length in beats. Scaling all of them by the same factor keeps the
    // order; the limits only bite when the tempo changed a lot.
    for (auto& point : pointList)
        point.timeMs = std::min(point.timeMs * factor, kMaxEnvelopeTimeMs);
    release = std::clamp(release * factor, kMinReleaseMs, kMaxEnvelopeTimeMs);
    syncRef = newReference;
}

void BreakpointEnvelope::disableSync(float bpm)
{
    rebaseTo(bpm);
    syncRef = 0.0f;
}

std::vector<int> BreakpointEnvelope::mainIndices() const
{
    std::vector<int> indices;
    for (int i = 0; i < size(); ++i)
        if (pointList[static_cast<size_t>(i)].isMain)
            indices.push_back(i);
    return indices;
}

int BreakpointEnvelope::addPoint(float timeMs, float value)
{
    if (size() >= kMaxEnvelopePoints || !std::isfinite(timeMs) || !std::isfinite(value))
        return -1;

    const float time = std::clamp(timeMs, 0.0f, kMaxEnvelopeTimeMs);
    if (time >= pointList.back().timeMs)
        return -1; // the last point has to stay last

    // After every point that is not later than the new time.
    size_t position = 1;
    while (position < pointList.size() && pointList[position].timeMs <= time)
        ++position;

    // Both halves of the segment it splits keep that segment's curvature.
    EnvelopePoint point{time, clamp01(value), pointList[position - 1].curve, false};
    pointList.insert(pointList.begin() + static_cast<std::ptrdiff_t>(position), point);
    return static_cast<int>(position);
}

void BreakpointEnvelope::movePoint(int index, float timeMs, float value)
{
    if (index < 0 || index >= size() || !std::isfinite(timeMs) || !std::isfinite(value))
        return;

    auto& point = pointList[static_cast<size_t>(index)];

    if (index > 0)
    {
        const float lower = pointList[static_cast<size_t>(index - 1)].timeMs;
        const float upper =
            index + 1 < size() ? pointList[static_cast<size_t>(index + 1)].timeMs : kMaxEnvelopeTimeMs;
        point.timeMs = std::clamp(timeMs, lower, upper);
    }

    // The amp envelope always starts at 0 so a kick never starts with a click.
    if (!(index == 0 && envelopeKind == EnvelopeKind::amp))
        point.value = clamp01(value);
}

bool BreakpointEnvelope::removePoint(int index)
{
    if (index <= 0 || index >= size() || pointList[static_cast<size_t>(index)].isMain)
        return false;

    pointList.erase(pointList.begin() + index);
    return true;
}

void BreakpointEnvelope::setCurve(int index, float curve)
{
    if (index >= 0 && index + 1 < size() && std::isfinite(curve))
        pointList[static_cast<size_t>(index)].curve = std::clamp(curve, -1.0f, 1.0f);
}

void BreakpointEnvelope::setReleaseMs(float newReleaseMs)
{
    if (std::isfinite(newReleaseMs))
        release = std::clamp(newReleaseMs, kMinReleaseMs, kMaxEnvelopeTimeMs);
}

void BreakpointEnvelope::retimeMainPoints(const std::vector<float>& newTimes)
{
    const auto mains = mainIndices();
    std::vector<float> oldTimes;
    for (int index : mains)
        oldTimes.push_back(pointList[static_cast<size_t>(index)].timeMs);

    for (size_t section = 0; section + 1 < mains.size(); ++section)
    {
        const float oldStart = oldTimes[section];
        const float oldLength = oldTimes[section + 1] - oldStart;
        const float newStart = newTimes[section];
        const float newLength = newTimes[section + 1] - newStart;

        for (int i = mains[section] + 1; i < mains[section + 1]; ++i)
        {
            auto& extra = pointList[static_cast<size_t>(i)];
            const float fraction = oldLength > 0.0f ? (extra.timeMs - oldStart) / oldLength : 0.0f;
            extra.timeMs = newStart + fraction * newLength;
        }
    }

    for (size_t i = 0; i < mains.size(); ++i)
        pointList[static_cast<size_t>(mains[i])].timeMs = newTimes[i];
}

float BreakpointEnvelope::knob(AmpKnob which) const
{
    const auto mains = mainIndices();
    const auto time = [&](size_t i) { return pointList[static_cast<size_t>(mains[i])].timeMs; };

    switch (which)
    {
    case AmpKnob::attack:
        return time(1) - time(0);
    case AmpKnob::hold:
        return time(2) - time(1);
    case AmpKnob::decay:
        return time(3) - time(2);
    case AmpKnob::sustain:
        return pointList.back().value;
    case AmpKnob::release:
        return release;
    }
    return 0.0f;
}

void BreakpointEnvelope::setKnob(AmpKnob which, float value)
{
    if (!std::isfinite(value))
        return;

    if (which == AmpKnob::sustain)
    {
        pointList.back().value = clamp01(value);
        return;
    }
    if (which == AmpKnob::release)
    {
        setReleaseMs(value);
        return;
    }

    float attack = knob(AmpKnob::attack);
    float hold = knob(AmpKnob::hold);
    float decay = knob(AmpKnob::decay);

    // The knob that changes may take what the other two leave of the 2000 ms.
    const auto fit = [](float wanted, float maximum, float others)
    { return std::clamp(wanted, 0.0f, std::min(maximum, kMaxEnvelopeTimeMs - others)); };
    switch (which)
    {
    case AmpKnob::attack:
        attack = fit(value, 500.0f, hold + decay);
        break;
    case AmpKnob::hold:
        hold = fit(value, 1000.0f, attack + decay);
        break;
    case AmpKnob::decay:
        decay = fit(value, kMaxEnvelopeTimeMs, attack + hold);
        break;
    case AmpKnob::sustain:
    case AmpKnob::release:
        break; // handled above
    }

    retimeMainPoints({0.0f, attack, attack + hold, attack + hold + decay});
}

float BreakpointEnvelope::knob(PitchKnob which) const
{
    const auto mains = mainIndices();
    const auto& peak = pointList[static_cast<size_t>(mains[1])];

    switch (which)
    {
    case PitchKnob::attack:
        return peak.timeMs;
    case PitchKnob::decay:
        return pointList.back().timeMs - peak.timeMs;
    case PitchKnob::curve:
        return peak.curve;
    case PitchKnob::end:
        return pointList.back().value;
    }
    return 0.0f;
}

void BreakpointEnvelope::setKnob(PitchKnob which, float value)
{
    if (!std::isfinite(value))
        return;

    const auto mains = mainIndices();

    if (which == PitchKnob::curve)
    {
        pointList[static_cast<size_t>(mains[1])].curve = std::clamp(value, -1.0f, 1.0f);
        return;
    }
    if (which == PitchKnob::end)
    {
        pointList.back().value = clamp01(value);
        return;
    }

    float attack = knob(PitchKnob::attack);
    float decay = knob(PitchKnob::decay);

    if (which == PitchKnob::attack)
        attack = std::clamp(value, 0.0f, std::min(200.0f, kMaxEnvelopeTimeMs - decay));
    else
        decay = std::clamp(value, 1.0f, std::min(1000.0f, kMaxEnvelopeTimeMs - attack));

    retimeMainPoints({0.0f, attack, attack + decay});
}

std::unique_ptr<const EnvelopeShape> BreakpointEnvelope::toShape() const
{
    auto shape = std::make_unique<EnvelopeShape>();
    shape->count = size();
    shape->releaseMs = release;
    shape->syncRefBpm = syncRef;
    std::copy(pointList.begin(), pointList.end(), shape->points.begin());
    return shape;
}

bool BreakpointEnvelope::operator==(const BreakpointEnvelope& other) const
{
    if (envelopeKind != other.envelopeKind || pointList.size() != other.pointList.size() ||
        !(std::abs(release - other.release) < 1.0e-6f) || !(std::abs(syncRef - other.syncRef) < 1.0e-6f))
        return false;

    for (size_t i = 0; i < pointList.size(); ++i)
    {
        const auto& a = pointList[i];
        const auto& b = other.pointList[i];
        if (!(std::abs(a.timeMs - b.timeMs) < 1.0e-6f && std::abs(a.value - b.value) < 1.0e-6f &&
              std::abs(a.curve - b.curve) < 1.0e-6f && a.isMain == b.isMain))
            return false;
    }
    return true;
}

} // namespace fcm::dsp

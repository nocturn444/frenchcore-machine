// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/EnvelopeShape.h"

#include <memory>
#include <optional>
#include <vector>

namespace fcm::dsp
{

enum class EnvelopeKind
{
    amp,
    pitch
};

enum class AmpKnob
{
    attack,
    hold,
    decay,
    sustain,
    release
};

enum class PitchKnob
{
    attack,
    decay,
    curve,
    end
};

// The editable envelope (docs/specs/envelope-breakpoint.md §4.1): a list of points joined by curved
// segments. A plain value type for the message thread; the audio thread only sees the immutable
// EnvelopeShape made by toShape() (ADR 0006).
//
// Main points are flagged in the data and are the ones the knobs move: the first point (fixed at
// time 0), then for the amp envelope the end of the attack, the end of the hold and the last
// point (the sustain level); for the pitch envelope the peak and the last point. Every other point
// is an extra point.
class BreakpointEnvelope
{
  public:
    static constexpr float kMinReleaseMs = 1.0f;

    // The default shape of the kind (the first preset).
    explicit BreakpointEnvelope(EnvelopeKind kind);

    // Returns nothing when the points do not describe a valid envelope of this kind.
    static std::optional<BreakpointEnvelope> tryCreate(EnvelopeKind kind, std::vector<EnvelopePoint> points,
                                                       float releaseMs);

    static int minPoints(EnvelopeKind kind) noexcept { return kind == EnvelopeKind::amp ? 4 : 3; }
    static int numMainPoints(EnvelopeKind kind) noexcept { return kind == EnvelopeKind::amp ? 4 : 3; }

    EnvelopeKind kind() const noexcept { return envelopeKind; }
    const std::vector<EnvelopePoint>& points() const noexcept { return pointList; }
    int size() const noexcept { return static_cast<int>(pointList.size()); }
    float releaseMs() const noexcept { return release; }

    // Editing. All of them keep the envelope valid: out-of-range requests are clamped.
    // Returns the index of the new point, or -1 when it cannot be added (32 points, or at or after
    // the last point's time).
    int addPoint(float timeMs, float value);
    void movePoint(int index, float timeMs, float value);
    // Only extra points can be removed.
    bool removePoint(int index);
    void setCurve(int index, float curve);
    void setReleaseMs(float newReleaseMs);

    // Knobs. Times are durations in milliseconds; asking for more than the 2000 ms in total is clamped.
    float knob(AmpKnob which) const;
    void setKnob(AmpKnob which, float value);
    float knob(PitchKnob which) const;
    void setKnob(PitchKnob which, float value);

    std::unique_ptr<const EnvelopeShape> toShape() const;

    bool operator==(const BreakpointEnvelope& other) const;

  private:
    BreakpointEnvelope(EnvelopeKind kind, std::vector<EnvelopePoint> points, float releaseMs);

    static bool isValid(EnvelopeKind kind, const std::vector<EnvelopePoint>& points, float releaseMs);
    std::vector<int> mainIndices() const;
    // Gives the main points these times and moves each extra point to the same fraction of the
    // section it sits in.
    void retimeMainPoints(const std::vector<float>& newTimes);

    EnvelopeKind envelopeKind;
    std::vector<EnvelopePoint> pointList;
    float release = 30.0f;

    friend class EnvelopePresets;
};

} // namespace fcm::dsp

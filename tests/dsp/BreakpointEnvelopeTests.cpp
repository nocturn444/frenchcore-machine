// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/BreakpointEnvelope.h"
#include "dsp/envelope/EnvelopePresets.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_core/juce_core.h>

using Catch::Matchers::WithinAbs;
using fcm::dsp::AmpKnob;
using fcm::dsp::BreakpointEnvelope;
using fcm::dsp::EnvelopeKind;
using fcm::dsp::EnvelopePoint;
using fcm::dsp::EnvelopePresets;
using fcm::dsp::PitchKnob;

namespace
{
bool timesAreSorted(const BreakpointEnvelope& envelope)
{
    for (size_t i = 1; i < envelope.points().size(); ++i)
        if (envelope.points()[i].timeMs < envelope.points()[i - 1].timeMs)
            return false;
    return true;
}
} // namespace

TEST_CASE("Default envelopes are the first presets and are valid", "[dsp][envelope][model]")
{
    for (auto kind : {EnvelopeKind::amp, EnvelopeKind::pitch})
    {
        const BreakpointEnvelope envelope(kind);
        REQUIRE(envelope == EnvelopePresets::all(kind).front().envelope);

        // Every preset is accepted by the validation that state loading uses.
        for (const auto& preset : EnvelopePresets::all(kind))
        {
            INFO(preset.name);
            REQUIRE(BreakpointEnvelope::tryCreate(kind, preset.envelope.points(), preset.envelope.releaseMs())
                        .has_value());
        }
    }
}

TEST_CASE("The default amp shape is Kick + Bass", "[dsp][envelope][model]")
{
    const BreakpointEnvelope amp(EnvelopeKind::amp);
    const auto& points = amp.points();

    REQUIRE(points.size() == 5);
    REQUIRE(juce::exactlyEqual(points[0].timeMs, 0.0f));
    REQUIRE(juce::exactlyEqual(points[0].value, 0.0f));
    REQUIRE(juce::exactlyEqual(points[1].timeMs, 1.0f)); // 1 ms attack, no click
    REQUIRE(juce::exactlyEqual(points[1].value, 1.0f));
    REQUIRE(juce::exactlyEqual(points[2].timeMs, 25.0f));  // 25 ms punch
    REQUIRE_THAT(points[3].value, WithinAbs(0.4, 1.0e-6)); // drop to 40 % by 80 ms
    REQUIRE(juce::exactlyEqual(points[3].timeMs, 80.0f));
    REQUIRE(juce::exactlyEqual(points[4].timeMs, 140.0f)); // 70 % body by 140 ms
    REQUIRE_THAT(points[4].value, WithinAbs(0.7, 1.0e-6));
    REQUIRE_THAT(amp.releaseMs(), WithinAbs(30.0, 1.0e-6));
    REQUIRE_FALSE(points[3].isMain); // the dip is an extra point
}

TEST_CASE("addPoint keeps the points sorted and respects the limits", "[dsp][envelope][model]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);

    const int index = amp.addPoint(10.0f, 0.9f); // between the attack and hold points
    REQUIRE(index == 2);
    REQUIRE(amp.size() == 6);
    REQUIRE(timesAreSorted(amp));
    REQUIRE_FALSE(amp.points()[static_cast<size_t>(index)].isMain);

    // Nothing can go at or after the last point.
    REQUIRE(amp.addPoint(140.0f, 0.5f) == -1);
    REQUIRE(amp.addPoint(500.0f, 0.5f) == -1);

    // Levels are clamped into 0..1, times into the envelope.
    const int clamped = amp.addPoint(-50.0f, 7.0f);
    REQUIRE(clamped >= 1);
    REQUIRE(amp.points()[static_cast<size_t>(clamped)].value <= 1.0f);
    REQUIRE(amp.points()[static_cast<size_t>(clamped)].timeMs >= 0.0f);
    REQUIRE(timesAreSorted(amp));

    // 32 points at most.
    while (amp.size() < fcm::dsp::kMaxEnvelopePoints)
        REQUIRE(amp.addPoint(50.0f + static_cast<float>(amp.size()) * 0.01f, 0.5f) >= 0);
    REQUIRE(amp.addPoint(60.0f, 0.5f) == -1);
    REQUIRE(amp.size() == fcm::dsp::kMaxEnvelopePoints);
    REQUIRE(timesAreSorted(amp));
}

TEST_CASE("A new point keeps the curvature of the segment it splits", "[dsp][envelope][model]")
{
    BreakpointEnvelope pitch(EnvelopeKind::pitch);
    const int index = pitch.addPoint(30.0f, 0.5f);
    REQUIRE(index >= 0);
    REQUIRE_THAT(pitch.points()[static_cast<size_t>(index)].curve, WithinAbs(-1.0, 1.0e-6));
    REQUIRE_THAT(pitch.points()[static_cast<size_t>(index - 1)].curve, WithinAbs(-1.0, 1.0e-6));
}

TEST_CASE("movePoint is clamped between the neighbours and keeps the first point fixed",
          "[dsp][envelope][model]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);

    amp.movePoint(2, 5000.0f, 0.3f); // hold end: cannot pass the next point (80 ms)
    REQUIRE_THAT(amp.points()[2].timeMs, WithinAbs(80.0, 1.0e-4));
    REQUIRE_THAT(amp.points()[2].value, WithinAbs(0.3, 1.0e-6));

    amp.movePoint(2, -10.0f, 2.0f); // cannot pass the previous point (1 ms); level clamps to 1
    REQUIRE_THAT(amp.points()[2].timeMs, WithinAbs(1.0, 1.0e-4));
    REQUIRE(amp.points()[2].value <= 1.0f);

    amp.movePoint(0, 300.0f, 0.8f); // the first point stays at time 0, and at level 0 on the amp envelope
    REQUIRE(juce::exactlyEqual(amp.points()[0].timeMs, 0.0f));
    REQUIRE(juce::exactlyEqual(amp.points()[0].value, 0.0f));

    amp.movePoint(4, 9000.0f, -1.0f); // the last point cannot go past 2000 ms
    REQUIRE(amp.points()[4].timeMs <= fcm::dsp::kMaxEnvelopeTimeMs);
    REQUIRE(amp.points()[4].value >= 0.0f);
    REQUIRE(timesAreSorted(amp));

    // The pitch envelope's first point may change level.
    BreakpointEnvelope pitch(EnvelopeKind::pitch);
    pitch.movePoint(0, 0.0f, 0.4f);
    REQUIRE_THAT(pitch.points()[0].value, WithinAbs(0.4, 1.0e-6));

    // Out of range indices and non-finite numbers are ignored.
    const auto before = amp;
    amp.movePoint(99, 1.0f, 1.0f);
    amp.movePoint(1, std::nanf(""), 0.5f);
    REQUIRE(amp == before);
}

TEST_CASE("removePoint only removes extra points", "[dsp][envelope][model]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);

    REQUIRE_FALSE(amp.removePoint(0));
    REQUIRE_FALSE(amp.removePoint(1)); // main
    REQUIRE_FALSE(amp.removePoint(2)); // main
    REQUIRE_FALSE(amp.removePoint(4)); // last, main
    REQUIRE_FALSE(amp.removePoint(40));
    REQUIRE(amp.size() == 5);

    REQUIRE(amp.removePoint(3)); // the dip
    REQUIRE(amp.size() == 4);
    REQUIRE(amp.size() >= BreakpointEnvelope::minPoints(EnvelopeKind::amp));
}

TEST_CASE("setCurve and setReleaseMs clamp their values", "[dsp][envelope][model]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);

    amp.setCurve(2, 5.0f);
    REQUIRE_THAT(amp.points()[2].curve, WithinAbs(1.0, 1.0e-6));
    amp.setCurve(2, -5.0f);
    REQUIRE_THAT(amp.points()[2].curve, WithinAbs(-1.0, 1.0e-6));

    amp.setCurve(4, 0.5f); // the last point has no segment leaving it
    REQUIRE_THAT(amp.points()[4].curve, WithinAbs(0.0, 1.0e-6));

    amp.setReleaseMs(0.0f);
    REQUIRE_THAT(amp.releaseMs(), WithinAbs(1.0, 1.0e-6));
    amp.setReleaseMs(99999.0f);
    REQUIRE_THAT(amp.releaseMs(), WithinAbs(2000.0, 1.0e-6));
}

TEST_CASE("Amp knobs read back what they set and move the main points", "[dsp][envelope][knobs]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);

    // Defaults: attack 1, hold 24, decay 115, sustain 70 %, release 30.
    REQUIRE_THAT(amp.knob(AmpKnob::attack), WithinAbs(1.0, 1.0e-4));
    REQUIRE_THAT(amp.knob(AmpKnob::hold), WithinAbs(24.0, 1.0e-4));
    REQUIRE_THAT(amp.knob(AmpKnob::decay), WithinAbs(115.0, 1.0e-4));
    REQUIRE_THAT(amp.knob(AmpKnob::sustain), WithinAbs(0.7, 1.0e-6));
    REQUIRE_THAT(amp.knob(AmpKnob::release), WithinAbs(30.0, 1.0e-6));

    amp.setKnob(AmpKnob::attack, 10.0f);
    amp.setKnob(AmpKnob::hold, 40.0f);
    amp.setKnob(AmpKnob::decay, 200.0f);
    amp.setKnob(AmpKnob::sustain, 0.5f);
    amp.setKnob(AmpKnob::release, 80.0f);

    REQUIRE_THAT(amp.knob(AmpKnob::attack), WithinAbs(10.0, 1.0e-3));
    REQUIRE_THAT(amp.knob(AmpKnob::hold), WithinAbs(40.0, 1.0e-3));
    REQUIRE_THAT(amp.knob(AmpKnob::decay), WithinAbs(200.0, 1.0e-3));
    REQUIRE_THAT(amp.knob(AmpKnob::sustain), WithinAbs(0.5, 1.0e-6));
    REQUIRE_THAT(amp.knob(AmpKnob::release), WithinAbs(80.0, 1.0e-6));

    // The main points are where the knobs say.
    REQUIRE_THAT(amp.points()[1].timeMs, WithinAbs(10.0, 1.0e-3));
    REQUIRE_THAT(amp.points()[2].timeMs, WithinAbs(50.0, 1.0e-3));
    REQUIRE_THAT(amp.points().back().timeMs, WithinAbs(250.0, 1.0e-3));
    REQUIRE(timesAreSorted(amp));
}

TEST_CASE("Amp knob changes keep extra points at the same fraction of their section",
          "[dsp][envelope][knobs]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);

    // The dip at 80 ms sits between the hold end (25) and the sustain point (140): 55 / 115 of the way.
    const float fraction = (80.0f - 25.0f) / (140.0f - 25.0f);
    const float level = amp.points()[3].value;

    amp.setKnob(AmpKnob::decay, 400.0f);
    amp.setKnob(AmpKnob::attack, 20.0f);

    const float start = amp.points()[2].timeMs;
    const float end = amp.points().back().timeMs;
    REQUIRE_THAT(amp.points()[3].timeMs, WithinAbs(start + fraction * (end - start), 1.0e-3));
    REQUIRE_THAT(amp.points()[3].value, WithinAbs(level, 1.0e-6)); // the level is not touched
    REQUIRE(amp.size() == 5);
}

TEST_CASE("Knob times never push the envelope past 2000 ms", "[dsp][envelope][knobs]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);
    amp.setKnob(AmpKnob::decay, 5000.0f);
    REQUIRE(amp.points().back().timeMs <= fcm::dsp::kMaxEnvelopeTimeMs + 1.0e-3f);

    amp.setKnob(AmpKnob::hold, 1000.0f);
    amp.setKnob(AmpKnob::attack, 500.0f);
    REQUIRE(amp.points().back().timeMs <= fcm::dsp::kMaxEnvelopeTimeMs + 1.0e-3f);
    REQUIRE(timesAreSorted(amp));

    // Out-of-range and non-finite requests are harmless.
    const auto before = amp;
    amp.setKnob(AmpKnob::sustain, std::nanf(""));
    REQUIRE(amp == before);
    amp.setKnob(AmpKnob::sustain, 3.0f);
    REQUIRE_THAT(amp.knob(AmpKnob::sustain), WithinAbs(1.0, 1.0e-6));
}

TEST_CASE("Pitch knobs read back what they set", "[dsp][envelope][knobs]")
{
    BreakpointEnvelope pitch(EnvelopeKind::pitch);

    REQUIRE_THAT(pitch.knob(PitchKnob::attack), WithinAbs(0.0, 1.0e-6));
    REQUIRE_THAT(pitch.knob(PitchKnob::decay), WithinAbs(90.0, 1.0e-4));
    REQUIRE_THAT(pitch.knob(PitchKnob::curve), WithinAbs(-1.0, 1.0e-6));
    REQUIRE_THAT(pitch.knob(PitchKnob::end), WithinAbs(0.0, 1.0e-6));

    pitch.setKnob(PitchKnob::attack, 12.0f);
    pitch.setKnob(PitchKnob::decay, 150.0f);
    pitch.setKnob(PitchKnob::curve, -0.25f);
    pitch.setKnob(PitchKnob::end, 0.2f);

    REQUIRE_THAT(pitch.knob(PitchKnob::attack), WithinAbs(12.0, 1.0e-3));
    REQUIRE_THAT(pitch.knob(PitchKnob::decay), WithinAbs(150.0, 1.0e-3));
    REQUIRE_THAT(pitch.knob(PitchKnob::curve), WithinAbs(-0.25, 1.0e-6));
    REQUIRE_THAT(pitch.knob(PitchKnob::end), WithinAbs(0.2, 1.0e-6));
    REQUIRE_THAT(pitch.points().back().timeMs, WithinAbs(162.0, 1.0e-3));

    // The decay is at least 1 ms.
    pitch.setKnob(PitchKnob::decay, 0.0f);
    REQUIRE(pitch.knob(PitchKnob::decay) >= 1.0f - 1.0e-4f);
}

TEST_CASE("Knobs keep working after the user has added and moved points", "[dsp][envelope][knobs]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);
    REQUIRE(amp.addPoint(10.0f, 0.95f) >= 0); // an extra point before the hold end
    REQUIRE(amp.addPoint(300.0f, 0.2f) < 0);  // not allowed: after the last point
    REQUIRE(amp.addPoint(100.0f, 0.5f) >= 0);

    // The index of the main points changed, but the knobs still find them by their flag.
    REQUIRE_THAT(amp.knob(AmpKnob::attack), WithinAbs(1.0, 1.0e-4));
    REQUIRE_THAT(amp.knob(AmpKnob::hold), WithinAbs(24.0, 1.0e-4));
    REQUIRE_THAT(amp.knob(AmpKnob::decay), WithinAbs(115.0, 1.0e-4));

    amp.setKnob(AmpKnob::hold, 60.0f);
    REQUIRE_THAT(amp.knob(AmpKnob::hold), WithinAbs(60.0, 1.0e-3));
    REQUIRE(timesAreSorted(amp));
}

TEST_CASE("toShape copies the model", "[dsp][envelope][model]")
{
    const BreakpointEnvelope amp(EnvelopeKind::amp);
    const auto shape = amp.toShape();

    REQUIRE(shape->count == amp.size());
    REQUIRE_THAT(shape->releaseMs, WithinAbs(amp.releaseMs(), 1.0e-6));
    for (int i = 0; i < shape->count; ++i)
    {
        REQUIRE(juce::exactlyEqual(shape->points[static_cast<size_t>(i)].timeMs,
                                   amp.points()[static_cast<size_t>(i)].timeMs));
        REQUIRE(juce::exactlyEqual(shape->points[static_cast<size_t>(i)].value,
                                   amp.points()[static_cast<size_t>(i)].value));
    }
}

TEST_CASE("tryCreate rejects invalid envelopes", "[dsp][envelope][model]")
{
    const auto good = BreakpointEnvelope(EnvelopeKind::amp).points();
    REQUIRE(BreakpointEnvelope::tryCreate(EnvelopeKind::amp, good, 30.0f).has_value());

    auto expectRejected = [&](auto change, float release = 30.0f)
    {
        auto points = good;
        change(points);
        REQUIRE_FALSE(BreakpointEnvelope::tryCreate(EnvelopeKind::amp, points, release).has_value());
    };

    expectRejected(
        [](auto& p)
        {
            p.pop_back();
            p.pop_back();
        }); // too few points
    expectRejected(
        [](auto& p)
        {
            while (p.size() < 40)
                p.insert(p.begin() + 1, p[1]);
        });                                              // too many
    expectRejected([](auto& p) { p[2].timeMs = 0.5f; }); // out of order
    expectRejected([](auto& p) { p[3].value = 1.5f; });
    expectRejected([](auto& p) { p[3].curve = -2.0f; });
    expectRejected([](auto& p) { p[3].value = std::nanf(""); });
    expectRejected([](auto& p) { p[4].timeMs = 3000.0f; });
    expectRejected([](auto& p) { p[0].timeMs = 1.0f; });
    expectRejected([](auto& p) { p[0].value = 0.5f; });  // the amp starts at 0
    expectRejected([](auto& p) { p[3].isMain = true; }); // wrong number of main points
    expectRejected([](auto& p) { p.back().isMain = false; });
    expectRejected([](auto&) {}, 0.0f);    // release too short
    expectRejected([](auto&) {}, 5000.0f); // release too long
    expectRejected([](auto&) {}, std::nanf(""));

    // The pitch envelope needs 3 main points.
    REQUIRE_FALSE(BreakpointEnvelope::tryCreate(EnvelopeKind::pitch, good, 30.0f).has_value());
}

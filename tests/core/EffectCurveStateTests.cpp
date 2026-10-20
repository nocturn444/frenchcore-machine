// SPDX-License-Identifier: AGPL-3.0-only
// The drawn curves of effects (docs/decisions/0012 item 3): publishing, saving, loading hostile input,
// and what adding and duplicating an effect do to its curve. Tested with a test-only effect.
#include "../dsp/TestEffects.h"
#include "TestProcessor.h"
#include "core/EffectCurveState.h"
#include "core/FxRackState.h"
#include "core/ParameterIds.h"
#include "core/ProjectState.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>

using Catch::Matchers::WithinAbs;
using fcm::core::EffectCurveState;
using fcm::core::FxRackState;
using fcm::dsp::CurvePoint;
using fcm::dsp::EffectCurve;
using fcm::dsp::EffectRegistry;
using fcm::dsp::RackId;
using fcm::test::CurveEffect;

namespace
{
constexpr int kGain = 0;
constexpr int kCurve = 1;

EffectRegistry makeRegistry()
{
    EffectRegistry registry;
    registry.add<fcm::test::GainEffect>();
    registry.add<CurveEffect>();
    return registry;
}

EffectCurve make(std::initializer_list<CurvePoint> points)
{
    const std::vector<CurvePoint> list(points);
    return EffectCurve::from(list);
}

bool same(const EffectCurve& a, const EffectCurve& b)
{
    if (a.count != b.count)
        return false;
    for (int i = 0; i < a.count; ++i)
        if (a.points[static_cast<size_t>(i)].x != b.points[static_cast<size_t>(i)].x ||
            a.points[static_cast<size_t>(i)].y != b.points[static_cast<size_t>(i)].y ||
            a.points[static_cast<size_t>(i)].curve != b.points[static_cast<size_t>(i)].curve)
            return false;
    return true;
}

int instance(int effect, int copy = 1)
{
    return EffectRegistry::instanceIndex(effect, copy);
}

const EffectCurve kDown = make({{0, 1, 0}, {1, 0, 0}});
const EffectCurve kDrawn = make({{0, 0.2f, 0.5f}, {0.3f, 1, -0.25f}, {0.3f, 0.4f, 0}, {1, 0.9f, 1}});

struct Rig
{
    Rig() : processor(registry), racks(processor.state, registry, {&busA, &busB, &main}), curves(registry)
    {
        racks.setCurveState(&curves);
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EffectRegistry registry = makeRegistry();
    fcm::test::TestProcessor processor;
    std::atomic<std::uint64_t> busA{0}, busB{0}, main{0};
    FxRackState racks;
    EffectCurveState curves;
};

juce::ValueTree curveNode(const juce::String& instanceId, std::initializer_list<std::array<double, 3>> points)
{
    juce::ValueTree curve("Curve");
    curve.setProperty("instance", instanceId, nullptr);
    for (const auto& p : points)
    {
        juce::ValueTree point("Point");
        point.setProperty("x", p[0], nullptr);
        point.setProperty("y", p[1], nullptr);
        point.setProperty("curve", p[2], nullptr);
        curve.appendChild(point, nullptr);
    }
    return curve;
}
} // namespace

TEST_CASE("Only effects with a curve have one, each copy with its own", "[core][fx][curve]")
{
    Rig rig;
    for (int copy = 1; copy <= fcm::dsp::kInstancesPerEffect; ++copy)
    {
        REQUIRE_FALSE(rig.curves.hasCurve(instance(kGain, copy)));
        REQUIRE(rig.curves.source(instance(kGain, copy)) == nullptr);
        REQUIRE(rig.curves.get(instance(kGain, copy)).count == 0);

        REQUIRE(rig.curves.hasCurve(instance(kCurve, copy)));
        REQUIRE(rig.curves.source(instance(kCurve, copy)) != nullptr);
        REQUIRE(same(rig.curves.get(instance(kCurve, copy)), CurveEffect::rampCurve()));
    }
    REQUIRE_FALSE(rig.curves.hasCurve(-1));
    REQUIRE_FALSE(rig.curves.hasCurve(1000));
    REQUIRE(rig.curves.source(1000) == nullptr);

    // The default is published from the start.
    REQUIRE(rig.curves.source(instance(kCurve))->acquire() != nullptr);
}

TEST_CASE("Setting a curve publishes it and announces the change; a bad one is refused", "[core][fx][curve]")
{
    Rig rig;
    const int target = instance(kCurve, 2);

    REQUIRE(rig.curves.set(target, kDrawn));
    REQUIRE(same(rig.curves.get(target), kDrawn));
    const auto* published = rig.curves.source(target)->acquire();
    REQUIRE(published != nullptr);
    REQUIRE(same(*published, kDrawn));
    // The other copies are untouched.
    REQUIRE(same(rig.curves.get(instance(kCurve, 1)), CurveEffect::rampCurve()));

    // Refused: invalid curves and instances without a curve; nothing changes.
    EffectCurve bad = kDown;
    bad.points[1].x = 0.5f; // does not end at 1
    REQUIRE_FALSE(rig.curves.set(target, bad));
    REQUIRE_FALSE(rig.curves.set(target, EffectCurve{}));
    REQUIRE_FALSE(rig.curves.set(instance(kGain), kDown));
    REQUIRE_FALSE(rig.curves.set(-3, kDown));
    REQUIRE(same(rig.curves.get(target), kDrawn));

    rig.curves.reset(target);
    REQUIRE(same(rig.curves.get(target), CurveEffect::rampCurve()));
    REQUIRE(same(*rig.curves.source(target)->acquire(), CurveEffect::rampCurve()));
}

TEST_CASE("Adding an effect resets its curve, duplicating copies it, removing leaves it", "[core][fx][curve]")
{
    Rig rig;

    const auto first = rig.racks.add(RackId::main, kCurve);
    REQUIRE(first == "testcurve");
    REQUIRE(rig.curves.set(instance(kCurve, 1), kDrawn));

    // Duplicate: the copy has the same drawing, and the original keeps it.
    const auto copy = rig.racks.duplicate(RackId::main, first);
    REQUIRE(copy == "testcurve2");
    REQUIRE(same(rig.curves.get(instance(kCurve, 2)), kDrawn));
    REQUIRE(same(rig.curves.get(instance(kCurve, 1)), kDrawn));

    // Editing the copy does not touch the original.
    REQUIRE(rig.curves.set(instance(kCurve, 2), kDown));
    REQUIRE(same(rig.curves.get(instance(kCurve, 1)), kDrawn));

    // Remove leaves the curve where it is...
    REQUIRE(rig.racks.remove(RackId::main, copy));
    REQUIRE(same(rig.curves.get(instance(kCurve, 2)), kDown));
    // ...and the next add of that copy starts again from the default.
    REQUIRE(rig.racks.add(RackId::busA, kCurve) == "testcurve2");
    REQUIRE(same(rig.curves.get(instance(kCurve, 2)), CurveEffect::rampCurve()));
}

TEST_CASE("Curves are saved and loaded back, every one of them", "[core][fx][curve]")
{
    Rig source;
    REQUIRE(source.curves.set(instance(kCurve, 1), kDrawn));
    REQUIRE(source.curves.set(instance(kCurve, 3), kDown));
    const auto node = source.curves.toValueTree();
    REQUIRE(node.getType() == EffectCurveState::nodeType());
    REQUIRE(node.getNumChildren() == fcm::dsp::kInstancesPerEffect); // a curve per copy, drawn or not

    Rig target;
    REQUIRE(target.curves.set(instance(kCurve, 2), kDrawn)); // will be replaced by the default
    target.curves.restoreFrom(node);
    REQUIRE(same(target.curves.get(instance(kCurve, 1)), kDrawn));
    REQUIRE(same(target.curves.get(instance(kCurve, 2)), CurveEffect::rampCurve()));
    REQUIRE(same(target.curves.get(instance(kCurve, 3)), kDown));
    REQUIRE(same(*target.curves.source(instance(kCurve, 3))->acquire(), kDown));

    // Through the text form too (what the host stores), with the numbers surviving.
    const auto xml = node.createXml();
    REQUIRE(xml != nullptr);
    const auto reread = juce::ValueTree::fromXml(*xml);
    Rig again;
    again.curves.restoreFrom(reread);
    REQUIRE(same(again.curves.get(instance(kCurve, 1)), kDrawn));
}

TEST_CASE("A hostile curve node loads safely: bad curves fall back to the default",
          "[core][fx][curve][security]")
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const std::array<double, 3> start{0, 1, 0}, end{1, 0, 0};

    struct Case
    {
        const char* what;
        juce::ValueTree node;
    };
    std::vector<Case> cases;

    juce::ValueTree wrongType("Something");
    cases.push_back({"wrong node type", wrongType});
    cases.push_back({"no node", juce::ValueTree()});

    juce::ValueTree bad(EffectCurveState::nodeType());
    bad.appendChild(curveNode("testcurve", {{0, 1, 0}}), nullptr);                             // one point
    bad.appendChild(curveNode("testcurve2", {{0, 1, 0}, {0.8, 0, 0}}), nullptr);               // ends early
    bad.appendChild(curveNode("testcurve3", {start, {0.7, 0, 0}, {0.3, 1, 0}, end}), nullptr); // backwards
    bad.appendChild(curveNode("testcurve4", {start, {nan, 1, 0}, end}), nullptr);              // not a number
    bad.appendChild(curveNode("nonexistent", {start, end}), nullptr); // unknown instance
    bad.appendChild(curveNode("testgain", {start, end}), nullptr);    // no curve there
    bad.appendChild(curveNode("testcurve5", {start, end}), nullptr);  // no fifth copy
    bad.appendChild(curveNode("", {start, end}), nullptr);
    cases.push_back({"bad curves", bad});

    // More than 32 points.
    juce::ValueTree many(EffectCurveState::nodeType());
    {
        juce::ValueTree curve("Curve");
        curve.setProperty("instance", "testcurve", nullptr);
        for (int i = 0; i < 500; ++i)
        {
            juce::ValueTree point("Point");
            point.setProperty("x", i / 499.0, nullptr);
            point.setProperty("y", 0.5, nullptr);
            point.setProperty("curve", 0.0, nullptr);
            curve.appendChild(point, nullptr);
        }
        many.appendChild(curve, nullptr);
    }
    cases.push_back({"500 points", many});

    // Thousands of nodes, and nodes with wrong types and missing properties.
    juce::ValueTree flood(EffectCurveState::nodeType());
    for (int i = 0; i < 5000; ++i)
        flood.appendChild(curveNode("testcurve", {start, end}), nullptr);
    flood.appendChild(juce::ValueTree("Junk"), nullptr);
    juce::ValueTree noInstance("Curve");
    flood.appendChild(noInstance, nullptr);
    juce::ValueTree noProps("Curve");
    noProps.setProperty("instance", "testcurve2", nullptr);
    juce::ValueTree emptyPoint("Point");
    noProps.appendChild(emptyPoint, nullptr);
    noProps.appendChild(emptyPoint.createCopy(), nullptr);
    flood.appendChild(noProps, nullptr);
    cases.push_back({"flood", flood});

    for (const auto& c : cases)
    {
        INFO(c.what);
        Rig rig;
        REQUIRE(rig.curves.set(instance(kCurve, 2), kDrawn));
        rig.curves.restoreFrom(c.node); // must not crash or throw
        for (int copy = 1; copy <= fcm::dsp::kInstancesPerEffect; ++copy)
        {
            const auto& curve = rig.curves.get(instance(kCurve, copy));
            REQUIRE(curve.isValid());
        }
        // Whatever was before is gone: the bad node never leaves a half-loaded curve behind.
        if (std::string(c.what) != "flood")
            REQUIRE(same(rig.curves.get(instance(kCurve, 2)), CurveEffect::rampCurve()));
    }

    // A curve listed twice: the first counts.
    juce::ValueTree twice(EffectCurveState::nodeType());
    twice.appendChild(curveNode("testcurve", {start, end}), nullptr);
    twice.appendChild(curveNode("testcurve", {{0, 0.3, 0}, {1, 0.3, 0}}), nullptr);
    Rig rig;
    rig.curves.restoreFrom(twice);
    REQUIRE(same(rig.curves.get(instance(kCurve, 1)), kDown));

    // Text that is not a number is read as a number or zero, and the result is validated all the same.
    juce::ValueTree text(EffectCurveState::nodeType());
    juce::ValueTree curve("Curve");
    curve.setProperty("instance", "testcurve", nullptr);
    for (const char* x : {"0", "banana", "1"})
    {
        juce::ValueTree point("Point");
        point.setProperty("x", juce::String(x), nullptr);
        point.setProperty("y", "0.5", nullptr);
        point.setProperty("curve", "0", nullptr);
        curve.appendChild(point, nullptr);
    }
    text.appendChild(curve, nullptr);
    rig.curves.restoreFrom(text);
    REQUIRE(rig.curves.get(instance(kCurve, 1)).isValid());
}

TEST_CASE("The project state keeps the curves with the effects", "[core][fx][curve][project]")
{
    struct Pieces
    {
        Rig rig;
        fcm::core::EnvelopeState amp{fcm::dsp::EnvelopeKind::amp};
        fcm::core::EnvelopeState pitch{fcm::dsp::EnvelopeKind::pitch};
        fcm::core::WavetableSelector wavetable1{rig.processor.state, 0};
        fcm::core::WavetableSelector wavetable2{rig.processor.state, 1};
        fcm::core::TockSelector tock{rig.processor.state};
        fcm::core::NoiseSelector noise{rig.processor.state};
        fcm::core::ProjectState project{
            fcm::core::ProjectState::Parts{rig.processor.state,
                                           juce::Identifier("FrenchcoreMachineState"),
                                           amp,
                                           pitch,
                                           rig.racks,
                                           {&wavetable1, &wavetable2},
                                           tock,
                                           noise,
                                           rig.curves}};
    };

    Pieces source;
    source.rig.racks.add(RackId::main, kCurve);
    REQUIRE(source.rig.curves.set(instance(kCurve, 1), kDrawn));
    const auto saved = source.project.capture();
    REQUIRE(static_cast<int>(saved["stateVersion"]) == fcm::core::kStateVersion);
    REQUIRE(saved.getChildWithName("EffectCurves").isValid());

    Pieces target;
    REQUIRE(target.project.restore(saved) != fcm::core::StateLoadResult::rejected);
    REQUIRE(same(target.rig.curves.get(instance(kCurve, 1)), kDrawn));

    // A 0.2 state has no curve node: every effect gets its default.
    auto old = saved.createCopy();
    old.setProperty("stateVersion", 2, nullptr);
    old.removeChild(old.getChildWithName("EffectCurves"), nullptr);
    REQUIRE(target.rig.curves.set(instance(kCurve, 1), kDown));
    REQUIRE(target.project.restore(old) != fcm::core::StateLoadResult::rejected);
    REQUIRE(same(target.rig.curves.get(instance(kCurve, 1)), CurveEffect::rampCurve()));
}

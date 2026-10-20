// SPDX-License-Identifier: AGPL-3.0-only
// The operations of the curve editor keep a valid curve whatever they are given
// (docs/specs/fx-volumelfo.md sections 2 and 5).
#include "core/EffectCurveEdit.h"
#include "core/EffectCurveState.h"
#include "dsp/fx/EffectRegistry.h"
#include "dsp/fx/volumelfo/VolumeLfoEffect.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

namespace edit = fcm::core::curve_edit;
using fcm::dsp::CurvePoint;
using fcm::dsp::EffectCurve;

namespace
{
EffectCurve pump()
{
    return *fcm::dsp::VolumeLfoEffect().defaultCurve();
}
} // namespace

TEST_CASE("Moving a point keeps it between its neighbours and the ends at the ends", "[core][curve][edit]")
{
    auto curve = pump(); // (0, 1) (0.45, 1) (0.9, 0) (1, 0)
    REQUIRE(edit::movePoint(curve, 1, 0.6f, 0.4f, 0.0f));
    REQUIRE(curve.points[1].x == 0.6f);
    REQUIRE(curve.points[1].y == 0.4f);

    // It cannot pass the next point, but it can reach it (a vertical edge).
    REQUIRE(edit::movePoint(curve, 1, 0.95f, 0.4f, 0.0f));
    REQUIRE(curve.points[1].x == 0.9f);
    REQUIRE(edit::movePoint(curve, 1, 0.0f, 0.4f, 0.0f));
    REQUIRE(curve.points[1].x == 0.0f);
    REQUIRE(curve.isValid());

    // The ends only move up and down.
    REQUIRE(edit::movePoint(curve, 0, 0.5f, 0.3f, 0.0f));
    REQUIRE(curve.points[0].x == 0.0f);
    REQUIRE(curve.points[0].y == 0.3f);
    REQUIRE(edit::movePoint(curve, 3, 0.2f, 2.0f, 0.0f));
    REQUIRE(curve.points[3].x == 1.0f);
    REQUIRE(curve.points[3].y == 1.0f);
    REQUIRE(curve.isValid());

    REQUIRE_FALSE(edit::movePoint(curve, 9, 0.5f, 0.5f, 0.0f));
    REQUIRE_FALSE(edit::movePoint(curve, -1, 0.5f, 0.5f, 0.0f));
}

TEST_CASE("Snapping puts a point on the grid", "[core][curve][edit]")
{
    REQUIRE(edit::snapped(0.30f, 0.25f) == 0.25f);
    REQUIRE(edit::snapped(0.40f, 0.25f) == 0.5f);
    REQUIRE(edit::snapped(0.40f, 0.0f) == 0.40f);
    REQUIRE(edit::snapped(2.0f, 0.25f) == 1.0f);

    auto curve = pump();
    REQUIRE(edit::movePoint(curve, 1, 0.43f, 0.5f, 0.0625f));
    REQUIRE(curve.points[1].x == 0.4375f);
    const int added = edit::addPoint(curve, 0.69f, 0.5f, 0.125f);
    REQUIRE(added >= 0);
    REQUIRE(curve.points[static_cast<size_t>(added)].x == 0.75f);
    REQUIRE(curve.isValid());
}

TEST_CASE("Adding puts the point in order, up to 32, and deleting keeps two", "[core][curve][edit]")
{
    auto curve = pump();
    const int at = edit::addPoint(curve, 0.7f, 0.5f, 0.0f);
    REQUIRE(at == 2);
    REQUIRE(curve.count == 5);
    REQUIRE(curve.points[2].x == 0.7f);
    REQUIRE(curve.isValid());

    // At the very ends the new point goes next to them, never before the first or after the last.
    REQUIRE(edit::addPoint(curve, 0.0f, 0.2f, 0.0f) == 1);
    const int atEnd = edit::addPoint(curve, 1.0f, 0.2f, 0.0f);
    REQUIRE(atEnd == curve.count - 2);
    REQUIRE(curve.isValid());

    while (edit::addPoint(curve, 0.5f, 0.5f, 0.0f) >= 0)
        REQUIRE(curve.isValid());
    REQUIRE(curve.count == fcm::dsp::kMaxCurvePoints);
    REQUIRE(edit::addPoint(curve, 0.5f, 0.5f, 0.0f) == -1);

    // The ends cannot be deleted; any other point can, down to two.
    REQUIRE_FALSE(edit::removePoint(curve, 0));
    REQUIRE_FALSE(edit::removePoint(curve, curve.count - 1));
    while (curve.count > 2)
    {
        REQUIRE(edit::removePoint(curve, 1));
        REQUIRE(curve.isValid());
    }
    REQUIRE_FALSE(edit::removePoint(curve, 1));
    REQUIRE(curve.count == 2);
}

TEST_CASE("Bending sets the segment's curve within range", "[core][curve][edit]")
{
    auto curve = pump();
    REQUIRE(edit::setBend(curve, 1, 0.5f));
    REQUIRE(curve.points[1].curve == 0.5f);
    REQUIRE(edit::setBend(curve, 1, 7.0f));
    REQUIRE(curve.points[1].curve == 1.0f);
    REQUIRE(edit::setBend(curve, 1, std::numeric_limits<float>::quiet_NaN()));
    REQUIRE(curve.points[1].curve == 0.0f);
    REQUIRE_FALSE(edit::setBend(curve, curve.count - 1, 0.5f)); // no segment leaves the last point
    REQUIRE(curve.isValid());
}

TEST_CASE("Any sequence of edits, including hostile numbers, leaves a valid curve", "[core][curve][edit]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float values[] = {-inf, -3.0f, -0.5f, 0.0f, 0.0001f, 0.3f, 0.5f, 0.999f, 1.0f, 7.0f, inf, nan};

    unsigned state = 12345;
    const auto next = [&]
    {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    };

    auto curve = pump();
    for (int step = 0; step < 20000; ++step)
    {
        const float x = values[next() % std::size(values)];
        const float y = values[next() % std::size(values)];
        const float snap = (next() % 3 == 0) ? 0.0f : 1.0f / static_cast<float>(1 + next() % 32);
        const int index = static_cast<int>(next() % 40) - 3;
        switch (next() % 4)
        {
        case 0:
            edit::movePoint(curve, index, x, y, snap);
            break;
        case 1:
            edit::addPoint(curve, x, y, snap);
            break;
        case 2:
            edit::removePoint(curve, index);
            break;
        default:
            edit::setBend(curve, index, x);
            break;
        }
        REQUIRE(curve.isValid());
    }
}

TEST_CASE("The Volume LFO's drawn shape is saved and loaded back, and a damaged one falls back to Pump",
          "[core][curve][volumelfo]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    const int effect = registry.indexOf("volumelfo");
    REQUIRE(effect >= 0);
    const int first = fcm::dsp::EffectRegistry::instanceIndex(effect, 1);
    const int second = fcm::dsp::EffectRegistry::instanceIndex(effect, 2);

    fcm::core::EffectCurveState state(registry);
    REQUIRE(state.hasCurve(first));
    REQUIRE(state.get(first).count == pump().count); // the default is Pump

    auto drawn = pump();
    edit::addPoint(drawn, 0.7f, 0.6f, 0.0625f);
    edit::setBend(drawn, 1, 0.3f);
    REQUIRE(state.set(second, drawn));

    fcm::core::EffectCurveState loaded(registry);
    loaded.restoreFrom(state.toValueTree());
    REQUIRE(loaded.get(second).count == drawn.count);
    for (int i = 0; i < drawn.count; ++i)
    {
        REQUIRE(loaded.get(second).points[static_cast<size_t>(i)].x ==
                drawn.points[static_cast<size_t>(i)].x);
        REQUIRE(loaded.get(second).points[static_cast<size_t>(i)].y ==
                drawn.points[static_cast<size_t>(i)].y);
        REQUIRE(loaded.get(second).points[static_cast<size_t>(i)].curve ==
                drawn.points[static_cast<size_t>(i)].curve);
    }
    REQUIRE(loaded.get(first).count == pump().count); // the other copy kept its own

    // A damaged node: a point that goes backwards. That copy is Pump again.
    auto node = state.toValueTree();
    for (auto curveNode : node)
        if (curveNode["instance"].toString() == "volumelfo2")
            curveNode.getChild(1).setProperty("x", 5.0f, nullptr);
    fcm::core::EffectCurveState damaged(registry);
    damaged.restoreFrom(node);
    REQUIRE(damaged.get(second).isValid());
    REQUIRE(damaged.get(second).count == pump().count);
}

// SPDX-License-Identifier: AGPL-3.0-only
// The shared pieces of version 0.3's effects (docs/decisions/0012): the drawn curve, the note-on
// helper, the tempo LFO and the curve path from the publisher to an effect.
#include "AllocationCounter.h"
#include "TestEffects.h"
#include "dsp/engine/Engine.h"
#include "dsp/envelope/MusicalTime.h"
#include "dsp/fx/EffectCurve.h"
#include "dsp/fx/NoteTriggers.h"
#include "dsp/fx/TempoLfo.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::CurvePoint;
using fcm::dsp::EffectCurve;
using fcm::dsp::PendingNoteOns;
using fcm::dsp::TempoLfo;

namespace
{
EffectCurve make(std::initializer_list<CurvePoint> points)
{
    const std::vector<CurvePoint> list(points);
    return EffectCurve::from(list);
}

const float kNaN = std::numeric_limits<float>::quiet_NaN();
} // namespace

TEST_CASE("A curve is valid with 2 to 32 points that go from the start to the end of the cycle",
          "[dsp][fx][curve]")
{
    REQUIRE(make({{0, 1, 0}, {1, 0, 0}}).isValid());
    REQUIRE(make({{0, 0, 0}, {0.5, 1, -1}, {0.5, 0, 1}, {1, 1, 0}}).isValid()); // a vertical edge is fine

    EffectCurve full;
    full.count = fcm::dsp::kMaxCurvePoints;
    for (int i = 0; i < full.count; ++i)
        full.points[static_cast<size_t>(i)] = {static_cast<float>(i) / 31.0f, 0.5f, 0.0f};
    REQUIRE(full.isValid());
    full.count = 1;
    REQUIRE_FALSE(full.isValid());
    full.count = 0;
    REQUIRE_FALSE(full.isValid());
    full.count = 33; // never more than the array holds
    REQUIRE_FALSE(full.isValid());

    REQUIRE_FALSE(make({{0, 1, 0}}).isValid());
    REQUIRE_FALSE(make({{0.1f, 1, 0}, {1, 0, 0}}).isValid());                          // does not start at 0
    REQUIRE_FALSE(make({{0, 1, 0}, {0.9f, 0, 0}}).isValid());                          // does not end at 1
    REQUIRE_FALSE(make({{0, 1, 0}, {0.6f, 0, 0}, {0.4f, 1, 0}, {1, 0, 0}}).isValid()); // goes backwards
    REQUIRE_FALSE(make({{0, 1, 0}, {1.5f, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{-0.5f, 1, 0}, {1, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{0, 1.5f, 0}, {1, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{0, -0.1f, 0}, {1, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{0, 1, 2.0f}, {1, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{0, 1, -1.5f}, {1, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{0, kNaN, 0}, {1, 0, 0}}).isValid());
    REQUIRE_FALSE(make({{0, 1, 0}, {1, 0, kNaN}}).isValid());
    REQUIRE_FALSE(make({{0, 1, 0}, {std::numeric_limits<float>::infinity(), 0, 0}}).isValid());
}

TEST_CASE("A curve gives its level along the cycle", "[dsp][fx][curve]")
{
    const auto line = make({{0, 1, 0}, {1, 0, 0}});
    REQUIRE_THAT(line.valueAt(0.0f), WithinAbs(1.0, 1.0e-6));
    REQUIRE_THAT(line.valueAt(0.25f), WithinAbs(0.75, 1.0e-6));
    REQUIRE_THAT(line.valueAt(1.0f), WithinAbs(0.0, 1.0e-6));
    REQUIRE_THAT(line.valueAt(-3.0f), WithinAbs(1.0, 1.0e-6)); // clamped
    REQUIRE_THAT(line.valueAt(7.0f), WithinAbs(0.0, 1.0e-6));

    // A bent segment follows the envelopes' formula; the bend leaves the point towards the next one.
    const auto bent = make({{0, 0, 1.0f}, {1, 1, 0}});
    REQUIRE_THAT(bent.valueAt(0.5f), WithinAbs(fcm::dsp::curveProgress(0.5f, 1.0f), 1.0e-6));
    REQUIRE(bent.valueAt(0.5f) < 0.5f); // slow first, then fast

    // A vertical edge: the later point wins at the edge itself.
    const auto gate = make({{0, 1, 0}, {0.5, 1, 0}, {0.5, 0, 0}, {1, 0, 0}});
    REQUIRE(gate.valueAt(0.49f) == 1.0f);
    REQUIRE(gate.valueAt(0.5f) == 0.0f);
    REQUIRE(gate.valueAt(0.99f) == 0.0f);

    // Empty or hostile: no crash, a finite answer.
    REQUIRE(EffectCurve{}.valueAt(0.3f) == 0.0f);
    REQUIRE(std::isfinite(line.valueAt(kNaN)));
}

TEST_CASE("A segment cache that moves forward, and back at a wrap, gives the same values", "[dsp][fx][curve]")
{
    const auto curve = make({{0, 0, 0}, {0.2f, 1, -0.5f}, {0.5f, 0.2f, 0.7f}, {0.5f, 0.9f, 0}, {1, 0.1f, 0}});
    int segment = 0;
    for (int cycle = 0; cycle < 3; ++cycle)
        for (int i = 0; i <= 1000; ++i)
        {
            const float x = static_cast<float>(i) / 1000.0f;
            REQUIRE(curve.valueAt(x, segment) == curve.valueAt(x));
        }
    // Even a cache that starts anywhere is brought to the right place.
    for (int start : {-5, 0, 2, 4, 99})
    {
        int cache = start;
        REQUIRE(curve.valueAt(0.35f, cache) == curve.valueAt(0.35f));
    }
}

TEST_CASE("PendingNoteOns cuts a buffer at the note-ons", "[dsp][fx][notetrigger]")
{
    struct Piece
    {
        int begin, end;
        bool restart;
        bool operator==(const Piece&) const = default;
    };
    const auto cut = [](std::initializer_list<int> offsets, int length)
    {
        PendingNoteOns pending;
        for (int o : offsets)
            pending.add(o);
        std::vector<Piece> pieces;
        pending.run(length, [&](int b, int e, bool r) { pieces.push_back({b, e, r}); });
        return pieces;
    };

    REQUIRE(cut({}, 100) == std::vector<Piece>{{0, 100, false}});
    REQUIRE(cut({0}, 100) == std::vector<Piece>{{0, 100, true}});
    REQUIRE(cut({30, 70}, 100) == std::vector<Piece>{{0, 30, false}, {30, 70, true}, {70, 100, true}});
    REQUIRE(cut({30, 30}, 100) == std::vector<Piece>{{0, 30, false}, {30, 100, true}}); // twice at one sample
    REQUIRE(cut({99}, 100) == std::vector<Piece>{{0, 99, false}, {99, 100, true}});
    REQUIRE(cut({0, 0, 50}, 100) == std::vector<Piece>{{0, 50, true}, {50, 100, true}});

    // It forgets them after one run, and never holds more than its capacity.
    PendingNoteOns pending;
    for (int i = 0; i < 40; ++i)
        pending.add(i);
    int pieces = 0;
    pending.run(100, [&](int, int, bool) { ++pieces; });
    REQUIRE(pieces == fcm::dsp::kMaxNoteTriggers); // 16 kept: 15 short pieces and the rest
    pieces = 0;
    pending.run(100, [&](int, int, bool) { ++pieces; });
    REQUIRE(pieces == 1); // one piece, no restart
}

TEST_CASE("The tempo LFO runs at its rate, restarts and holds at zero", "[dsp][fx][lfo]")
{
    TempoLfo lfo;
    lfo.prepare(48000.0);

    // 2 Hz: a cycle lasts 24000 samples.
    lfo.setRateHz(2.0);
    REQUIRE(lfo.next() == 0.0f);
    for (int i = 1; i < 12000; ++i)
        lfo.next();
    REQUIRE_THAT(lfo.value(), WithinAbs(0.5, 1.0e-4));
    for (int i = 0; i < 12000; ++i)
        lfo.next();
    REQUIRE_THAT(lfo.value(), WithinAbs(0.0, 1.0e-4)); // wrapped

    // A note restarts it: the very next value is 0.
    for (int i = 0; i < 5000; ++i)
        lfo.next();
    REQUIRE(lfo.value() > 0.1f);
    lfo.restart();
    REQUIRE(lfo.next() == 0.0f);

    // Rate 0 holds the phase; so do rates that are not usable.
    lfo.restart();
    lfo.setRateHz(0.0);
    for (int i = 0; i < 1000; ++i)
        lfo.next();
    REQUIRE(lfo.value() == 0.0f);
    lfo.setRateHz(std::numeric_limits<double>::quiet_NaN());
    lfo.advance();
    lfo.setRateHz(-3.0);
    lfo.advance();
    REQUIRE(lfo.value() == 0.0f);
}

TEST_CASE("The synced tempo LFO has one cycle per note value at the tempo", "[dsp][fx][lfo]")
{
    TempoLfo lfo;
    lfo.prepare(48000.0);

    const auto samplesPerCycle = [&](int figure, double bpm)
    {
        lfo.setSynced(figure, bpm);
        lfo.restart();
        int n = 0;
        float previous = 0.0f;
        do
        {
            previous = lfo.value();
            lfo.advance();
            ++n;
        } while (lfo.value() > previous && n < 10000000); // the phase falls when the cycle wraps
        return n;
    };

    // 1/4 at 120 BPM: 500 ms. A bar at 120 BPM: 2 s. 1/8 at 60 BPM: 500 ms. A triplet and a dotted value.
    REQUIRE(std::abs(samplesPerCycle(9, 120.0) - 24000) <= 1);
    REQUIRE(std::abs(samplesPerCycle(13, 120.0) - 96000) <= 1);
    REQUIRE(std::abs(samplesPerCycle(6, 60.0) - 24000) <= 1);
    REQUIRE(std::abs(samplesPerCycle(7, 120.0) - 16000) <= 1);  // 1/4T = 2/3 beat
    REQUIRE(std::abs(samplesPerCycle(11, 120.0) - 36000) <= 1); // 1/4D = 1.5 beats
    // A tempo that is not usable plays at 120; a figure out of range is clamped.
    REQUIRE(std::abs(samplesPerCycle(9, std::numeric_limits<double>::quiet_NaN()) - 24000) <= 1);
    REQUIRE(std::abs(samplesPerCycle(99, 120.0) - 96000) <= 1);
    REQUIRE(std::abs(samplesPerCycle(-4, 120.0) - 1500) <= 1);
}

TEST_CASE("The LFO's note values are the envelopes' ones, frozen", "[dsp][fx][lfo]")
{
    // The saved value of a division parameter is the entry number, so the list never changes. It starts
    // as a copy of the envelopes' figures; the numbers are written out here so that a change of either
    // list is noticed.
    REQUIRE(fcm::dsp::kLfoDivisions.size() == fcm::dsp::kMusicalFigures.size());
    for (size_t i = 0; i < fcm::dsp::kLfoDivisions.size(); ++i)
    {
        REQUIRE(fcm::dsp::kLfoDivisions[i].name == std::string_view(fcm::dsp::kMusicalFigures[i].name));
        REQUIRE(fcm::dsp::kLfoDivisions[i].beats == fcm::dsp::kMusicalFigures[i].beats);
        REQUIRE(fcm::dsp::kLfoDivisionNames[i] == fcm::dsp::kLfoDivisions[i].name);
    }
    const std::vector<std::string_view> frozen{"1/64", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8",
                                               "1/4T", "1/8D", "1/4",   "1/2T", "1/4D", "1/2",   "1 bar"};
    for (size_t i = 0; i < frozen.size(); ++i)
        REQUIRE(fcm::dsp::kLfoDivisions[i].name == frozen[i]);
    REQUIRE(fcm::dsp::kLfoDivisions[9].beats == 1.0); // 1/4 is one beat
    REQUIRE(fcm::dsp::kLfoDivisions[13].beats == 4.0);
}

namespace
{
using fcm::test::CurveEffect;

fcm::dsp::EffectRegistry makeCurveRegistry()
{
    fcm::dsp::EffectRegistry registry;
    registry.add<fcm::test::GainEffect>();
    registry.add<CurveEffect>();
    return registry;
}
} // namespace

TEST_CASE("The registry knows which effects have a curve and what their default and shapes are",
          "[dsp][fx][curve]")
{
    const auto registry = makeCurveRegistry();
    REQUIRE(registry.defaultCurve(0) == nullptr); // the gain effect has none
    REQUIRE(registry.curvePresets(0).empty());
    REQUIRE(registry.defaultCurve(1) != nullptr);
    REQUIRE(registry.defaultCurve(1)->isValid());
    REQUIRE(registry.curvePresets(1).size() == 2);
    for (const auto& preset : registry.curvePresets(1))
        REQUIRE(preset.curve.isValid());
    REQUIRE(registry.defaultCurve(-1) == nullptr);
    REQUIRE(registry.defaultCurve(99) == nullptr);
    REQUIRE(registry.curvePresets(99).empty());
}

TEST_CASE("A published curve reaches the effect once per block, and nothing allocates", "[dsp][fx][curve]")
{
    CurveEffect::curveCalls = 0;
    CurveEffect::last = nullptr;
    const auto registry = makeCurveRegistry();
    fcm::dsp::Engine engine(registry);

    fcm::dsp::PublishedPointer<EffectCurve> source;
    const int instance = fcm::dsp::EffectRegistry::instanceIndex(1, 1);
    engine.setEffectCurveSource(instance, &source);

    fcm::dsp::EngineParameters parameters;
    parameters.effects[static_cast<size_t>(instance)].enabled = true;
    engine.setParameters(parameters);
    engine.prepare({48000.0, 128, 2});
    const std::vector<int> contents{instance};
    engine.rackValue(fcm::dsp::RackId::main).store(fcm::dsp::EffectRack::pack(contents));

    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;

    // Nothing published yet: the effect is given nullptr and uses its default.
    buffer.clear();
    engine.setParameters(parameters);
    engine.process(buffer, midi);
    REQUIRE(CurveEffect::curveCalls == 1);
    REQUIRE(CurveEffect::last == &CurveEffect::rampCurve());

    source.publish(std::make_unique<const EffectCurve>(make({{0, 1, 0}, {1, 0, 0}})));
    {
        fcm::test::AllocationCounter counter;
        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            engine.setParameters(parameters);
            engine.process(buffer, midi);
        }
        if (fcm::test::AllocationCounter::available())
            REQUIRE(counter.count() == 0);
    }
    REQUIRE(CurveEffect::curveCalls == 21);
    REQUIRE(CurveEffect::last != &CurveEffect::rampCurve());
    REQUIRE(CurveEffect::last->valueAt(0.0f) == 1.0f);

    // An instance that is in no rack is not touched.
    const int before = CurveEffect::curveCalls;
    engine.rackValue(fcm::dsp::RackId::main).store(fcm::dsp::EffectRack::pack({}));
    buffer.clear();
    engine.setParameters(parameters);
    engine.process(buffer, midi);
    REQUIRE(CurveEffect::curveCalls == before);
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "../TestUtils.h"
#include "dsp/fx/volumelfo/VolumeLfoEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using fcm::dsp::EffectCurve;
using fcm::dsp::VolumeLfoEffect;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

struct Settings
{
    float sync = 1.0f;
    float rate = 2.0f;
    float division = 9.0f; // 1/4
    float depth = 100.0f;
    float smooth = 10.0f;
    std::vector<float> values() const { return {sync, rate, division, depth, smooth}; }
};

VolumeLfoEffect makeEffect(const Settings& settings, const EffectCurve* curve = nullptr)
{
    VolumeLfoEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());
    effect.setCurve(curve);
    return effect;
}

const EffectCurve& preset(const char* name)
{
    for (const auto& each : VolumeLfoEffect().curvePresets())
        if (std::string(each.name) == name)
            return each.curve;
    FAIL("no preset " << name);
    return *VolumeLfoEffect().defaultCurve();
}

struct Note
{
    int at;
};

std::vector<float> run(VolumeLfoEffect& effect, const std::vector<float>& input,
                       const std::vector<Note>& notes, float tempo = 120.0f)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.setContext({0.0f, tempo});
        for (const auto& note : notes)
            if (note.at >= static_cast<int>(start) && note.at < static_cast<int>(start) + kBlock)
                effect.noteOn(note.at - static_cast<int>(start));
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    return out;
}

std::vector<float> noise(int length, unsigned seed)
{
    std::vector<float> out(static_cast<size_t>(length));
    unsigned state = seed;
    for (auto& sample : out)
    {
        state = state * 1664525u + 1013904223u;
        sample = 0.5f * (static_cast<float>(state >> 8) / 8388608.0f - 1.0f);
    }
    return out;
}

// Samples until the gain of a unit input has covered 63 % of the way from 1 towards 0 (the time
// constant of the slew), counted from the start of the edge.
double timeConstantMs(float smooth)
{
    Settings settings;
    settings.smooth = smooth;
    auto effect = makeEffect(settings, &preset("Gate 1/2"));
    const std::vector<float> ones(kBlock * 200, 1.0f);
    const auto out = run(effect, ones, {{0}});
    // The gate falls at half of the cycle: 1/4 at 120 BPM is 500 ms, so the edge is at 250 ms.
    const size_t edge = static_cast<size_t>(0.25 * kRate);
    for (size_t i = edge; i < out.size(); ++i)
        if (out[i] <= 1.0f - 0.632f)
            return static_cast<double>(i - edge) / kRate * 1000.0;
    return -1.0;
}
} // namespace

TEST_CASE("Pump at 1/4 and 120 BPM: full at the note, empty at 450 ms, repeated every 500 ms",
          "[dsp][fx][volumelfo]")
{
    Settings settings;
    auto effect = makeEffect(settings);
    const std::vector<float> ones(kBlock * 400, 1.0f);
    const auto out = run(effect, ones, {{0}});

    const auto at = [&](double seconds) { return out[static_cast<size_t>(seconds * kRate)]; };
    REQUIRE(out[0] > 0.99f);
    REQUIRE(at(0.2) > 0.99f);
    REQUIRE(at(0.455) < 0.02f);
    REQUIRE(at(0.499) < 0.02f);
    REQUIRE(at(0.505) > 0.97f); // the second cycle
    REQUIRE(at(0.955) < 0.02f);
    REQUIRE(at(1.505) > 0.97f); // the third
}

TEST_CASE("A note-on at an offset restarts the shape at that sample", "[dsp][fx][volumelfo]")
{
    Settings settings;
    settings.smooth = 50.0f;
    const std::vector<float> ones(kBlock * 120, 1.0f);

    // A note at 300 ms: the shape was empty (the cycle was at 60 %), and it comes back from there.
    auto inside = makeEffect(settings);
    const int noteAt = static_cast<int>(0.3 * kRate) / kBlock * kBlock + 100;
    const auto a = run(inside, ones, {{0}, {noteAt}});

    auto split = makeEffect(settings);
    std::vector<float> b;
    const auto process = [&](size_t from, size_t count, bool note)
    {
        juce::AudioBuffer<float> buffer(2, static_cast<int>(count));
        for (int c = 0; c < 2; ++c)
            for (size_t i = 0; i < count; ++i)
                buffer.setSample(c, static_cast<int>(i), ones[from + i]);
        split.setContext({0.0f, 120.0f});
        if (note)
            split.noteOn(0);
        split.process(buffer);
        b.insert(b.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + count);
    };
    process(0, static_cast<size_t>(noteAt) / kBlock * kBlock, true);
    process(static_cast<size_t>(noteAt) / kBlock * kBlock, 100, false);
    process(static_cast<size_t>(noteAt), ones.size() - static_cast<size_t>(noteAt), true);
    REQUIRE(a.size() == b.size());
    REQUIRE(a == b);
    REQUIRE(a[static_cast<size_t>(noteAt) - 1] < 0.9f);     // it was on its way down or empty
    REQUIRE(a[static_cast<size_t>(noteAt) + 2000] > 0.99f); // and the shape started again
}

TEST_CASE("Depth 0 is bit-exact unity and Depth 50 never goes below half", "[dsp][fx][volumelfo]")
{
    const auto input = noise(kBlock * 300, 9);

    Settings none;
    none.depth = 0.0f;
    auto unity = makeEffect(none);
    REQUIRE(run(unity, input, {{0}, {20000}}) == input);

    Settings half;
    half.depth = 50.0f;
    half.smooth = 0.0f;
    auto effect = makeEffect(half);
    const std::vector<float> ones(kBlock * 300, 1.0f);
    const auto out = run(effect, ones, {{0}});
    float lowest = 1.0f;
    for (float sample : out)
        lowest = std::min(lowest, sample);
    INFO("lowest " << lowest);
    REQUIRE(lowest >= 0.5f - 1.0e-5f);
    REQUIRE(lowest < 0.52f);
}

TEST_CASE("Smooth sets how long an edge takes, from 0.2 ms to 20 ms", "[dsp][fx][volumelfo]")
{
    const double shortest = timeConstantMs(0.0f);
    const double middle = timeConstantMs(50.0f);
    const double longest = timeConstantMs(100.0f);
    INFO(shortest << " ms, " << middle << " ms, " << longest << " ms");
    REQUIRE(shortest >= 0.2 * 0.9);
    REQUIRE(shortest <= 0.2 * 1.25);
    REQUIRE(std::abs(middle - 5.0) < 0.5);
    REQUIRE(std::abs(longest - 20.0) < 2.0);

    // Smooth 0 on a full-scale sine across a sharp edge: no step larger than the one a 0.2 ms slew allows.
    Settings sharp;
    sharp.smooth = 0.0f;
    auto effect = makeEffect(sharp, &preset("Gate 1/2"));
    std::vector<float> sine(kBlock * 200);
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] =
            static_cast<float>(std::sin(2.0 * std::numbers::pi * 100.0 * static_cast<double>(i) / kRate));
    const auto out = run(effect, sine, {{0}});
    float most = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        most = std::max(most, std::abs(out[i] - out[i - 1]));
    REQUIRE(most < 0.12f);
}

TEST_CASE("The ready-made shapes are valid curves, Pump is the default and each does what its name says",
          "[dsp][fx][volumelfo]")
{
    VolumeLfoEffect effect;
    const auto presets = effect.curvePresets();
    REQUIRE(presets.size() == 10);
    for (const auto& each : presets)
    {
        INFO(each.name);
        REQUIRE(each.curve.isValid());
    }
    REQUIRE(std::string(presets[0].name) == "Pump");
    REQUIRE(effect.defaultCurve()->isValid());

    const auto& gate = preset("Gate 1/4");
    REQUIRE(gate.valueAt(0.1f) == 1.0f);
    REQUIRE(gate.valueAt(0.3f) == 0.0f);
    REQUIRE(gate.valueAt(0.6f) == 1.0f);
    REQUIRE(gate.valueAt(0.9f) == 0.0f);
    REQUIRE(preset("Saw Down").valueAt(0.25f) == 0.75f);
    REQUIRE(preset("Triangle").valueAt(0.5f) == 0.0f);
    REQUIRE(std::abs(preset("Sine").valueAt(0.25f) - 0.5f) < 1.0e-5f);
    REQUIRE(preset("Sidechain").valueAt(0.1f) < 0.1f * 1.0f / 0.35f); // slow first
}

TEST_CASE("A curve that is not valid, or none, plays the default; hostile values stay finite",
          "[dsp][fx][volumelfo]")
{
    const std::vector<float> ones(kBlock * 100, 1.0f);
    Settings settings;
    auto reference = makeEffect(settings);
    const auto expected = run(reference, ones, {{0}});

    EffectCurve empty; // count 0
    auto withEmpty = makeEffect(settings, &empty);
    REQUIRE(run(withEmpty, ones, {{0}}) == expected);

    EffectCurve broken = *VolumeLfoEffect().defaultCurve();
    broken.points[1].y = std::numeric_limits<float>::quiet_NaN();
    auto withBroken = makeEffect(settings, &broken);
    REQUIRE(run(withBroken, ones, {{0}}) == expected);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const auto& values :
         {std::vector<float>(5, nan), std::vector<float>(5, inf), std::vector<float>(5, -inf),
          std::vector<float>(5, 1.0e9f), std::vector<float>(5, -1.0e9f)})
    {
        VolumeLfoEffect hostile;
        hostile.prepare({kRate, kBlock, 2});
        hostile.setParameters(values);
        hostile.setContext({nan, nan});
        hostile.noteOn(-5);
        hostile.noteOn(100000);
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int i = 0; i < kBlock; ++i)
        {
            const float sample = i == 3 ? nan : 0.5f;
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }
        hostile.process(buffer);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                REQUIRE(std::isfinite(buffer.getSample(c, i)));
    }
}

TEST_CASE("Sync off follows Rate in Hz, and the effect is deterministic with no latency",
          "[dsp][fx][volumelfo]")
{
    Settings synced;
    Settings free = synced;
    free.sync = 0.0f;
    free.rate = 2.0f; // one cycle per 500 ms, the same as 1/4 at 120 BPM
    const std::vector<float> ones(kBlock * 100, 1.0f);
    auto a = makeEffect(synced);
    auto b = makeEffect(free);
    REQUIRE(run(a, ones, {{0}}, 120.0f) == run(b, ones, {{0}}, 120.0f));

    REQUIRE(a.latencySamples(8) == 0);
    a.reset();
    a.setParameters(synced.values());
    a.setCurve(nullptr);
    auto c = makeEffect(synced);
    REQUIRE(run(a, ones, {{0}}) == run(c, ones, {{0}}));
}

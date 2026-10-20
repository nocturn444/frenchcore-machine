// SPDX-License-Identifier: AGPL-3.0-only
#include "../TestUtils.h"
#include "dsp/fx/flanger/FlangerEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using fcm::dsp::FlangerEffect;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

struct Settings
{
    float delay = 2.0f;
    float sync = 0.0f;
    float rate = 0.0f;
    float division = 9.0f; // 1/4
    float depth = 0.0f;
    float feedback = 0.0f;
    float keytrack = 0.0f;
    std::vector<float> values() const { return {delay, sync, rate, division, depth, feedback, keytrack}; }
};

// A still flanger seen through its impulse response.
struct Response
{
    std::vector<float> impulse;

    explicit Response(const Settings& settings, float noteSemitones = 0.0f)
    {
        FlangerEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(settings.values());
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < 256; ++block)
        {
            buffer.clear();
            if (block == 0)
            {
                buffer.setSample(0, 0, 1.0f);
                buffer.setSample(1, 0, 1.0f);
            }
            effect.setContext({noteSemitones, 120.0f});
            effect.process(buffer);
            impulse.insert(impulse.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        }
    }

    double magnitude(double hz) const
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < impulse.size(); ++i)
        {
            const double w = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / kRate;
            re += impulse[i] * std::cos(w);
            im -= impulse[i] * std::sin(w);
        }
        return std::sqrt(re * re + im * im);
    }

    // The frequency in [low, high] where the response is lowest or highest.
    double extremum(double low, double high, bool lowest) const
    {
        double best = low, bestLevel = magnitude(low);
        for (double f = low; f <= high; f *= 1.002)
        {
            const double level = magnitude(f);
            if (lowest ? level < bestLevel : level > bestLevel)
            {
                best = f;
                bestLevel = level;
            }
        }
        return best;
    }
};

std::vector<float> noiseBurst(int length, unsigned seed)
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

struct Note
{
    int at;
};

std::vector<float> run(FlangerEffect& effect, const std::vector<float>& input, const std::vector<Note>& notes,
                       float tempo = 120.0f)
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

FlangerEffect makeEffect(const Settings& settings)
{
    FlangerEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());
    return effect;
}
} // namespace

TEST_CASE("A still flanger of 2 ms has notches at the odd multiples of 1 / (2 D)", "[dsp][fx][flanger]")
{
    const Response response{Settings{}};
    for (const double expected : {250.0, 750.0, 1250.0})
    {
        const double found = response.extremum(expected * 0.8, expected * 1.2, true);
        INFO("notch near " << expected << " Hz found at " << found);
        REQUIRE(std::abs(found - expected) < 0.02 * expected);
        REQUIRE(response.magnitude(found) < 0.02);
    }
}

TEST_CASE("Key Track 100 % halves the delay when the note goes up an octave", "[dsp][fx][flanger]")
{
    Settings settings;
    settings.keytrack = 100.0f;
    const double atC3 = Response(settings, 0.0f).extremum(150.0, 400.0, true);
    const double octaveUp = Response(settings, 12.0f).extremum(300.0, 800.0, true);
    INFO(atC3 << " Hz, " << octaveUp << " Hz");
    REQUIRE(std::abs(octaveUp - 2.0 * atC3) < 0.04 * 2.0 * atC3);

    settings.keytrack = 0.0f;
    REQUIRE(std::abs(Response(settings, 12.0f).extremum(150.0, 400.0, true) - atC3) < 0.02 * atC3);
}

TEST_CASE("Feedback of either sign puts the peaks where it should and stays within +6 dB",
          "[dsp][fx][flanger]")
{
    Settings positive;
    positive.feedback = 95.0f;
    Settings negative;
    negative.feedback = -95.0f;

    // Positive feedback rings at the multiples of 1 / D (500, 1000 Hz), negative at the odd multiples
    // of 1 / (2 D) (250, 750 Hz).
    const Response up(positive), down(negative);
    REQUIRE(std::abs(up.extremum(400.0, 600.0, false) - 500.0) < 15.0);
    REQUIRE(std::abs(up.extremum(900.0, 1100.0, false) - 1000.0) < 25.0);
    REQUIRE(std::abs(down.extremum(200.0, 300.0, false) - 250.0) < 10.0);
    REQUIRE(std::abs(down.extremum(650.0, 850.0, false) - 750.0) < 25.0);

    for (const auto* settings : {&positive, &negative})
    {
        Settings swept = *settings;
        swept.rate = 3.0f;
        swept.depth = 100.0f;
        std::vector<float> sweep(kBlock * 600);
        double phase = 0.0;
        for (size_t i = 0; i < sweep.size(); ++i)
        {
            phase += 2.0 * std::numbers::pi * 30.0 * std::pow(600.0, static_cast<double>(i) / sweep.size()) /
                     kRate;
            sweep[i] = static_cast<float>(std::sin(phase));
        }
        auto effect = makeEffect(swept);
        const auto out = run(effect, sweep, {{0}});
        REQUIRE(fcm::test::allFinite(out));
        INFO("peak " << fcm::test::peak(out));
        REQUIRE(fcm::test::peak(out) < 2.05f);
    }
}

TEST_CASE("The sweep starts again at every note, at the sample of the note", "[dsp][fx][flanger]")
{
    Settings settings;
    settings.rate = 1.0f;
    settings.depth = 100.0f;
    settings.feedback = 40.0f;
    const auto burst = noiseBurst(kBlock * 12, 7);

    std::vector<float> input(kBlock * 400, 0.0f);
    const int second = kBlock * 200;
    std::copy(burst.begin(), burst.end(), input.begin());
    std::copy(burst.begin(), burst.end(), input.begin() + second);
    auto effect = makeEffect(settings);
    const auto out = run(effect, input, {{0}, {second}});
    float worst = 0.0f;
    for (size_t i = 0; i < burst.size(); ++i)
        worst = std::max(worst, std::abs(out[i] - out[static_cast<size_t>(second) + i]));
    INFO("difference " << worst);
    REQUIRE(worst < 1.0e-6f);
    REQUIRE(fcm::test::peak(out) > 0.05f);

    auto free = makeEffect(settings);
    const auto without = run(free, input, {{0}});
    float apart = 0.0f;
    for (size_t i = 0; i < burst.size(); ++i)
        apart = std::max(apart, std::abs(without[i] - without[static_cast<size_t>(second) + i]));
    REQUIRE(apart > 0.01f);

    // A note-on at offset 100 of a block is the same as a block that starts there.
    auto warm = noiseBurst(kBlock * 8, 3);
    auto longInput = warm;
    longInput.insert(longInput.end(), burst.begin(), burst.begin() + kBlock);
    auto inside = makeEffect(settings);
    auto split = makeEffect(settings);
    const auto a = run(inside, longInput, {{kBlock * 8 + 100}});

    std::vector<float> b =
        run(split, std::vector<float>(longInput.begin(), longInput.begin() + kBlock * 8), {});
    const auto process = [&](size_t from, size_t count, bool note)
    {
        juce::AudioBuffer<float> buffer(2, static_cast<int>(count));
        for (int c = 0; c < 2; ++c)
            for (size_t i = 0; i < count; ++i)
                buffer.setSample(c, static_cast<int>(i), longInput[from + i]);
        if (note)
            split.noteOn(0);
        split.process(buffer);
        b.insert(b.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + count);
    };
    process(kBlock * 8, 100, false);
    process(kBlock * 8 + 100, longInput.size() - (kBlock * 8 + 100), true);
    REQUIRE(a.size() == b.size());
    REQUIRE(a == b);
}

TEST_CASE("Sync follows the tempo: 1/4 at 120 BPM is 2 Hz, and a tempo change is followed",
          "[dsp][fx][flanger]")
{
    Settings synced;
    synced.sync = 1.0f;
    synced.depth = 100.0f;
    synced.feedback = 50.0f;
    Settings free = synced;
    free.sync = 0.0f;
    free.rate = 2.0f;
    const auto input = noiseBurst(kBlock * 400, 11);

    auto a = makeEffect(synced);
    auto b = makeEffect(free);
    REQUIRE(run(a, input, {{0}}, 120.0f) == run(b, input, {{0}}, 120.0f));

    const std::vector<float> first(input.begin(), input.begin() + kBlock * 200);
    const std::vector<float> second(input.begin() + kBlock * 200, input.end());
    auto c = makeEffect(synced);
    auto d = makeEffect(free);
    auto outC = run(c, first, {{0}}, 120.0f);
    auto outD = run(d, first, {{0}}, 120.0f);
    Settings faster = free;
    faster.rate = 4.0f;
    d.setParameters(faster.values());
    const auto tailC = run(c, second, {}, 240.0f);
    const auto tailD = run(d, second, {}, 240.0f);
    outC.insert(outC.end(), tailC.begin(), tailC.end());
    outD.insert(outD.end(), tailD.begin(), tailD.end());
    REQUIRE(outC == outD);
}

TEST_CASE("A sweep of the delay from 10 ms to 0.1 ms makes no step larger than the signal's",
          "[dsp][fx][flanger]")
{
    // Three seconds, a block at a time; the delay shortens by 2 to 3 % of itself per second at most,
    // so the copy's pitch shifts by that much and no step can be much larger than the signal's.
    Settings settings;
    settings.delay = 10.0f;
    auto effect = makeEffect(settings);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    constexpr int kBlocks = 560;
    for (int block = 0; block < kBlocks; ++block)
    {
        settings.delay = 10.0f * std::pow(0.01f, static_cast<float>(block) / kBlocks);
        effect.setParameters(settings.values());
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i,
                                 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 *
                                                                    (block * kBlock + i) / kRate)));
        effect.setContext({0.0f, 120.0f});
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    const double inputStep = 0.5 * 2.0 * std::numbers::pi * 1000.0 / kRate;
    float most = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        most = std::max(most, std::abs(out[i] - out[i - 1]));
    INFO("largest step " << most << " against the signal's " << inputStep);
    REQUIRE(most <= 1.1 * inputStep);
}

TEST_CASE("The flanger is deterministic after a reset, has no latency and survives hostile values",
          "[dsp][fx][flanger]")
{
    Settings settings;
    settings.depth = 80.0f;
    settings.rate = 2.0f;
    auto effect = makeEffect(settings);
    REQUIRE(effect.latencySamples(8) == 0);

    const auto input = noiseBurst(kBlock * 20, 5);
    const auto first = run(effect, input, {{0}});
    effect.reset();
    effect.setParameters(settings.values());
    REQUIRE(run(effect, input, {{0}}) == first);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const auto& values :
         {std::vector<float>(7, nan), std::vector<float>(7, inf), std::vector<float>(7, -inf),
          std::vector<float>(7, 1.0e9f), std::vector<float>(7, -1.0e9f)})
    {
        FlangerEffect hostile;
        hostile.prepare({kRate, kBlock, 2});
        hostile.setParameters(values);
        hostile.setContext({nan, nan});
        hostile.noteOn(-5);
        hostile.noteOn(100000);
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int i = 0; i < kBlock; ++i)
        {
            const float sample = i == 3 ? nan : 0.5f * static_cast<float>(std::sin(0.05 * i));
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }
        hostile.process(buffer);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                REQUIRE(std::isfinite(buffer.getSample(c, i)));
    }
}

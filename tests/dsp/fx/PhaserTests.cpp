// SPDX-License-Identifier: AGPL-3.0-only
#include "../TestUtils.h"
#include "dsp/fx/phaser/PhaserEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using fcm::dsp::EffectContext;
using fcm::dsp::PhaserEffect;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

struct Settings
{
    float stages = 1.0f; // the choice: 0, 1, 2 for 4, 8, 12
    float freq = 300.0f;
    float sync = 0.0f;
    float rate = 0.0f;
    float division = 9.0f; // 1/4
    float depth = 0.0f;
    float feedback = 0.0f;
    float keytrack = 0.0f;
    std::vector<float> values() const
    {
        return {stages, freq, sync, rate, division, depth, feedback, keytrack};
    }
};

// A still phaser (Rate 0, Depth 0) seen through its impulse response.
struct Response
{
    std::vector<float> impulse;

    explicit Response(const Settings& settings, float noteSemitones = 0.0f)
    {
        PhaserEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(settings.values());
        effect.setContext({noteSemitones, 120.0f});
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < 128; ++block)
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

    double magnitudeDb(double hz) const
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < impulse.size(); ++i)
        {
            const double w = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / kRate;
            re += impulse[i] * std::cos(w);
            im -= impulse[i] * std::sin(w);
        }
        return 20.0 * std::log10(std::max(std::sqrt(re * re + im * im), 1.0e-9));
    }

    // The notches: local minima of the response below -20 dB, lowest first, each refined by a search.
    std::vector<double> notches() const
    {
        std::vector<double> found;
        std::vector<double> hz, level;
        for (double f = 30.0; f < 20000.0; f *= 1.004)
        {
            hz.push_back(f);
            level.push_back(magnitudeDb(f));
        }
        for (size_t i = 1; i + 1 < hz.size(); ++i)
            if (level[i] < level[i - 1] && level[i] <= level[i + 1] && level[i] < -20.0)
            {
                double low = hz[i - 1], high = hz[i + 1];
                for (int k = 0; k < 40; ++k)
                {
                    const double a = low + (high - low) / 3.0, b = high - (high - low) / 3.0;
                    (magnitudeDb(a) < magnitudeDb(b) ? high : low) =
                        (magnitudeDb(a) < magnitudeDb(b) ? b : a);
                }
                found.push_back(0.5 * (low + high));
            }
        return found;
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

// Processes `input` in blocks, telling the effect the notes (the block they fall in, at their offset)
// and the tempo of each block. `change` can alter the settings between blocks.
std::vector<float> run(PhaserEffect& effect, const std::vector<float>& input, const std::vector<Note>& notes,
                       float tempo = 120.0f, int block = kBlock)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, block);
    for (size_t start = 0; start + static_cast<size_t>(block) <= input.size();
         start += static_cast<size_t>(block))
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.setContext({0.0f, tempo});
        for (const auto& note : notes)
            if (note.at >= static_cast<int>(start) && note.at < static_cast<int>(start) + block)
                effect.noteOn(note.at - static_cast<int>(start));
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + block);
    }
    return out;
}
} // namespace

TEST_CASE("A still phaser has stages / 2 deep notches, the first at Frequency", "[dsp][fx][phaser]")
{
    for (const float choice : {0.0f, 1.0f, 2.0f})
    {
        Settings settings;
        settings.stages = choice;
        settings.freq = 100.0f;
        const auto notches = Response(settings).notches();
        const int stages = PhaserEffect::stagesOf(static_cast<int>(choice));
        INFO(stages << " stages: " << notches.size() << " notches");
        REQUIRE(static_cast<int>(notches.size()) == stages / 2);
        REQUIRE(std::abs(notches.front() - 100.0) < 5.0);
        Response response(settings);
        REQUIRE(response.magnitudeDb(notches.front()) < -30.0);
    }
}

TEST_CASE("The sweep starts again at every note, at the sample of the note", "[dsp][fx][phaser]")
{
    Settings settings;
    settings.rate = 1.0f;
    settings.depth = 100.0f;
    settings.feedback = 40.0f;
    const auto burst = noiseBurst(kBlock * 12, 7);

    // Two notes far apart, the same input after each: the same output.
    std::vector<float> input(kBlock * 400, 0.0f);
    const int second = kBlock * 200;
    std::copy(burst.begin(), burst.end(), input.begin());
    std::copy(burst.begin(), burst.end(), input.begin() + second);
    PhaserEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());
    const auto out = run(effect, input, {{0}, {second}});
    float worst = 0.0f;
    for (size_t i = 0; i < burst.size(); ++i)
        worst = std::max(worst, std::abs(out[i] - out[static_cast<size_t>(second) + i]));
    INFO("difference " << worst);
    REQUIRE(worst < 1.0e-6f);
    REQUIRE(fcm::test::peak(out) > 0.05f);

    // Without the second note-on the sweep would be somewhere else.
    PhaserEffect free;
    free.prepare({kRate, kBlock, 2});
    free.setParameters(settings.values());
    const auto without = run(free, input, {{0}});
    float apart = 0.0f;
    for (size_t i = 0; i < burst.size(); ++i)
        apart = std::max(apart, std::abs(without[i] - without[static_cast<size_t>(second) + i]));
    REQUIRE(apart > 0.01f);

    // A note-on at offset 100 of a block is the same as a block that starts there.
    const auto warm = noiseBurst(kBlock * 8, 3);
    auto longInput = warm;
    longInput.insert(longInput.end(), burst.begin(), burst.begin() + kBlock);
    PhaserEffect inside, split;
    for (auto* effect : {&inside, &split})
    {
        effect->prepare({kRate, kBlock, 2});
        effect->setParameters(settings.values());
    }
    const auto a = run(inside, longInput, {{kBlock * 8 + 100}});

    std::vector<float> beforeNote(longInput.begin(), longInput.begin() + kBlock * 8 + 100);
    std::vector<float> afterNote(longInput.begin() + kBlock * 8 + 100, longInput.end());
    // The same input in blocks of 4 samples around the note keeps the block grid out of the way.
    const auto b1 = run(split, std::vector<float>(beforeNote.begin(), beforeNote.begin() + (kBlock * 8)), {});
    juce::AudioBuffer<float> buffer(2, 100);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 100; ++i)
            buffer.setSample(c, i, beforeNote[static_cast<size_t>(kBlock * 8 + i)]);
    split.process(buffer);
    std::vector<float> b = b1;
    b.insert(b.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 100);
    juce::AudioBuffer<float> rest(2, static_cast<int>(afterNote.size()));
    for (int c = 0; c < 2; ++c)
        for (size_t i = 0; i < afterNote.size(); ++i)
            rest.setSample(c, static_cast<int>(i), afterNote[i]);
    split.noteOn(0);
    split.process(rest);
    b.insert(b.end(), rest.getReadPointer(0), rest.getReadPointer(0) + afterNote.size());
    REQUIRE(a.size() == b.size());
    REQUIRE(a == b);
}

TEST_CASE("Sync follows the tempo: 1/4 at 120 BPM is 2 Hz, and a tempo change is followed",
          "[dsp][fx][phaser]")
{
    Settings synced;
    synced.sync = 1.0f;
    synced.depth = 100.0f;
    synced.feedback = 50.0f;
    Settings free = synced;
    free.sync = 0.0f;
    free.rate = 2.0f;
    const auto input = noiseBurst(kBlock * 400, 11);

    PhaserEffect a, b;
    for (auto* effect : {&a, &b})
        effect->prepare({kRate, kBlock, 2});
    a.setParameters(synced.values());
    b.setParameters(free.values());
    REQUIRE(run(a, input, {{0}}, 120.0f) == run(b, input, {{0}}, 120.0f));

    // 240 BPM from the second half on: 4 Hz.
    const std::vector<float> first(input.begin(), input.begin() + kBlock * 200);
    const std::vector<float> second(input.begin() + kBlock * 200, input.end());
    PhaserEffect c, d;
    for (auto* effect : {&c, &d})
        effect->prepare({kRate, kBlock, 2});
    c.setParameters(synced.values());
    d.setParameters(free.values());
    auto outC = run(c, first, {{0}}, 120.0f);
    auto outD = run(d, first, {{0}}, 120.0f);
    Settings faster = free;
    faster.rate = 4.0f;
    d.setParameters(faster.values());
    const auto tailC = run(c, second, {}, 240.0f);
    const auto tailD = run(d, second, {}, 240.0f);
    outC.insert(outC.end(), tailC.begin(), tailC.end());
    outD.insert(outD.end(), tailD.begin(), tailD.end());
    REQUIRE(outC.size() == outD.size());
    REQUIRE(outC == outD);
}

TEST_CASE("Key Track 100 % moves the notch up an octave when the note goes up an octave", "[dsp][fx][phaser]")
{
    Settings settings;
    settings.freq = 200.0f;
    settings.keytrack = 100.0f;
    const double atC3 = Response(settings, 0.0f).notches().front();
    const double octaveUp = Response(settings, 12.0f).notches().front();
    INFO(atC3 << " Hz, " << octaveUp << " Hz");
    REQUIRE(std::abs(atC3 - 200.0) < 10.0);
    REQUIRE(std::abs(octaveUp - 2.0 * atC3) < 0.05 * 2.0 * atC3);

    settings.keytrack = 0.0f;
    REQUIRE(std::abs(Response(settings, 12.0f).notches().front() - atC3) < 2.0);
}

TEST_CASE("Feedback 95 % stays within +6 dB on a full-scale sweep", "[dsp][fx][phaser]")
{
    for (const float choice : {0.0f, 1.0f, 2.0f})
    {
        Settings settings;
        settings.stages = choice;
        settings.feedback = 95.0f;
        settings.rate = 3.0f;
        settings.depth = 100.0f;
        std::vector<float> sweep(kBlock * 600);
        double phase = 0.0;
        for (size_t i = 0; i < sweep.size(); ++i)
        {
            phase += 2.0 * std::numbers::pi * 30.0 * std::pow(600.0, static_cast<double>(i) / sweep.size()) /
                     kRate;
            sweep[i] = static_cast<float>(std::sin(phase));
        }
        PhaserEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(settings.values());
        const auto out = run(effect, sweep, {{0}});
        REQUIRE(fcm::test::allFinite(out));
        INFO("peak " << fcm::test::peak(out));
        REQUIRE(fcm::test::peak(out) < 2.05f);
    }
}

TEST_CASE("Changing Stages while playing does not click", "[dsp][fx][phaser]")
{
    Settings settings;
    PhaserEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    constexpr int kBlocks = 120, kChangeAt = 60;
    for (int block = 0; block < kBlocks; ++block)
    {
        if (block == kChangeAt)
        {
            settings.stages = 2.0f;
            effect.setParameters(settings.values());
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i,
                                 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 700.0 *
                                                                    (block * kBlock + i) / kRate)));
        effect.setContext({0.0f, 120.0f});
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    const auto largestStep = [&](size_t from, size_t to)
    {
        float most = 0.0f;
        for (size_t i = from + 1; i < to; ++i)
            most = std::max(most, std::abs(out[i] - out[i - 1]));
        return most;
    };
    const size_t change = static_cast<size_t>(kChangeAt) * kBlock;
    const float before = largestStep(change - 4096, change);
    const float after = largestStep(out.size() - 4096, out.size());
    const float atChange = largestStep(change - 1, change + 64);
    INFO("steps: before " << before << ", at the change " << atChange << ", after " << after);
    REQUIRE(atChange <= 1.2f * std::max(before, after));
}

TEST_CASE("The phaser is deterministic after a reset, has no latency and survives hostile values",
          "[dsp][fx][phaser]")
{
    PhaserEffect effect;
    effect.prepare({kRate, kBlock, 2});
    REQUIRE(effect.latencySamples(8) == 0);

    Settings settings;
    settings.depth = 80.0f;
    settings.rate = 2.0f;
    const auto input = noiseBurst(kBlock * 20, 5);
    effect.setParameters(settings.values());
    const auto first = run(effect, input, {{0}});
    effect.reset();
    effect.setParameters(settings.values());
    REQUIRE(run(effect, input, {{0}}) == first);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const auto& values :
         {std::vector<float>(8, nan), std::vector<float>(8, inf), std::vector<float>(8, -inf),
          std::vector<float>(8, 1.0e9f), std::vector<float>(8, -1.0e9f)})
    {
        PhaserEffect hostile;
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

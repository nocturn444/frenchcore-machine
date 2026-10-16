// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/transient/TransientEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::TransientEffect;

namespace
{
constexpr int kBlock = 256;
constexpr double kRate = 48000.0;

struct Settings
{
    float attack = 0.0f;
    float sustain = 0.0f;
    float speed = 50.0f;
    float output = 0.0f;

    std::vector<float> values() const { return {attack, sustain, speed, output}; }
};

std::vector<float> run(const Settings& settings, const std::vector<float>& input)
{
    TransientEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    return out;
}

// A kick: a 60 Hz sine with a 1 ms attack and an exponential decay of 80 ms, at `amplitude`.
std::vector<float> kick(float amplitude, double seconds = 0.5)
{
    std::vector<float> out(static_cast<size_t>(seconds * kRate) / kBlock * kBlock);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / kRate;
        const double envelope = std::min(1.0, t / 0.001) * std::exp(-t / 0.08);
        out[i] = amplitude * static_cast<float>(envelope * std::sin(2.0 * std::numbers::pi * 60.0 * t));
    }
    return out;
}

double peakBetween(const std::vector<float>& x, double fromSeconds, double toSeconds)
{
    double peak = 0.0;
    for (auto i = static_cast<size_t>(fromSeconds * kRate); i < static_cast<size_t>(toSeconds * kRate); ++i)
        peak = std::max(peak, static_cast<double>(std::abs(x[i])));
    return peak;
}

double energyAfter(const std::vector<float>& x, double seconds)
{
    double energy = 0.0;
    for (auto i = static_cast<size_t>(seconds * kRate); i < x.size(); ++i)
        energy += static_cast<double>(x[i]) * x[i];
    return energy;
}

double db(double ratio)
{
    return 20.0 * std::log10(ratio);
}
} // namespace

TEST_CASE("Neutral settings change nothing at all", "[dsp][fx][transient]")
{
    juce::Random random(8);
    std::vector<float> input(kBlock * 30);
    for (auto& sample : input)
        sample = random.nextFloat() * 2.0f - 1.0f;
    REQUIRE(run(Settings{}, input) == input);
}

TEST_CASE("The parameters describe the spec and there is no latency", "[dsp][fx][transient]")
{
    TransientEffect effect;
    REQUIRE(effect.descriptor().id == "transient");
    REQUIRE(effect.parameters().size() == 4);
    REQUIRE(effect.parameters()[2].defaultValue == 50.0f);
    for (const int factor : {1, 2, 4, 8})
        REQUIRE(effect.latencySamples(factor) == 0);
}

TEST_CASE("Attack raises or lowers the start of a hit", "[dsp][fx][transient]")
{
    const auto input = kick(0.8f);
    const double base = peakBetween(input, 0.0, 0.005);
    const double tailBase = peakBetween(input, 0.1, 0.2);

    Settings up;
    up.attack = 100.0f;
    const auto boosted = run(up, input);
    Settings down;
    down.attack = -100.0f;
    const auto softened = run(down, input);

    // The first 5 ms against the tail: more with Attack up, less with Attack down.
    const double ratioBase = base / tailBase;
    const double ratioUp = peakBetween(boosted, 0.0, 0.005) / peakBetween(boosted, 0.1, 0.2);
    const double ratioDown = peakBetween(softened, 0.0, 0.005) / peakBetween(softened, 0.1, 0.2);
    REQUIRE(db(ratioUp) > db(ratioBase) + 3.0);
    REQUIRE(db(ratioDown) < db(ratioBase) - 3.0);
}

TEST_CASE("Sustain raises or lowers what rings after the hit", "[dsp][fx][transient]")
{
    const auto input = kick(0.8f);
    const double base = energyAfter(input, 0.05);

    Settings up;
    up.sustain = 100.0f;
    Settings down;
    down.sustain = -100.0f;

    REQUIRE(energyAfter(run(up, input), 0.05) > base * 2.0);
    REQUIRE(energyAfter(run(down, input), 0.05) < base * 0.5);
}

TEST_CASE("A quiet kick is shaped like a loud one", "[dsp][fx][transient]")
{
    Settings settings;
    settings.attack = 80.0f;
    settings.sustain = -60.0f;

    const auto loud = kick(0.8f);
    const auto quiet = kick(0.08f); // 20 dB lower
    const auto loudOut = run(settings, loud);
    const auto quietOut = run(settings, quiet);

    // The gain applied, in dB, in a few places over the first 150 ms (where both are far above silence).
    for (const double t : {0.003, 0.01, 0.03, 0.06, 0.1})
    {
        const auto from = static_cast<size_t>(t * kRate);
        const auto to =
            from + static_cast<size_t>(0.004 * kRate); // a whole 60 Hz cycle is 16 ms: use the peak
        double loudGain = 0.0, quietGain = 0.0;
        for (size_t i = from; i < to + static_cast<size_t>(0.012 * kRate); ++i)
        {
            if (std::abs(loud[i]) > 0.02f)
                loudGain = std::max(loudGain, static_cast<double>(std::abs(loudOut[i]) / std::abs(loud[i])));
            if (std::abs(quiet[i]) > 0.002f)
                quietGain =
                    std::max(quietGain, static_cast<double>(std::abs(quietOut[i]) / std::abs(quiet[i])));
        }
        INFO("at " << t << " s");
        REQUIRE_THAT(db(quietGain), WithinAbs(db(loudGain), 1.0));
    }
}

TEST_CASE("Output scales the level", "[dsp][fx][transient]")
{
    Settings settings;
    settings.output = -6.0206f;
    const auto input = kick(0.8f);
    const auto out = run(settings, input);
    for (size_t i = kBlock * 4; i < input.size(); ++i)
        REQUIRE_THAT(out[i], WithinAbs(input[i] * 0.5, 1.0e-5));
}

TEST_CASE("The effect stays finite and bounded at the extremes", "[dsp][fx][transient]")
{
    juce::Random random(15);
    for (const float attack : {-100.0f, 0.0f, 100.0f})
        for (const float sustain : {-100.0f, 0.0f, 100.0f})
            for (const float speed : {0.0f, 50.0f, 100.0f})
            {
                Settings settings;
                settings.attack = attack;
                settings.sustain = sustain;
                settings.speed = speed;
                settings.output = 12.0f;

                std::vector<float> input(kBlock * 80);
                for (size_t i = 0; i < input.size(); ++i)
                {
                    const bool burst = (i / (kBlock * 8)) % 2 == 0;
                    input[i] = burst ? random.nextFloat() * 2.0f - 1.0f : 0.0f; // loud noise, then silence
                }
                for (float sample : run(settings, input))
                {
                    REQUIRE(std::isfinite(sample));
                    // +24 dB of shaping and +12 dB of output on a full-scale input.
                    REQUIRE(std::abs(sample) < 70.0f);
                }
            }
}

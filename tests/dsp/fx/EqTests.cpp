// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/eq/EqEffect.h"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::EqEffect;

namespace
{
constexpr int kBlock = 256;

struct Band
{
    float freq;
    float gain;
    float q;
};

struct Settings
{
    Band low{100.0f, 0.0f, 0.71f};
    Band mid{500.0f, 0.0f, 1.0f};
    Band high{8000.0f, 0.0f, 0.71f};

    std::vector<float> values() const
    {
        return {low.freq, low.gain, low.q, mid.freq, mid.gain, mid.q, high.freq, high.gain, high.q};
    }
};

std::vector<float> run(EqEffect& effect, const std::vector<float>& input)
{
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

// The gain in dB of the effect at one frequency, measured with a sine.
double gainDbAt(const Settings& settings, double hz, double sampleRate = 48000.0)
{
    EqEffect effect;
    effect.prepare({sampleRate, kBlock, 2});
    effect.setParameters(settings.values());

    const int total = static_cast<int>(sampleRate) / kBlock * kBlock; // about one second
    std::vector<float> input(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i)
        input[static_cast<size_t>(i)] =
            0.1f * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / sampleRate));

    const auto out = run(effect, input);
    double inEnergy = 0.0, outEnergy = 0.0;
    for (size_t i = static_cast<size_t>(total) / 2; i < out.size(); ++i)
    {
        inEnergy += static_cast<double>(input[i]) * input[i];
        outEnergy += static_cast<double>(out[i]) * out[i];
    }
    return 10.0 * std::log10(outEnergy / inEnergy);
}
} // namespace

TEST_CASE("A flat EQ changes nothing at all", "[dsp][fx][eq]")
{
    EqEffect effect;
    effect.prepare({48000.0, kBlock, 2});
    effect.setParameters(Settings{}.values());

    juce::Random random(5);
    std::vector<float> input(kBlock * 20);
    for (auto& s : input)
        s = random.nextFloat() * 2.0f - 1.0f;
    REQUIRE(run(effect, input) == input);
}

TEST_CASE("The low shelf boosts and cuts below its frequency", "[dsp][fx][eq]")
{
    Settings boost;
    boost.low = {100.0f, 12.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(boost, 20.0), WithinAbs(12.0, 0.5));
    REQUIRE_THAT(gainDbAt(boost, 2000.0), WithinAbs(0.0, 0.5));
    REQUIRE_THAT(gainDbAt(boost, 100.0), WithinAbs(6.0, 0.7)); // half the gain at the corner

    Settings cut;
    cut.low = {100.0f, -12.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(cut, 20.0), WithinAbs(-12.0, 0.5));
    REQUIRE_THAT(gainDbAt(cut, 2000.0), WithinAbs(0.0, 0.5));
}

TEST_CASE("The peak boosts around its frequency, narrower with a higher Q", "[dsp][fx][eq]")
{
    Settings bell;
    bell.mid = {1000.0f, 12.0f, 1.0f};
    REQUIRE_THAT(gainDbAt(bell, 1000.0), WithinAbs(12.0, 0.5)); // the full gain at the centre
    REQUIRE_THAT(gainDbAt(bell, 100.0), WithinAbs(0.0, 0.7));
    REQUIRE_THAT(gainDbAt(bell, 10000.0), WithinAbs(0.0, 0.7));

    Settings wide = bell;
    wide.mid.q = 0.5f;
    Settings narrow = bell;
    narrow.mid.q = 8.0f;
    REQUIRE(gainDbAt(wide, 1500.0) > gainDbAt(narrow, 1500.0) + 4.0);
}

TEST_CASE("The high shelf boosts and cuts above its frequency", "[dsp][fx][eq]")
{
    Settings boost;
    boost.high = {8000.0f, 12.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(boost, 16000.0), WithinAbs(12.0, 0.8));
    REQUIRE_THAT(gainDbAt(boost, 500.0), WithinAbs(0.0, 0.5));
    REQUIRE_THAT(gainDbAt(boost, 8000.0), WithinAbs(6.0, 0.8));
}

TEST_CASE("Bands add up in dB when they are far apart", "[dsp][fx][eq]")
{
    Settings all;
    all.low = {80.0f, 6.0f, 0.71f};
    all.mid = {1000.0f, -6.0f, 1.0f};
    all.high = {10000.0f, 4.0f, 0.71f};
    REQUIRE_THAT(gainDbAt(all, 20.0), WithinAbs(6.0, 0.8));
    REQUIRE_THAT(gainDbAt(all, 1000.0), WithinAbs(-6.0, 0.8));
    REQUIRE_THAT(gainDbAt(all, 20000.0), WithinAbs(4.0, 0.8));
}

TEST_CASE("The design is stable and bounded at every extreme", "[dsp][fx][eq]")
{
    juce::Random random(11);
    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const float freq :
             {-5.0f, 1.0f, 20.0f, 20000.0f, 90000.0f, std::numeric_limits<float>::quiet_NaN()})
            for (const float q : {0.0f, 0.1f, 10.0f, 500.0f, std::numeric_limits<float>::infinity()})
                for (const float gain : {-60.0f, -18.0f, 18.0f, 60.0f})
                {
                    Settings settings;
                    settings.low = {freq, gain, q};
                    settings.mid = {freq, gain, q};
                    settings.high = {freq, gain, q};

                    EqEffect effect;
                    effect.prepare({rate, kBlock, 2});
                    effect.setParameters(settings.values());

                    // Noise, then a loud kick-like burst.
                    std::vector<float> input(kBlock * 16);
                    for (size_t i = 0; i < input.size(); ++i)
                        input[i] = i < kBlock * 8
                                       ? random.nextFloat() * 2.0f - 1.0f
                                       : static_cast<float>(std::sin(2.0 * std::numbers::pi * 50.0 *
                                                                     static_cast<double>(i) / rate));
                    for (float s : run(effect, input))
                    {
                        REQUIRE(std::isfinite(s));
                        // Three bands at +30 dB each can add up to +90 dB, so the bound is wide.
                        REQUIRE(std::abs(s) < 1.0e6f);
                    }
                }
}

TEST_CASE("Changing the parameters every block stays bounded", "[dsp][fx][eq]")
{
    juce::Random random(21);
    EqEffect effect;
    effect.prepare({48000.0, kBlock, 2});

    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < 400; ++block)
    {
        Settings settings;
        settings.low = {20.0f + random.nextFloat() * 480.0f, random.nextFloat() * 36.0f - 18.0f,
                        0.3f + random.nextFloat() * 1.7f};
        settings.mid = {60.0f + random.nextFloat() * 7940.0f, random.nextFloat() * 36.0f - 18.0f,
                        0.2f + random.nextFloat() * 9.8f};
        settings.high = {1000.0f + random.nextFloat() * 19000.0f, random.nextFloat() * 36.0f - 18.0f,
                         0.3f + random.nextFloat() * 1.7f};
        effect.setParameters(settings.values());

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, 0.5f * (random.nextFloat() * 2.0f - 1.0f));
        effect.process(buffer);
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(std::abs(buffer.getSample(0, i)) < 50.0f);
    }
}

TEST_CASE("The EQ works on one channel", "[dsp][fx][eq]")
{
    EqEffect effect;
    effect.prepare({48000.0, kBlock, 1});
    Settings settings;
    settings.low = {100.0f, 12.0f, 0.71f};
    effect.setParameters(settings.values());

    juce::AudioBuffer<float> mono(1, kBlock);
    for (int i = 0; i < kBlock; ++i)
        mono.setSample(0, i, 0.1f);
    effect.process(mono);
    for (int i = 0; i < kBlock; ++i)
        REQUIRE(std::isfinite(mono.getSample(0, i)));
}

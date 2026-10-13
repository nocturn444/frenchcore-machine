// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/stereo/StereoEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <complex>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::StereoEffect;

namespace
{
constexpr int kBlock = 256;

struct Settings
{
    float xover1 = 120.0f;
    float xover2 = 2500.0f;
    float low = 100.0f;
    float mid = 100.0f;
    float high = 100.0f;
    bool monoBelow = false;

    std::vector<float> values() const { return {xover1, xover2, low, mid, high, monoBelow ? 1.0f : 0.0f}; }
};

struct Stereo
{
    std::vector<float> left, right;
};

Stereo run(const Settings& settings, const Stereo& input, double sampleRate = 48000.0)
{
    StereoEffect effect;
    effect.prepare({sampleRate, kBlock, 2});
    effect.setParameters(settings.values());

    Stereo out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0; start + kBlock <= input.left.size(); start += kBlock)
    {
        for (int i = 0; i < kBlock; ++i)
        {
            buffer.setSample(0, i, input.left[start + static_cast<size_t>(i)]);
            buffer.setSample(1, i, input.right[start + static_cast<size_t>(i)]);
        }
        effect.process(buffer);
        out.left.insert(out.left.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        out.right.insert(out.right.end(), buffer.getReadPointer(1), buffer.getReadPointer(1) + kBlock);
    }
    return out;
}

Stereo sineStereo(double hz, double sampleRate, float leftAmplitude, float rightAmplitude, int blocks = 100)
{
    Stereo s;
    const int total = blocks * kBlock;
    for (int i = 0; i < total; ++i)
    {
        const float v = static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / sampleRate));
        s.left.push_back(v * leftAmplitude);
        s.right.push_back(v * rightAmplitude);
    }
    return s;
}

double energyDb(const std::vector<float>& x, size_t from)
{
    double e = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        e += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(std::max(e, 1.0e-30));
}

std::vector<float> difference(const Stereo& s)
{
    std::vector<float> d(s.left.size());
    for (size_t i = 0; i < d.size(); ++i)
        d[i] = s.left[i] - s.right[i];
    return d;
}

double correlation(const Stereo& s, size_t from)
{
    double lr = 0.0, ll = 0.0, rr = 0.0;
    for (size_t i = from; i < s.left.size(); ++i)
    {
        lr += static_cast<double>(s.left[i]) * s.right[i];
        ll += static_cast<double>(s.left[i]) * s.left[i];
        rr += static_cast<double>(s.right[i]) * s.right[i];
    }
    return lr / std::sqrt(std::max(ll * rr, 1.0e-30));
}

Stereo noiseStereo(int blocks, bool mono, unsigned seed = 3)
{
    juce::Random random(static_cast<juce::int64>(seed));
    Stereo s;
    for (int i = 0; i < blocks * kBlock; ++i)
    {
        const float l = 0.5f * (random.nextFloat() * 2.0f - 1.0f);
        const float r = mono ? l : 0.5f * (random.nextFloat() * 2.0f - 1.0f);
        s.left.push_back(l);
        s.right.push_back(r);
    }
    return s;
}
} // namespace

TEST_CASE("With every width at 100 percent the level is flat across the spectrum", "[dsp][fx][stereo]")
{
    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const double hz : {25.0, 120.0, 1000.0, 2500.0, 10000.0, 18000.0})
        {
            const auto input = sineStereo(hz, rate, 0.5f, 0.5f, 120);
            const auto out = run(Settings{}, input, rate);
            const size_t from = out.left.size() / 2;
            INFO("rate " << rate << " hz " << hz);
            REQUIRE_THAT(energyDb(out.left, from) - energyDb(input.left, from), WithinAbs(0.0, 0.3));
            for (size_t i = from; i < out.left.size(); ++i)
                REQUIRE(out.left[i] == out.right[i]); // a mono input stays mono at 100 %
        }
}

TEST_CASE("Width 0 in every band makes a stereo input mono", "[dsp][fx][stereo]")
{
    Settings mono;
    mono.low = mono.mid = mono.high = 0.0f;
    for (const bool monoBelow : {false, true})
    {
        mono.monoBelow = monoBelow;
        const auto out = run(mono, noiseStereo(60, false));
        double maxDifference = 0.0;
        for (size_t i = kBlock * 4; i < out.left.size(); ++i)
            maxDifference =
                std::max(maxDifference, static_cast<double>(std::abs(out.left[i] - out.right[i])));
        REQUIRE(maxDifference < 1.0e-5);
    }
}

TEST_CASE("Width 100 percent keeps the stereo image", "[dsp][fx][stereo]")
{
    const auto input = sineStereo(800.0, 48000.0, 1.0f, 0.25f, 120); // left four times the right
    const auto out = run(Settings{}, input);
    const size_t from = out.left.size() / 2;
    REQUIRE_THAT(energyDb(out.left, from) - energyDb(out.right, from), WithinAbs(12.04, 0.3));
}

TEST_CASE("Width above 100 percent widens a mono input and keeps the level", "[dsp][fx][stereo]")
{
    Settings wide;
    wide.low = wide.mid = wide.high = 200.0f;
    wide.monoBelow = false;

    const auto input = noiseStereo(200, true);
    const auto out = run(wide, input);
    const size_t from = kBlock * 40;

    REQUIRE(correlation(out, from) < 0.85);
    // The loudness of the sides together stays close to the input's.
    const double total = 10.0 * std::log10(std::pow(10.0, energyDb(out.left, from) / 10.0) +
                                           std::pow(10.0, energyDb(out.right, from) / 10.0));
    const double reference = 10.0 * std::log10(2.0) + energyDb(input.left, from);
    REQUIRE_THAT(total, WithinAbs(reference, 2.0));
}

TEST_CASE("A band that is mono-ised does not touch the others", "[dsp][fx][stereo]")
{
    Settings highMono;
    highMono.high = 0.0f;

    // Content at 10 kHz in the left channel only becomes mono; at 100 Hz it stays as it was.
    const auto high = run(highMono, sineStereo(10000.0, 48000.0, 1.0f, 0.0f, 100));
    const auto low = run(highMono, sineStereo(100.0, 48000.0, 1.0f, 0.0f, 100));
    const size_t from = high.left.size() / 2;
    REQUIRE(energyDb(difference(high), from) < energyDb(high.left, from) - 40.0);
    REQUIRE(energyDb(low.right, from) < energyDb(low.left, from) - 40.0); // still only in the left channel
}

TEST_CASE("Mono Below keeps the low end mono whatever Low Width says", "[dsp][fx][stereo]")
{
    Settings below;
    below.low = 200.0f;
    below.monoBelow = true;

    const auto sub = run(below, sineStereo(50.0, 48000.0, 1.0f, 0.0f, 200));
    const auto treble = run(below, sineStereo(5000.0, 48000.0, 1.0f, 0.0f, 100));
    const size_t fromSub = sub.left.size() / 2;
    const size_t fromTreble = treble.left.size() / 2;
    // About 25 dB down: the mid and high bands keep a small residue of a 50 Hz tone (the crossover slope).
    REQUIRE(energyDb(difference(sub), fromSub) < energyDb(sub.left, fromSub) - 20.0);
    // At 5 kHz the stereo content stays stereo (only the left channel has it).
    REQUIRE(energyDb(treble.right, fromTreble) < energyDb(treble.left, fromTreble) - 40.0);
}

TEST_CASE("The crossover order is corrected and extremes stay bounded", "[dsp][fx][stereo]")
{
    juce::Random random(17);
    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const float x1 : {40.0f, 400.0f, 10000.0f, -3.0f, std::numeric_limits<float>::quiet_NaN()})
            for (const float x2 : {400.0f, 8000.0f, 50.0f, 90000.0f, std::numeric_limits<float>::infinity()})
                for (const float width : {0.0f, 100.0f, 200.0f, 5000.0f})
                {
                    Settings settings;
                    settings.xover1 = x1;
                    settings.xover2 = x2;
                    settings.low = settings.mid = settings.high = width;

                    Stereo input = noiseStereo(8, false, 9);
                    // then a loud low burst
                    const auto burst = sineStereo(50.0, rate, 1.0f, 0.3f, 8);
                    input.left.insert(input.left.end(), burst.left.begin(), burst.left.end());
                    input.right.insert(input.right.end(), burst.right.begin(), burst.right.end());

                    const auto out = run(settings, input, rate);
                    for (size_t i = 0; i < out.left.size(); ++i)
                    {
                        REQUIRE(std::isfinite(out.left[i]));
                        REQUIRE(std::isfinite(out.right[i]));
                        REQUIRE(std::abs(out.left[i]) < 20.0f);
                        REQUIRE(std::abs(out.right[i]) < 20.0f);
                    }
                }
}

TEST_CASE("A buffer with one channel is left untouched", "[dsp][fx][stereo]")
{
    StereoEffect effect;
    effect.prepare({48000.0, kBlock, 1});
    effect.setParameters(Settings{}.values());

    juce::AudioBuffer<float> mono(1, kBlock);
    for (int i = 0; i < kBlock; ++i)
        mono.setSample(0, i, 0.01f * static_cast<float>(i));
    effect.process(mono);
    for (int i = 0; i < kBlock; ++i)
        REQUIRE(mono.getSample(0, i) == 0.01f * static_cast<float>(i));
}

TEST_CASE("The Butterworth sections are low-pass and high-pass at their corner", "[dsp][fx][stereo]")
{
    // At the corner a second-order Butterworth is at -3 dB; two in series (Linkwitz-Riley) at -6 dB.
    const auto magnitudeAt = [](const StereoEffect::Biquad& k, double hz, double rate)
    {
        const double w = 2.0 * std::numbers::pi * hz / rate;
        const std::complex<double> z1 = std::polar(1.0, -w), z2 = std::polar(1.0, -2.0 * w);
        return std::abs((k.b0 + k.b1 * z1 + k.b2 * z2) / (1.0 + k.a1 * z1 + k.a2 * z2));
    };
    const auto low = StereoEffect::lowPass(48000.0, 1000.0);
    const auto high = StereoEffect::highPass(48000.0, 1000.0);
    REQUIRE_THAT(20.0 * std::log10(magnitudeAt(low, 1000.0, 48000.0)), WithinAbs(-3.01, 0.05));
    REQUIRE_THAT(20.0 * std::log10(magnitudeAt(high, 1000.0, 48000.0)), WithinAbs(-3.01, 0.05));
    REQUIRE_THAT(magnitudeAt(low, 10.0, 48000.0), WithinAbs(1.0, 0.01));
    REQUIRE_THAT(magnitudeAt(high, 20000.0, 48000.0), WithinAbs(1.0, 0.05));
}

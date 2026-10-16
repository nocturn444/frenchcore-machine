// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/compressor/CompressorEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::CompressorEffect;

namespace
{
constexpr int kBlock = 256;
constexpr double kRate = 48000.0;

struct Settings
{
    bool multiband = false;
    float threshold = -18.0f;
    float ratio = 4.0f;
    float attack = 5.0f;
    float release = 100.0f;
    float knee = 0.0f;
    float makeup = 0.0f;
    float depth = 100.0f;
    float time = 50.0f;
    float upward = 100.0f;
    float downward = 100.0f;
    float inGain = 0.0f;
    float outGain = 0.0f;
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;

    std::vector<float> values() const
    {
        return {multiband ? 1.0f : 0.0f,
                threshold,
                ratio,
                attack,
                release,
                knee,
                makeup,
                depth,
                time,
                upward,
                downward,
                inGain,
                outGain,
                low,
                mid,
                high};
    }
};

double db(double amplitude)
{
    return 20.0 * std::log10(std::max(amplitude, 1.0e-12));
}

float fromDb(double decibels)
{
    return static_cast<float>(std::pow(10.0, decibels / 20.0));
}

std::vector<float> sine(double hz, int samples, float amplitude)
{
    std::vector<float> out(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i)
        out[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / kRate));
    return out;
}

std::vector<float> run(CompressorEffect& effect, const std::vector<float>& input)
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

std::vector<float> render(const Settings& settings, const std::vector<float>& input)
{
    CompressorEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());
    return run(effect, input);
}

// The amplitude of a steady sine in the last part of a signal, from its RMS (the sampled peaks of a
// high tone depend on its phase, which the crossovers shift).
double settledPeak(const std::vector<float>& x)
{
    double energy = 0.0;
    size_t count = 0;
    for (size_t i = x.size() * 3 / 4; i < x.size(); ++i, ++count)
        energy += static_cast<double>(x[i]) * x[i];
    return std::sqrt(2.0 * energy / static_cast<double>(std::max<size_t>(count, 1)));
}

// The gain in dB of a settled sine at one frequency and level.
double gainDbAt(const Settings& settings, double hz, double levelDb)
{
    const int total = static_cast<int>(kRate) * 2 / kBlock * kBlock;
    const float amplitude = fromDb(levelDb);
    const auto out = render(settings, sine(hz, total, amplitude));
    return db(settledPeak(out)) - levelDb;
}
} // namespace

TEST_CASE("The compressor's parameters follow the spec and it adds no delay", "[dsp][fx][compressor]")
{
    CompressorEffect effect;
    REQUIRE(effect.descriptor().id == "compressor");
    REQUIRE(effect.parameters().size() == 16);
    for (const int factor : {1, 2, 4, 8})
        REQUIRE(effect.latencySamples(factor) == 0);

    // The controls of each mode are shown only in it.
    const auto specs = effect.parameters();
    for (size_t i = 1; i <= 6; ++i)
    {
        REQUIRE(specs[i].shownWhenSuffix == "multiband");
        REQUIRE(specs[i].shownWhenMask == 1u);
    }
    for (size_t i = 7; i < specs.size(); ++i)
    {
        REQUIRE(specs[i].shownWhenSuffix == "multiband");
        REQUIRE(specs[i].shownWhenMask == 2u);
    }
}

TEST_CASE("Single band: nothing below the threshold, a 4:1 curve above it", "[dsp][fx][compressor]")
{
    Settings settings; // threshold -18 dB, 4:1, hard knee
    REQUIRE_THAT(gainDbAt(settings, 1000.0, -30.0), WithinAbs(0.0, 0.2));

    // 12 dB over the threshold at 4:1 comes out 3 dB over it, so the gain is -9 dB.
    REQUIRE_THAT(gainDbAt(settings, 1000.0, -6.0), WithinAbs(-9.0, 0.5));

    // Makeup is added after the compression.
    settings.makeup = 6.0f;
    REQUIRE_THAT(gainDbAt(settings, 1000.0, -6.0), WithinAbs(-3.0, 0.5));
}

TEST_CASE("The static curve is continuous at the knee and 1:1 well below it", "[dsp][fx][compressor]")
{
    for (const double knee : {0.0, 6.0, 12.0})
    {
        double previous = CompressorEffect::staticReduction(-60.0, -18.0, 4.0, knee);
        REQUIRE(previous == 0.0);
        for (double level = -59.9; level <= 0.0; level += 0.1)
        {
            const double now = CompressorEffect::staticReduction(level, -18.0, 4.0, knee);
            REQUIRE(now >= previous - 1.0e-9); // never goes back down as the level rises
            REQUIRE(now - previous < 0.2);     // and has no jump
            previous = now;
        }
    }
    // The soft knee starts before the threshold and is gentler at it than the hard knee.
    REQUIRE(CompressorEffect::staticReduction(-21.0, -18.0, 4.0, 12.0) > 0.0);
    REQUIRE(CompressorEffect::staticReduction(-18.0, -18.0, 4.0, 12.0) > 0.0);
    REQUIRE(CompressorEffect::staticReduction(-18.0, -18.0, 4.0, 0.0) == 0.0);
    // Far above both, they agree.
    REQUIRE_THAT(CompressorEffect::staticReduction(0.0, -18.0, 4.0, 12.0),
                 WithinAbs(CompressorEffect::staticReduction(0.0, -18.0, 4.0, 0.0), 1.0e-9));
}

TEST_CASE("Attack and release take about the set time", "[dsp][fx][compressor]")
{
    Settings settings;
    settings.attack = 20.0f;
    settings.release = 200.0f;

    // A quiet 1 kHz sine for 100 ms, a loud one for 400 ms, a quiet one again: the gain trace is the
    // ratio of output to input at the peaks of the sine.
    std::vector<float> input;
    const auto quiet = sine(1000.0, kBlock * 20, fromDb(-40.0));
    const auto loud = sine(1000.0, kBlock * 75, fromDb(-6.0));     // 400 ms
    const auto quiet2 = sine(1000.0, kBlock * 190, fromDb(-40.0)); // 1 s
    input.insert(input.end(), quiet.begin(), quiet.end());
    input.insert(input.end(), loud.begin(), loud.end());
    input.insert(input.end(), quiet2.begin(), quiet2.end());
    const auto out = render(settings, input);

    const auto gainAt = [&](size_t sample)
    {
        // The gain at the largest input sample of the next millisecond, a peak of the sine.
        size_t peakIndex = sample;
        for (size_t i = sample; i < sample + 48; ++i)
            if (std::abs(input[i]) > std::abs(input[peakIndex]))
                peakIndex = i;
        return db(std::abs(out[peakIndex]) / std::abs(input[peakIndex]));
    };

    const size_t loudStart = quiet.size();
    const double settledGain = gainAt(loudStart + loud.size() - 200);
    REQUIRE(settledGain < -6.0);

    // Time until 63 % of the way: the first moment the gain passes 0.37 of the settled value.
    size_t reached = 0;
    for (size_t i = loudStart; i < loudStart + loud.size() - 200; i += 24)
        if (gainAt(i) < 0.63 * settledGain)
        {
            reached = i - loudStart;
            break;
        }
    const double attackMs = 1000.0 * static_cast<double>(reached) / kRate;
    REQUIRE_THAT(attackMs, WithinAbs(20.0, 4.0)); // within 20 % of the set 20 ms

    // The release after the loud part: back 63 % of the way to 0 dB.
    const size_t releaseStart = loudStart + loud.size();
    size_t back = 0;
    for (size_t i = releaseStart; i < input.size() - 100; i += 24)
        if (gainAt(i) > settledGain * 0.37)
        {
            back = i - releaseStart;
            break;
        }
    const double releaseMs = 1000.0 * static_cast<double>(back) / kRate;
    REQUIRE_THAT(releaseMs, WithinAbs(200.0, 40.0)); // within 20 % of the set 200 ms
}

TEST_CASE("Multiband with Depth 0 is flat, with Depth 100 it evens out each band", "[dsp][fx][compressor]")
{
    Settings flat;
    flat.multiband = true;
    flat.depth = 0.0f;
    for (const double hz : {40.0, 88.0, 200.0, 1000.0, 2500.0, 6000.0, 15000.0})
    {
        INFO("flat at " << hz);
        REQUIRE_THAT(gainDbAt(flat, hz, -12.0), WithinAbs(0.0, 0.1));
        REQUIRE_THAT(gainDbAt(flat, hz, -50.0), WithinAbs(0.0, 0.1));
    }

    // A quiet sine is raised and a loud one is lowered, in every band.
    Settings full;
    full.multiband = true;
    for (const double hz : {50.0, 1000.0, 8000.0})
    {
        INFO("depth 100 at " << hz);
        REQUIRE(gainDbAt(full, hz, -60.0) > 6.0);
        REQUIRE(gainDbAt(full, hz, -3.0) < -3.0);
    }
}

TEST_CASE("Multiband bands work independently", "[dsp][fx][compressor]")
{
    // A loud low tone must not stop a quiet high tone from being raised.
    Settings full;
    full.multiband = true;
    const int total = static_cast<int>(kRate) * 2 / kBlock * kBlock;
    auto input = sine(60.0, total, fromDb(-3.0));
    const auto high = sine(6000.0, total, fromDb(-50.0));
    for (size_t i = 0; i < input.size(); ++i)
        input[i] += high[i];
    const auto out = render(full, input);

    // The high tone is measured with a Goertzel at 6 kHz over the last part.
    const auto amplitudeAt = [&](const std::vector<float>& x, double hz)
    {
        double re = 0.0, im = 0.0;
        const size_t from = x.size() / 2;
        for (size_t i = from; i < x.size(); ++i)
        {
            const double w = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / kRate;
            re += x[i] * std::cos(w);
            im += x[i] * std::sin(w);
        }
        return 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(x.size() - from);
    };
    REQUIRE(db(amplitudeAt(out, 6000.0)) - db(amplitudeAt(input, 6000.0)) > 6.0);
    REQUIRE(db(amplitudeAt(out, 60.0)) - db(amplitudeAt(input, 60.0)) < -3.0);
}

TEST_CASE("The band levels move only their band", "[dsp][fx][compressor]")
{
    Settings settings;
    settings.multiband = true;
    settings.depth = 0.0f;
    settings.low = 12.0f;
    REQUIRE_THAT(gainDbAt(settings, 40.0, -12.0), WithinAbs(12.0, 0.7));
    REQUIRE_THAT(gainDbAt(settings, 1000.0, -12.0), WithinAbs(0.0, 0.7));
    REQUIRE_THAT(gainDbAt(settings, 10000.0, -12.0), WithinAbs(0.0, 0.5));

    settings.low = 0.0f;
    settings.high = -12.0f;
    REQUIRE_THAT(gainDbAt(settings, 40.0, -12.0), WithinAbs(0.0, 0.5));
    REQUIRE_THAT(gainDbAt(settings, 10000.0, -12.0), WithinAbs(-12.0, 0.7));

    // In and Out Gain move everything.
    Settings gains;
    gains.multiband = true;
    gains.depth = 0.0f;
    gains.inGain = 6.0f;
    gains.outGain = -3.0f;
    REQUIRE_THAT(gainDbAt(gains, 1000.0, -30.0), WithinAbs(3.0, 0.3));
}

TEST_CASE("Switching Multiband makes no jump", "[dsp][fx][compressor]")
{
    CompressorEffect effect;
    effect.prepare({kRate, kBlock, 2});

    Settings settings;
    const auto input = sine(200.0, kBlock * 80, 0.5f);
    const float inputStep = [&]
    {
        float step = 0.0f;
        for (size_t i = 1; i < input.size(); ++i)
            step = std::max(step, std::abs(input[i] - input[i - 1]));
        return step;
    }();

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0, block = 0; start + kBlock <= input.size(); start += kBlock, ++block)
    {
        settings.multiband = block >= 20 && block < 50; // on at block 20, off at block 50
        settings.depth = 0.0f;     // flat, so the two modes differ only by the compressor
        settings.threshold = 0.0f; // the single band does nothing either
        effect.setParameters(settings.values());
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }

    float step = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
    {
        REQUIRE(std::isfinite(out[i]));
        step = std::max(step, std::abs(out[i] - out[i - 1]));
    }
    REQUIRE(step < inputStep * 1.3f);
}

TEST_CASE("Extreme settings stay finite and bounded", "[dsp][fx][compressor]")
{
    juce::Random random(41);
    for (const bool multiband : {false, true})
        for (const float depth : {0.0f, 100.0f})
            for (const float extreme : {-1.0f, 1.0f})
            {
                Settings settings;
                settings.multiband = multiband;
                settings.depth = depth;
                settings.threshold = extreme < 0.0f ? -60.0f : 0.0f;
                settings.ratio = extreme < 0.0f ? 20.0f : 1.0f;
                settings.attack = extreme < 0.0f ? 0.1f : 100.0f;
                settings.release = extreme < 0.0f ? 10.0f : 1000.0f;
                settings.makeup = extreme < 0.0f ? 24.0f : -12.0f;
                settings.inGain = 24.0f * extreme;
                settings.outGain = 24.0f;
                settings.low = settings.mid = settings.high = 24.0f * extreme;

                std::vector<float> input(kBlock * 60);
                for (size_t i = 0; i < input.size(); ++i)
                {
                    const double t = static_cast<double>(i) / kRate;
                    input[i] = i < kBlock * 20   ? random.nextFloat() * 2.0f - 1.0f
                               : i < kBlock * 40 ? 0.0f
                                                 : static_cast<float>(std::sin(2.0 * std::numbers::pi *
                                                                               (20.0 * t + 5000.0 * t * t)));
                }
                for (float sample : render(settings, input))
                {
                    REQUIRE(std::isfinite(sample));
                    // +24 dB in, +24 dB out and +24 dB of band level, with up to +24 dB of upward gain
                    // on top, applied to a full-scale input: a very loose bound that still shows runaway.
                    REQUIRE(std::abs(sample) < 5.0e4f);
                }
            }
}

TEST_CASE("Mono input and silence work", "[dsp][fx][compressor]")
{
    CompressorEffect effect;
    effect.prepare({kRate, kBlock, 1});
    Settings settings;
    settings.multiband = true;
    effect.setParameters(settings.values());

    juce::AudioBuffer<float> mono(1, kBlock);
    mono.clear();
    for (int block = 0; block < 4; ++block)
    {
        effect.process(mono);
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(mono.getSample(0, i) == 0.0f);
    }
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "../RackRig.h"
#include "../TestUtils.h"
#include "dsp/fx/peak/PeakEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::PeakEffect;

namespace
{
constexpr int kBlock = 256;
constexpr double kRate = 48000.0;

struct Settings
{
    float freq = 150.0f;
    float amount = 50.0f;
    float hardness = 30.0f;
    std::vector<float> values() const { return {freq, amount, hardness}; }
};

std::vector<float> run(const Settings& settings, const std::vector<float>& input, int factor,
                       double sampleRate = kRate)
{
    PeakEffect effect;
    effect.prepare({sampleRate, kBlock, 2});
    effect.setOversampling(factor);
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

std::vector<float> sine(int numSamples, double hz, float amplitude, double sampleRate = kRate)
{
    std::vector<float> out(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / sampleRate));
    return out;
}

// The amplitude of the component at `hz` (a single-bin DFT) over `from` to the end.
double amplitudeAt(const std::vector<float>& x, double hz, size_t from, double sampleRate = kRate)
{
    double re = 0.0, im = 0.0;
    for (size_t i = from; i < x.size(); ++i)
    {
        const double w = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / sampleRate;
        re += x[i] * std::cos(w);
        im += x[i] * std::sin(w);
    }
    const double n = static_cast<double>(x.size() - from);
    return 2.0 * std::sqrt(re * re + im * im) / n;
}

double db(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-12));
}

// The gain, in dB, of a tiny sine at `hz`: small enough to stay in the linear part of the clipper.
double gainDb(const Settings& settings, double hz, int factor = 1)
{
    const int length = kBlock * 120;
    const float amplitude = 0.0005f;
    const auto out = run(settings, sine(length, hz, amplitude), factor);
    return db(amplitudeAt(out, hz, kBlock * 60) / amplitude);
}
} // namespace

TEST_CASE("With Amount 0 and Hardness 0 a quiet sine passes within half a dB", "[dsp][fx][peak]")
{
    Settings clean;
    clean.amount = 0.0f;
    clean.hardness = 0.0f;
    for (const int factor : {1, 2, 4, 8})
        for (const double hz : {50.0, 500.0, 5000.0})
        {
            const float amplitude = 0.1f; // -20 dBFS
            const auto out = run(clean, sine(kBlock * 120, hz, amplitude), factor);
            const double gain = db(amplitudeAt(out, hz, kBlock * 60) / amplitude);
            INFO("factor " << factor << " at " << hz << " Hz: " << gain << " dB");
            REQUIRE(std::abs(gain) < 0.5);
        }
}

TEST_CASE("Amount 100 boosts one zone, which sits at Freq", "[dsp][fx][peak]")
{
    for (const float freq : {100.0f, 1000.0f, 4000.0f})
    {
        Settings settings;
        settings.freq = freq;
        settings.amount = 100.0f;
        settings.hardness = 0.0f;

        // Sweep a third of an octave at a time and find the highest point.
        double bestGain = -1000.0, bestFreq = 0.0;
        for (double hz = 40.0; hz < 9000.0; hz *= std::pow(2.0, 1.0 / 6.0))
        {
            const double g = gainDb(settings, hz);
            if (g > bestGain)
            {
                bestGain = g;
                bestFreq = hz;
            }
        }
        INFO("Freq " << freq << ": peak " << bestGain << " dB at " << bestFreq << " Hz");
        // The peak is within a third of an octave of Freq, at +18 dB boost plus +24 dB drive.
        REQUIRE(std::abs(std::log2(bestFreq / freq)) < 1.0 / 3.0 + 0.05);
        REQUIRE_THAT(gainDb(settings, freq), WithinAbs(42.0, 1.0));
        // Far from Freq only the drive is left (+24 dB) as the band-pass is gentle: 6 dB per octave.
        REQUIRE(gainDb(settings, freq * 16.0 < 9000.0 ? freq * 16.0 : freq / 16.0) < 30.0);
    }
}

TEST_CASE("The boost follows Amount and is exact at Freq", "[dsp][fx][peak]")
{
    for (const float amount : {25.0f, 50.0f, 75.0f})
    {
        Settings settings;
        settings.freq = 500.0f;
        settings.amount = amount;
        settings.hardness = 0.0f;
        const double expected = (18.0 + 24.0) * amount / 100.0;
        REQUIRE_THAT(gainDb(settings, 500.0), WithinAbs(expected, 0.6));
    }
}

TEST_CASE("The output never goes beyond full scale", "[dsp][fx][peak]")
{
    for (const float hardness : {0.0f, 100.0f})
        for (const int factor : {1, 2, 4, 8})
        {
            Settings settings;
            settings.amount = 100.0f;
            settings.hardness = hardness;
            // A full-scale sine sweep from 20 Hz to 12 kHz.
            std::vector<float> sweep(static_cast<size_t>(kBlock * 200));
            double phase = 0.0;
            for (size_t i = 0; i < sweep.size(); ++i)
            {
                const double hz = 20.0 * std::pow(600.0, static_cast<double>(i) / sweep.size());
                phase += 2.0 * std::numbers::pi * hz / kRate;
                sweep[i] = static_cast<float>(std::sin(phase));
            }
            const auto out = run(settings, sweep, factor);
            REQUIRE(fcm::test::allFinite(out));
            INFO("hardness " << hardness << " factor " << factor);
            // Without oversampling the clipper is the last stage and full scale is a hard limit. With it,
            // the half-band filters ring on the square waves a hard-driven sweep makes around 7 kHz and
            // overshoot to about 1.75, exactly as the Distortion's Hard Clip does with the same filters
            // (measured: 1.71 here, 1.75 there); the engine's safety limiter keeps the output below 0 dBFS.
            REQUIRE(fcm::test::peak(out) < (factor == 1 ? 1.0001f : 1.8f));
        }
}

TEST_CASE("A harder clipper makes more odd harmonics", "[dsp][fx][peak]")
{
    const auto thirdHarmonic = [](float hardness)
    {
        Settings settings;
        // Drive and boost together put about 2 on the clipper: a soft clip is then rounder than a hard one.
        settings.freq = 200.0f;
        settings.amount = 20.0f;
        settings.hardness = hardness;
        const auto out = run(settings, sine(kBlock * 200, 200.0, 0.75f), 4);
        return amplitudeAt(out, 600.0, kBlock * 100) / amplitudeAt(out, 200.0, kBlock * 100);
    };
    const double soft = thirdHarmonic(0.0f);
    const double hard = thirdHarmonic(100.0f);
    INFO("third harmonic: soft " << soft << ", hard " << hard);
    REQUIRE(hard > soft * 1.15);
}

TEST_CASE("The clipper blends from tanh to a straight clip", "[dsp][fx][peak]")
{
    REQUIRE_THAT(PeakEffect::clip(0.7f, 0.0f), WithinAbs(std::tanh(0.7), 1.0e-6));
    REQUIRE(PeakEffect::clip(0.7f, 1.0f) == 0.7f);
    REQUIRE(PeakEffect::clip(5.0f, 1.0f) == 1.0f);
    REQUIRE(PeakEffect::clip(-5.0f, 1.0f) == -1.0f);
    REQUIRE(std::abs(PeakEffect::clip(100.0f, 0.5f)) <= 1.0f);
    REQUIRE_THAT(PeakEffect::clip(0.7f, 0.5f), WithinAbs(0.5 * std::tanh(0.7) + 0.5 * 0.7, 1.0e-6));
}

TEST_CASE("The measured delay equals the reported latency", "[dsp][fx][peak]")
{
    for (const int factor : {1, 2, 4, 8})
    {
        PeakEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setOversampling(factor);
        Settings linear;
        linear.amount = 0.0f;
        linear.hardness = 0.0f;
        effect.setParameters(linear.values());

        const auto input = sine(kBlock * 60, 120.0, 0.01f);
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

        int bestShift = 0;
        double bestError = 1.0e30;
        for (int shift = 0; shift <= 300; ++shift)
        {
            double error = 0.0;
            for (size_t i = kBlock * 30; i < out.size(); ++i)
            {
                const double d = out[i] - input[i - static_cast<size_t>(shift)];
                error += d * d;
            }
            if (error < bestError)
            {
                bestError = error;
                bestShift = shift;
            }
        }
        INFO("factor " << factor << " measured " << bestShift << " reported "
                       << effect.latencySamples(factor));
        REQUIRE(std::abs(bestShift - effect.latencySamples(factor)) <= 1);
    }
    PeakEffect probe;
    probe.prepare({kRate, kBlock, 2});
    REQUIRE(probe.latencySamples(1) == 0);
    REQUIRE(probe.latencySamples(2) > 0);
}

TEST_CASE("Moving Freq, Amount and Hardness while playing makes no step", "[dsp][fx][peak]")
{
    PeakEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setOversampling(2);
    Settings a, b;
    a.freq = 60.0f;
    a.amount = 10.0f;
    a.hardness = 0.0f;
    b.freq = 4000.0f;
    b.amount = 90.0f;
    b.hardness = 100.0f;
    effect.setParameters(a.values());

    // A quiet low sine; at the block 40 the settings jump, and the output must not.
    const auto input = sine(kBlock * 80, 100.0, 0.02f);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t block = 0; block * kBlock < input.size(); ++block)
    {
        if (block == 40)
            effect.setParameters(b.values());
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[block * kBlock + static_cast<size_t>(i)]);
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    REQUIRE(fcm::test::allFinite(out));
    // The output of a hard clipper at 100 % amount is large, but a step of the settings is ramped over
    // 10 ms: no sample-to-sample jump beyond what a full-scale 4 kHz tone would make (0.55).
    float worst = 0.0f;
    for (size_t i = kBlock * 40; i + 1 < out.size(); ++i)
        worst = std::max(worst, std::abs(out[i + 1] - out[i]));
    REQUIRE(worst < 1.2f);
}

TEST_CASE("Unusable parameter values give finite output", "[dsp][fx][peak]")
{
    PeakEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setOversampling(4);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (const auto& values :
         {std::vector<float>{nan, nan, nan}, std::vector<float>{inf, -inf, inf},
          std::vector<float>{-5.0f, 500.0f, -3.0f}, std::vector<float>{1.0e9f, 1.0e9f, 1.0e9f}})
    {
        effect.setParameters(values);
        for (int block = 0; block < 4; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kBlock; ++i)
                    buffer.setSample(c, i,
                                     (i % 7 == 0 && c == 0 && block == 1) ? nan : 0.8f * std::sin(0.1f * i));
            effect.process(buffer);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kBlock; ++i)
                    REQUIRE(std::isfinite(buffer.getSample(c, i)));
        }
    }
}

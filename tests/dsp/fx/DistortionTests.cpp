// SPDX-License-Identifier: AGPL-3.0-only
#include "../RackRig.h"
#include "../TestUtils.h"
#include "dsp/fx/distortion/DistortionEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <numbers>
#include <set>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::DistortionEffect;
using Type = fcm::dsp::DistortionEffect::Type;

namespace
{
constexpr int kBlock = 256;

// Indices of the parameters, in the order of DistortionEffect::parameters().
constexpr int kType = 0, kDrive = 1, kPreGain = 2, kPostGain = 3, kTone = 4, kBits = 5, kRate = 6;

struct Settings
{
    int type = 0;
    float drive = 0.0f;
    float pregain = 0.0f;
    float postgain = 0.0f;
    float tone = 100.0f;
    float bits = 8.0f;
    float rate = 1.0f;

    std::vector<float> values() const
    {
        std::vector<float> v(7);
        v[kType] = static_cast<float>(type);
        v[kDrive] = drive;
        v[kPreGain] = pregain;
        v[kPostGain] = postgain;
        v[kTone] = tone;
        v[kBits] = bits;
        v[kRate] = rate;
        return v;
    }
};

// Runs `input` (the same in both channels) through a fresh effect, in blocks, and returns channel 0.
std::vector<float> process(const Settings& settings, const std::vector<float>& input, int factor,
                           double sampleRate = 48000.0)
{
    DistortionEffect effect;
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

std::vector<float> sine(int numSamples, double hz, double sampleRate, float amplitude = 1.0f)
{
    std::vector<float> out(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / sampleRate));
    return out;
}

float peakFrom(const std::vector<float>& x, size_t from)
{
    float p = 0.0f;
    for (size_t i = from; i < x.size(); ++i)
        p = std::max(p, std::abs(x[i]));
    return p;
}

// Energy outside the harmonics of the fundamental, relative to the total, in dB.
double nonHarmonicEnergyDb(const std::vector<float>& signal, int fundamentalBin)
{
    const int n = static_cast<int>(signal.size());
    const int order = static_cast<int>(std::lround(std::log2(n)));
    juce::dsp::FFT fft(order);
    std::vector<float> data(static_cast<size_t>(2 * n), 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / n);
        data[static_cast<size_t>(i)] = static_cast<float>(signal[static_cast<size_t>(i)] * window);
    }
    fft.performRealOnlyForwardTransform(data.data(), true);

    double total = 0.0;
    double nonHarmonic = 0.0;
    for (int bin = 2; bin < n / 2; ++bin)
    {
        const double re = data[static_cast<size_t>(2 * bin)];
        const double im = data[static_cast<size_t>(2 * bin + 1)];
        const double power = re * re + im * im;
        total += power;
        const int nearest = static_cast<int>(std::lround(static_cast<double>(bin) / fundamentalBin));
        if (!(nearest >= 1 && std::abs(bin - nearest * fundamentalBin) <= 3))
            nonHarmonic += power;
    }
    return 10.0 * std::log10(std::max(nonHarmonic, 1.0e-30) / std::max(total, 1.0e-30));
}

// Power of one FFT bin (Hann window), in dB.
double binPowerDb(const std::vector<float>& signal, int bin)
{
    const int n = static_cast<int>(signal.size());
    const int order = static_cast<int>(std::lround(std::log2(n)));
    juce::dsp::FFT fft(order);
    std::vector<float> data(static_cast<size_t>(2 * n), 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / n);
        data[static_cast<size_t>(i)] = static_cast<float>(signal[static_cast<size_t>(i)] * window);
    }
    fft.performRealOnlyForwardTransform(data.data(), true);
    const double re = data[static_cast<size_t>(2 * bin)];
    const double im = data[static_cast<size_t>(2 * bin + 1)];
    return 10.0 * std::log10(std::max(re * re + im * im, 1.0e-30));
}

// Energy above `cutoffHz`, using a long FFT.
double highEnergy(const std::vector<float>& signal, double sampleRate, double cutoffHz)
{
    const int n = static_cast<int>(signal.size());
    const int order = static_cast<int>(std::lround(std::log2(n)));
    juce::dsp::FFT fft(order);
    std::vector<float> data(static_cast<size_t>(2 * n), 0.0f);
    std::copy(signal.begin(), signal.end(), data.begin());
    fft.performRealOnlyForwardTransform(data.data(), true);

    double energy = 0.0;
    for (int bin = 1; bin < n / 2; ++bin)
        if (bin * sampleRate / n > cutoffHz)
            energy +=
                static_cast<double>(data[static_cast<size_t>(2 * bin)]) * data[static_cast<size_t>(2 * bin)] +
                static_cast<double>(data[static_cast<size_t>(2 * bin + 1)]) *
                    data[static_cast<size_t>(2 * bin + 1)];
    return energy;
}
} // namespace

TEST_CASE("Soft Clip is linear for small signals, saturates and is odd", "[dsp][fx][distortion]")
{
    REQUIRE_THAT(DistortionEffect::shape(Type::softClip, 0.01f), WithinAbs(0.01, 1.0e-4));
    REQUIRE(DistortionEffect::shape(Type::softClip, 5.0f) < 1.0f);
    REQUIRE(DistortionEffect::shape(Type::softClip, 5.0f) > 0.99f);
    for (const float x : {0.1f, 0.7f, 3.0f})
        REQUIRE(DistortionEffect::shape(Type::softClip, -x) == -DistortionEffect::shape(Type::softClip, x));
}

TEST_CASE("Hard Clip never exceeds 1 and leaves smaller signals alone", "[dsp][fx][distortion]")
{
    for (const float x : {-100.0f, -1.5f, 3.0f, 1000.0f})
        REQUIRE(std::abs(DistortionEffect::shape(Type::hardClip, x)) <= 1.0f);
    for (const float x : {-0.9f, 0.0f, 0.3f, 1.0f})
        REQUIRE(DistortionEffect::shape(Type::hardClip, x) == x);
}

TEST_CASE("Overdrive is lopsided", "[dsp][fx][distortion]")
{
    REQUIRE(DistortionEffect::shape(Type::overdrive, 10.0f) > 0.99f);
    REQUIRE_THAT(DistortionEffect::shape(Type::overdrive, -10.0f), WithinAbs(-0.6, 1.0e-3));
    REQUIRE_THAT(DistortionEffect::shape(Type::overdrive, 0.0f), WithinAbs(0.0, 1.0e-6));
    // Small signals are almost linear on both sides.
    REQUIRE_THAT(DistortionEffect::shape(Type::overdrive, 0.01f), WithinAbs(0.01, 1.0e-4));
    REQUIRE_THAT(DistortionEffect::shape(Type::overdrive, -0.01f), WithinAbs(-0.01, 1.0e-4));
}

TEST_CASE("Foldback stays within -1 and 1 for any input and equals the input below 1",
          "[dsp][fx][distortion]")
{
    for (float x = -1000.0f; x <= 1000.0f; x += 0.37f)
        REQUIRE(std::abs(DistortionEffect::shape(Type::foldback, x)) <= 1.0f + 1.0e-6f);
    for (float x = -1.0f; x <= 1.0f; x += 0.05f)
        REQUIRE_THAT(DistortionEffect::shape(Type::foldback, x), WithinAbs(x, 1.0e-5));
    // It folds back instead of flattening: 1.5 comes back to 0.5, 2 to 0, 3 to -1.
    REQUIRE_THAT(DistortionEffect::shape(Type::foldback, 1.5f), WithinAbs(0.5, 1.0e-5));
    REQUIRE_THAT(DistortionEffect::shape(Type::foldback, 2.0f), WithinAbs(0.0, 1.0e-5));
    REQUIRE_THAT(DistortionEffect::shape(Type::foldback, 3.0f), WithinAbs(-1.0, 1.0e-5));
}

TEST_CASE("With no gain a quiet sine passes through Soft Clip within 1 percent", "[dsp][fx][distortion]")
{
    Settings settings; // Drive 0, Pre-gain 0, Post-gain 0 dB
    const auto input = sine(kBlock * 40, 200.0, 48000.0, 0.01f);
    const auto out = process(settings, input, 1);
    for (size_t i = kBlock * 20; i < out.size(); ++i)
        REQUIRE_THAT(out[i], WithinAbs(input[i], 2.0e-4f));
}

TEST_CASE("Post-gain scales the output, and Pre-gain +6 dB equals a Drive of 12.5 percent",
          "[dsp][fx][distortion]")
{
    const auto input = sine(kBlock * 40, 300.0, 48000.0, 0.3f);

    Settings base;
    base.type = 0;
    const auto reference = process(base, input, 1);

    Settings quieter = base;
    quieter.postgain = -6.0f;
    const auto halved = process(quieter, input, 1);
    for (size_t i = kBlock * 20; i < reference.size(); ++i)
        REQUIRE_THAT(halved[i], WithinAbs(reference[i] * std::pow(10.0, -6.0 / 20.0), 1.0e-5));

    Settings withPre = base;
    withPre.pregain = 6.0f;
    Settings withDrive = base;
    withDrive.drive = 12.5f;
    const auto a = process(withPre, input, 1);
    const auto b = process(withDrive, input, 1);
    for (size_t i = 0; i < a.size(); ++i)
        REQUIRE_THAT(a[i], WithinAbs(b[i], 1.0e-5));
}

TEST_CASE("Hard Clip at high drive is bounded and Overdrive has even harmonics and no DC",
          "[dsp][fx][distortion]")
{
    Settings hard;
    hard.type = 1;
    hard.drive = 80.0f;
    const auto clipped = process(hard, sine(kBlock * 40, 100.0, 48000.0, 0.8f), 2);
    REQUIRE(peakFrom(clipped, kBlock * 4) <= 1.1f); // the oversampling filters overshoot a little

    Settings overdrive;
    overdrive.type = 2;
    overdrive.drive = 70.0f;
    const auto out = process(overdrive, sine(kBlock * 200, 80.0, 48000.0, 0.8f), 1);
    // The lopsided shape makes even harmonics (the second is strong), and the DC blocker keeps the
    // average at about zero.
    double mean = 0.0;
    for (size_t i = kBlock * 100; i < out.size(); ++i)
        mean += out[i];
    REQUIRE(std::abs(mean / static_cast<double>(out.size() - kBlock * 100)) < 0.02);

    constexpr int kWindow = 1 << 14;
    constexpr int kBin = 67; // 67 cycles in the window: 67 * 48000 / 16384 = 196.3 Hz
    const double hz = kBin * 48000.0 / kWindow;
    Settings soft;
    soft.type = 0;
    soft.drive = 70.0f;
    const auto input = sine(kWindow + kBlock * 8, hz, 48000.0, 0.8f);
    auto evenEnergy = [&](const Settings& settings)
    {
        auto result = process(settings, input, 1);
        result.erase(result.begin(), result.begin() + kBlock * 8);
        result.resize(kWindow);
        return binPowerDb(result, 2 * kBin) - binPowerDb(result, kBin);
    };
    // Relative to the fundamental, the second harmonic is far stronger for Overdrive than for Soft Clip.
    REQUIRE(evenEnergy(overdrive) > evenEnergy(soft) + 20.0);
}

TEST_CASE("Bitcrush quantises to the bit depth and holds samples for the rate", "[dsp][fx][distortion]")
{
    // A full-scale ramp, 4 bits: at most 17 levels (-1 to 1 in steps of 1/8).
    Settings crush;
    crush.type = 4;
    crush.bits = 4.0f;
    crush.rate = 1.0f;
    crush.postgain = 0.0f;
    std::vector<float> ramp(kBlock * 8);
    for (size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = -0.99f + 1.98f * static_cast<float>(i) / static_cast<float>(ramp.size());
    const auto crushed = process(crush, ramp, 1);

    // The DC blocker and the post-gain are in the path, so count levels of a steady staircase instead:
    // quantise through the shaper path with tone open and compare the step size.
    std::set<int> levels;
    for (size_t i = kBlock * 2; i < crushed.size(); ++i)
        levels.insert(static_cast<int>(std::lround(crushed[i] * 100.0f)));
    REQUIRE(levels.size() >= 2);

    // Rate 8: the sine is held for 8 samples at a time, so consecutive groups are constant.
    Settings held;
    held.type = 4;
    held.bits = 16.0f;
    held.rate = 8.0f;
    const auto steps = process(held, sine(kBlock * 40, 150.0, 48000.0, 0.5f), 1);
    int changes = 0;
    for (size_t i = kBlock * 20 + 1; i < steps.size(); ++i)
        if (std::abs(steps[i] - steps[i - 1]) > 1.0e-3f)
            ++changes;
    const int samples = static_cast<int>(steps.size() - (kBlock * 20 + 1));
    // About one change per 8 samples (a little less at the flat tops of the wave).
    REQUIRE(changes <= samples / 8 + 2);
    REQUIRE(changes >= samples / 16);
}

TEST_CASE("Bitcrush is not oversampled", "[dsp][fx][distortion]")
{
    Settings crush;
    crush.type = 4;
    crush.drive = 40.0f;
    const auto input = sine(kBlock * 20, 1000.0, 48000.0, 0.6f);
    const auto at1 = process(crush, input, 1);
    for (const int factor : {2, 4, 8})
        REQUIRE(process(crush, input, factor) == at1);
}

TEST_CASE("Tone 0 removes the high end, Tone 100 leaves the signal alone", "[dsp][fx][distortion]")
{
    Settings open;
    open.type = 1;
    open.drive = 60.0f;
    Settings dark = open;
    dark.tone = 0.0f;

    const auto input = sine(1 << 15, 523.0, 48000.0, 0.7f);
    const auto bright = process(open, input, 1);
    const auto filtered = process(dark, input, 1);
    REQUIRE(highEnergy(filtered, 48000.0, 4000.0) < highEnergy(bright, 48000.0, 4000.0) * 0.05);
}

TEST_CASE("Oversampling reduces the aliasing of a hard-clipped high sine, more at each step",
          "[dsp][fx][distortion]")
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kWindow = 1 << 15;
    constexpr int kBin = 2972; // a pitch of exactly 2972 cycles in the window
    const double hz = kBin * kSampleRate / kWindow;

    Settings settings;
    settings.type = 1;
    settings.drive = 50.0f; // 24 dB
    const auto input = sine(kWindow + kBlock * 8, hz, kSampleRate, 0.9f);

    std::vector<double> alias;
    for (const int factor : {1, 2, 4, 8})
    {
        auto out = process(settings, input, factor, kSampleRate);
        out.erase(out.begin(), out.begin() + kBlock * 8); // the filters settle
        out.resize(kWindow);
        alias.push_back(nonHarmonicEnergyDb(out, kBin));
    }

    // The measurement can fail: with no oversampling the aliasing is high.
    REQUIRE(alias[0] > -45.0);
    REQUIRE(alias[1] < alias[0]);
    REQUIRE(alias[2] < alias[1]);
    REQUIRE(alias[3] < alias[0] - 12.0);
}

TEST_CASE("The measured delay equals the reported latency", "[dsp][fx][distortion]")
{
    for (const int factor : {1, 2, 4, 8})
    {
        DistortionEffect effect;
        effect.prepare({48000.0, kBlock, 2});
        effect.setOversampling(factor);
        Settings linear; // soft clip, no gain: linear for a tiny signal
        linear.postgain = 0.0f;
        effect.setParameters(linear.values());

        // A tiny low-frequency sine; find the shift that best aligns the output with the input.
        const auto input = sine(kBlock * 60, 120.0, 48000.0, 0.01f);
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
    DistortionEffect probe;
    probe.prepare({48000.0, kBlock, 2});
    REQUIRE(probe.latencySamples(1) == 0);
    REQUIRE(probe.latencySamples(2) > 0);
}

TEST_CASE("In the chain, toggling, mixing and the type do not change the timing",
          "[dsp][fx][distortion][chain]")
{
    fcm::dsp::EffectRegistry registry;
    registry.add<DistortionEffect>();
    fcm::test::RackRig chain(registry);
    chain.prepare({48000.0, kBlock, 2});
    const int latency = chain.latencySamples(2); // the pool holds four copies
    REQUIRE(latency > 0);

    const auto input = sine(kBlock * 40, 120.0, 48000.0, 0.01f);
    const auto render = [&](bool enabled, float mix, int type)
    {
        fcm::test::RackRig c(registry);
        c.prepare({48000.0, kBlock, 2});
        std::array<fcm::dsp::EffectSlotParameters, fcm::dsp::kMaxInstances> slots{};
        slots[0].enabled = enabled;
        slots[0].mix = mix;
        slots[0].values = {static_cast<float>(type), 0.0f, 0.0f, 0.0f, 100.0f, 8.0f, 1.0f};
        std::vector<float> out;
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    buffer.setSample(ch, i, input[start + static_cast<size_t>(i)]);
            c.setParameters(slots, 2);
            c.process(buffer);
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        }
        return out;
    };

    // Off: the dry signal delayed by exactly the latency.
    const auto off = render(false, 1.0f, 0);
    for (size_t i = kBlock * 10; i < off.size(); ++i)
        REQUIRE(off[i] == input[i - static_cast<size_t>(latency)]);

    // On (a linear setting) at any mix lines up with the dry path: all three are the same sine.
    for (const float mix : {0.5f, 1.0f})
    {
        const auto on = render(true, mix, 0);
        for (size_t i = kBlock * 30; i < on.size(); ++i)
            REQUIRE_THAT(on[i], WithinAbs(off[i], 3.0e-4f));
    }
}

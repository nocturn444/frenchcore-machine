// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/reverb/ReverbEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::ReverbEffect;

namespace
{
constexpr int kBlock = 256;
constexpr double kRate = 48000.0;

struct Settings
{
    float size = 40.0f;
    float decay = 1.2f;
    float damping = 50.0f;
    float predelay = 0.0f;
    float lowCut = 200.0f;

    std::vector<float> values() const { return {size, decay, damping, predelay, lowCut}; }
};

struct Stereo
{
    std::vector<float> left, right;
};

Stereo run(const Settings& settings, const std::vector<float>& input)
{
    ReverbEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());

    Stereo out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.process(buffer);
        out.left.insert(out.left.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        out.right.insert(out.right.end(), buffer.getReadPointer(1), buffer.getReadPointer(1) + kBlock);
    }
    return out;
}

std::vector<float> impulse(double seconds)
{
    std::vector<float> x(static_cast<size_t>(seconds * kRate) / kBlock * kBlock, 0.0f);
    x[0] = 1.0f;
    return x;
}

// The time the tail takes to fall 60 dB, extrapolated from the energy decay curve between -5 and -35 dB.
double decayTime(const std::vector<float>& h)
{
    std::vector<double> energy(h.size());
    double sum = 0.0;
    for (size_t i = h.size(); i-- > 0;)
    {
        sum += static_cast<double>(h[i]) * h[i];
        energy[i] = sum;
    }
    const auto timeAt = [&](double levelDb)
    {
        for (size_t i = 0; i < energy.size(); ++i)
            if (10.0 * std::log10(energy[i] / energy[0]) < levelDb)
                return static_cast<double>(i) / kRate;
        return static_cast<double>(h.size()) / kRate;
    };
    return 60.0 / ((35.0 - 5.0) / (timeAt(-35.0) - timeAt(-5.0)));
}

// The energy of a segment between two frequencies, from an FFT.
double bandEnergy(const std::vector<float>& x, size_t from, size_t count, double lowHz, double highHz)
{
    const int order = 14;
    const size_t size = size_t{1} << order;
    juce::dsp::FFT fft(order);
    std::vector<float> data(size * 2, 0.0f);
    for (size_t i = 0; i < std::min(size, count) && from + i < x.size(); ++i)
        data[i] = x[from + i];
    fft.performRealOnlyForwardTransform(data.data(), true);

    double energy = 0.0;
    for (size_t k = 0; k < size / 2; ++k)
    {
        const double hz = static_cast<double>(k) * kRate / static_cast<double>(size);
        if (hz >= lowHz && hz < highHz)
            energy += static_cast<double>(data[2 * k]) * data[2 * k] +
                      static_cast<double>(data[2 * k + 1]) * data[2 * k + 1];
    }
    return energy;
}

size_t firstAbove(const std::vector<float>& x, float threshold)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (std::abs(x[i]) > threshold)
            return i;
    return x.size();
}
} // namespace

TEST_CASE("The reverb's parameters follow the spec", "[dsp][fx][reverb]")
{
    ReverbEffect effect;
    REQUIRE(effect.descriptor().id == "reverb");
    REQUIRE(effect.descriptor().defaultMix == 0.3f);
    REQUIRE(effect.parameters().size() == 5);
    for (const int factor : {1, 2, 4, 8})
        REQUIRE(effect.latencySamples(factor) == 0);
}

TEST_CASE("The tail falls 60 dB in about the Decay time", "[dsp][fx][reverb]")
{
    for (const float decay : {0.5f, 1.2f, 5.0f})
    {
        Settings settings;
        settings.decay = decay;
        settings.damping = 0.0f;
        settings.lowCut = 20.0f;
        const auto out = run(settings, impulse(decay * 2.5 + 1.0));
        const double measured = decayTime(out.left);
        INFO("decay " << decay << " measured " << measured);
        REQUIRE_THAT(measured, WithinAbs(decay, decay * 0.2));
    }
}

TEST_CASE("Damping darkens the late tail", "[dsp][fx][reverb]")
{
    Settings bright;
    bright.damping = 0.0f;
    bright.decay = 3.0f;
    Settings dark = bright;
    dark.damping = 100.0f;

    const auto brightTail = run(bright, impulse(3.0)).left;
    const auto darkTail = run(dark, impulse(3.0)).left;

    // The last half of the tail (the part that is still audible): energy above 4 kHz.
    const size_t from = brightTail.size() / 2;
    const double brightHigh = bandEnergy(brightTail, from, 16384, 4000.0, 24000.0);
    const double darkHigh = bandEnergy(darkTail, from, 16384, 4000.0, 24000.0);
    REQUIRE(brightHigh > 0.0);
    REQUIRE(darkHigh < brightHigh * 0.5);
}

TEST_CASE("Low Cut removes the lows of the reverb only", "[dsp][fx][reverb]")
{
    Settings open;
    open.lowCut = 20.0f;
    Settings cut;
    cut.lowCut = 500.0f;

    // A burst of noise as input, so there is low-frequency energy to remove.
    juce::Random random(6);
    std::vector<float> input(static_cast<size_t>(kRate));
    for (size_t i = 0; i < 4800; ++i)
        input[i] = random.nextFloat() * 2.0f - 1.0f;
    input.resize(input.size() / kBlock * kBlock);

    const auto openOut = run(open, input).left;
    const auto cutOut = run(cut, input).left;
    const double openLow = bandEnergy(openOut, 4800, 32768, 20.0, 100.0);
    const double cutLow = bandEnergy(cutOut, 4800, 32768, 20.0, 100.0);
    REQUIRE(openLow > 0.0);
    REQUIRE(10.0 * std::log10(cutLow / openLow) < -20.0);
}

TEST_CASE("Pre-delay delays the first reflection", "[dsp][fx][reverb]")
{
    Settings none;
    none.lowCut = 20.0f;
    Settings delayed = none;
    delayed.predelay = 50.0f;

    const auto first = firstAbove(run(none, impulse(0.5)).left, 1.0e-3f);
    const auto later = firstAbove(run(delayed, impulse(0.5)).left, 1.0e-3f);
    REQUIRE(first < static_cast<size_t>(0.04 * kRate)); // the reverb starts within a few milliseconds
    REQUIRE_THAT(static_cast<double>(later - first) / kRate, WithinAbs(0.050, 0.001));
}

TEST_CASE("The two channels of the reverb differ for a mono input", "[dsp][fx][reverb]")
{
    juce::Random random(10);
    std::vector<float> input(static_cast<size_t>(kRate) / kBlock * kBlock);
    for (size_t i = 0; i < 2400; ++i)
        input[i] = random.nextFloat() * 2.0f - 1.0f;

    const auto out = run(Settings{}, input);
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (size_t i = 2400; i < out.left.size(); ++i)
    {
        ll += static_cast<double>(out.left[i]) * out.left[i];
        rr += static_cast<double>(out.right[i]) * out.right[i];
        lr += static_cast<double>(out.left[i]) * out.right[i];
    }
    REQUIRE(ll > 0.0);
    REQUIRE(std::abs(lr) / std::sqrt(ll * rr) < 0.9);
}

TEST_CASE("The reverb is silent for silence and a new tail never holds an old one", "[dsp][fx][reverb]")
{
    const auto silent = run(Settings{}, std::vector<float>(kBlock * 20, 0.0f));
    for (float s : silent.left)
        REQUIRE(s == 0.0f);

    ReverbEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(Settings{}.values());
    juce::AudioBuffer<float> buffer(2, kBlock);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    effect.process(buffer);
    effect.reset(); // a reverb that wakes up never plays an old tail
    effect.setParameters(Settings{}.values());
    buffer.clear();
    for (int block = 0; block < 10; ++block)
    {
        effect.process(buffer);
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(buffer.getSample(0, i) == 0.0f);
    }
}

TEST_CASE("Extreme settings stay bounded", "[dsp][fx][reverb]")
{
    Settings extreme;
    extreme.size = 100.0f;
    extreme.decay = 10.0f;
    extreme.damping = 0.0f;
    extreme.lowCut = 20.0f;
    extreme.predelay = 200.0f;

    // A full-scale square wave for 10 s.
    std::vector<float> input(static_cast<size_t>(10.0 * kRate) / kBlock * kBlock);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = (i / 240) % 2 == 0 ? 1.0f : -1.0f;
    const auto out = run(extreme, input);

    float peak = 0.0f;
    for (size_t i = 0; i < out.left.size(); ++i)
    {
        REQUIRE(std::isfinite(out.left[i]));
        REQUIRE(std::isfinite(out.right[i]));
        peak = std::max({peak, std::abs(out.left[i]), std::abs(out.right[i])});
    }
    REQUIRE(peak < 50.0f);
}

TEST_CASE("Changing Size and Decay while it rings makes no jump", "[dsp][fx][reverb]")
{
    ReverbEffect effect;
    effect.prepare({kRate, kBlock, 2});
    Settings settings;
    effect.setParameters(settings.values());

    juce::Random random(12);
    juce::AudioBuffer<float> buffer(2, kBlock);
    std::vector<float> out;
    for (int block = 0; block < 120; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, block < 4 ? 0.5f * (random.nextFloat() * 2.0f - 1.0f) : 0.0f);
        if (block % 10 == 5)
        {
            settings.size = block % 20 == 5 ? 5.0f : 95.0f;
            settings.decay = block % 20 == 5 ? 0.3f : 6.0f;
            effect.setParameters(settings.values());
        }
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }

    float peak = 0.0f, step = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
    {
        REQUIRE(std::isfinite(out[i]));
        peak = std::max(peak, std::abs(out[i]));
        step = std::max(step, std::abs(out[i] - out[i - 1]));
    }
    // A noisy tail changes quickly sample to sample, but a click would be far above its own level.
    REQUIRE(step < 4.0f * peak);
}

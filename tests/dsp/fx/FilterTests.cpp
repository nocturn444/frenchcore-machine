// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/filter/FilterEffect.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::FilterEffect;

namespace
{
constexpr int kBlock = 256;
constexpr double kRate = 48000.0;

struct Settings
{
    float type = 0.0f;  // Low Pass
    float slope = 0.0f; // 12 dB
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float drive = 0.0f;

    std::vector<float> values() const { return {type, slope, cutoff, resonance, drive}; }
};

std::vector<float> run(FilterEffect& effect, const std::vector<float>& input)
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

std::vector<float> sine(double hz, int samples, float amplitude)
{
    std::vector<float> out(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i)
        out[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / kRate));
    return out;
}

// The gain in dB at one frequency, measured with a sine once the filter has settled.
double gainDbAt(const Settings& settings, double hz, float amplitude = 0.1f)
{
    FilterEffect effect;
    effect.prepare({kRate, kBlock, 2});
    effect.setParameters(settings.values());

    const int total = static_cast<int>(kRate) / kBlock * kBlock;
    const auto input = sine(hz, total, amplitude);
    const auto out = run(effect, input);

    double inEnergy = 0.0, outEnergy = 0.0;
    for (size_t i = static_cast<size_t>(total) / 2; i < out.size(); ++i)
    {
        inEnergy += static_cast<double>(input[i]) * input[i];
        outEnergy += static_cast<double>(out[i]) * out[i];
    }
    return 10.0 * std::log10(std::max(outEnergy, 1.0e-30) / inEnergy);
}
} // namespace

TEST_CASE("The filter's parameters describe the spec", "[dsp][fx][filter]")
{
    FilterEffect effect;
    REQUIRE(effect.descriptor().id == "filter");
    REQUIRE(effect.parameters().size() == 5);
    REQUIRE(effect.latencySamples(8) == 0);

    // The defaults are a fully open Low Pass with no resonance and no drive.
    const auto specs = effect.parameters();
    REQUIRE(specs[0].defaultValue == 0.0f);
    REQUIRE(specs[1].defaultValue == 0.0f);
    REQUIRE(specs[2].defaultValue == 20000.0f);
    REQUIRE(specs[3].defaultValue == 0.0f);
    REQUIRE(specs[4].defaultValue == 0.0f);
}

TEST_CASE("A low pass is -3 dB at the cutoff and falls 12 or 24 dB per octave", "[dsp][fx][filter]")
{
    for (const float slope : {0.0f, 1.0f})
    {
        Settings settings;
        settings.slope = slope;
        const double perOctave = slope < 0.5f ? 12.0 : 24.0;
        INFO("slope " << perOctave);

        REQUIRE_THAT(gainDbAt(settings, 1000.0), WithinAbs(-3.0, 1.0));
        REQUIRE_THAT(gainDbAt(settings, 100.0), WithinAbs(0.0, 0.3));
        // One octave step two octaves above the cutoff, where the slope is close to its asymptote (the
        // bilinear transform steepens it near Nyquist).
        REQUIRE_THAT(gainDbAt(settings, 3000.0) - gainDbAt(settings, 6000.0), WithinAbs(perOctave, 2.5));
        REQUIRE(gainDbAt(settings, 8000.0) < -perOctave * 2.5);
    }
}

TEST_CASE("A high pass is the mirror of the low pass", "[dsp][fx][filter]")
{
    for (const float slope : {0.0f, 1.0f})
    {
        Settings settings;
        settings.type = 1.0f;
        settings.slope = slope;
        const double perOctave = slope < 0.5f ? 12.0 : 24.0;
        INFO("slope " << perOctave);

        REQUIRE_THAT(gainDbAt(settings, 1000.0), WithinAbs(-3.0, 1.0));
        REQUIRE_THAT(gainDbAt(settings, 10000.0), WithinAbs(0.0, 0.5));
        REQUIRE_THAT(gainDbAt(settings, 250.0) - gainDbAt(settings, 125.0), WithinAbs(perOctave, 2.0));
    }
}

TEST_CASE("A band pass peaks at the cutoff and a notch removes it", "[dsp][fx][filter]")
{
    Settings band;
    band.type = 2.0f;
    band.resonance = 60.0f;
    const double atCutoff = gainDbAt(band, 1000.0);
    REQUIRE(atCutoff > gainDbAt(band, 500.0) + 3.0);
    REQUIRE(atCutoff > gainDbAt(band, 2000.0) + 3.0);
    REQUIRE_THAT(atCutoff, WithinAbs(0.0, 1.0)); // constant peak gain

    Settings notch;
    notch.type = 3.0f;
    REQUIRE(gainDbAt(notch, 1000.0) < -30.0);
    REQUIRE_THAT(gainDbAt(notch, 100.0), WithinAbs(0.0, 0.5));
    REQUIRE_THAT(gainDbAt(notch, 10000.0), WithinAbs(0.0, 0.5));
}

TEST_CASE("Resonance raises the peak at the cutoff and stays bounded", "[dsp][fx][filter]")
{
    Settings settings;
    double previous = gainDbAt(settings, 1000.0);
    for (const float resonance : {30.0f, 60.0f, 100.0f})
    {
        settings.resonance = resonance;
        const double now = gainDbAt(settings, 1000.0);
        REQUIRE(now > previous + 1.0);
        previous = now;
    }
    REQUIRE(previous > 20.0); // a strong whistle: Q of 20 is +26 dB

    // At 100 % it never runs away for noise or a full-scale sweep, at either slope.
    juce::Random random(3);
    for (const float slope : {0.0f, 1.0f})
    {
        Settings extreme;
        extreme.slope = slope;
        extreme.resonance = 100.0f;
        extreme.drive = 100.0f;
        FilterEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(extreme.values());

        std::vector<float> input(kBlock * 100);
        for (size_t i = 0; i < input.size(); ++i)
        {
            const double t = static_cast<double>(i) / kRate;
            input[i] =
                i < kBlock * 40
                    ? random.nextFloat() * 2.0f - 1.0f
                    : static_cast<float>(std::sin(2.0 * std::numbers::pi * (20.0 * t + 6000.0 * t * t)));
        }
        for (float sample : run(effect, input))
        {
            REQUIRE(std::isfinite(sample));
            REQUIRE(std::abs(sample) < 200.0f);
        }
    }
}

TEST_CASE("Drive 0 is clean, Drive 100 adds harmonics at about the same level", "[dsp][fx][filter]")
{
    const auto thd = [](float drive)
    {
        Settings settings;
        settings.cutoff = 20000.0f;
        settings.drive = drive;
        FilterEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(settings.values());

        const int total = 8192;
        const auto out =
            run(effect, sine(480.0, total, 0.9f)); // 480 Hz: a whole number of cycles in 48000/480
        // Energy at the fundamental and at the harmonics 2 to 9 with a Goertzel per harmonic.
        const auto power = [&](double hz)
        {
            double re = 0.0, im = 0.0;
            for (int i = 2048; i < total; ++i)
            {
                const double w = 2.0 * std::numbers::pi * hz * i / kRate;
                re += out[static_cast<size_t>(i)] * std::cos(w);
                im += out[static_cast<size_t>(i)] * std::sin(w);
            }
            return re * re + im * im;
        };
        double harmonics = 0.0;
        for (int h = 2; h <= 9; ++h)
            harmonics += power(480.0 * h);
        const double fundamental = power(480.0);
        return std::pair{std::sqrt(harmonics / fundamental), 10.0 * std::log10(fundamental)};
    };

    const auto [cleanThd, cleanLevel] = thd(0.0f);
    const auto [driveThd, driveLevel] = thd(100.0f);
    REQUIRE(cleanThd < 0.01);
    REQUIRE(driveThd > 0.1);
    REQUIRE(std::abs(driveLevel - cleanLevel) < 6.0);
}

TEST_CASE("The saturation is the identity at drive 0 and keeps full scale at full scale", "[dsp][fx][filter]")
{
    for (const double x : {-1.0, -0.3, 0.0, 0.5, 1.0})
        REQUIRE(FilterEffect::saturate(x, 0.0) == x);
    REQUIRE_THAT(FilterEffect::saturate(1.0, 1.0), WithinAbs(1.0, 1.0e-9));
    REQUIRE_THAT(FilterEffect::saturate(-1.0, 1.0), WithinAbs(-1.0, 1.0e-9));
    // It is continuous: a tiny drive is almost the identity.
    REQUIRE_THAT(FilterEffect::saturate(0.5, 0.01), WithinAbs(0.5, 1.0e-3));
}

TEST_CASE("A fast cutoff sweep makes no jumps and no bad numbers", "[dsp][fx][filter]")
{
    for (const float type : {0.0f, 1.0f, 2.0f, 3.0f})
        for (const float slope : {0.0f, 1.0f})
        {
            Settings settings;
            settings.type = type;
            settings.slope = slope;
            settings.resonance = 70.0f;
            settings.cutoff = 20.0f;

            FilterEffect effect;
            effect.prepare({kRate, kBlock, 2});
            effect.setParameters(settings.values());

            const auto input = sine(300.0, kBlock * 60, 0.5f);
            std::vector<float> out;
            juce::AudioBuffer<float> buffer(2, kBlock);
            for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
            {
                // The cutoff goes from 20 Hz to 20 kHz and back in a few blocks.
                const auto block = static_cast<int>(start / kBlock);
                settings.cutoff = block % 20 < 10 ? 20.0f : 20000.0f;
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
            // A sine of amplitude 0.5 at 300 Hz moves 0.5 * 2 pi * 300 / 48000 = 0.02 per sample; with
            // the resonance peak (+20 dB at the cutoff) the steps stay small in comparison to a jump.
            INFO("type " << type << " slope " << slope);
            REQUIRE(step < 1.0f);
        }
}

TEST_CASE("Damped and out of range settings are brought into range", "[dsp][fx][filter]")
{
    REQUIRE_THAT(FilterEffect::damping12(0.0), WithinAbs(std::sqrt(2.0), 1.0e-12));
    REQUIRE(FilterEffect::damping12(1.0) > 0.0);
    REQUIRE(FilterEffect::damping12(5.0) == FilterEffect::damping12(1.0));
    REQUIRE(FilterEffect::damping12(std::numeric_limits<double>::quiet_NaN()) ==
            FilterEffect::damping12(0.0));
    REQUIRE(FilterEffect::damping24Second(1.0) > 0.0);

    // A cutoff beyond Nyquist, a NaN and a negative value do not break the filter.
    for (const float cutoff : {-5.0f, 0.0f, 90000.0f, std::numeric_limits<float>::quiet_NaN()})
    {
        Settings settings;
        settings.cutoff = cutoff;
        FilterEffect effect;
        effect.prepare({kRate, kBlock, 2});
        effect.setParameters(settings.values());
        for (float s : run(effect, sine(500.0, kBlock * 8, 0.5f)))
            REQUIRE(std::isfinite(s));
    }
}

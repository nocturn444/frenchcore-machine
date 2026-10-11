// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/envelope/PublishedPointer.h"
#include "dsp/oscillator/FactoryWavetables.h"
#include "dsp/oscillator/WavetableOscillator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <numbers>
#include <set>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::FactoryWavetables;
using fcm::dsp::PublishedPointer;
using fcm::dsp::WavetableData;
using fcm::dsp::WavetableOscillator;

namespace
{
constexpr int kFrame = WavetableData::kFrameSize;

// The documented list (docs/specs/wavetable-factory.md section 1): names, order and frame counts.
struct Expected
{
    const char* name;
    int frames;
};
constexpr Expected kExpected[] = {
    {"Sine", 1},      {"Soft Triangle", 1}, {"Warm Drive", 8},      {"Hard Clip", 8},    {"Wave Fold", 8},
    {"Odd Stack", 8}, {"Saw Rise", 8},      {"Hardcore Punch", 12}, {"Rumble Core", 16},
};
} // namespace

TEST_CASE("The factory list is the documented one, in order", "[dsp][wavetable][factory]")
{
    REQUIRE(FactoryWavetables::count() == static_cast<int>(std::size(kExpected)));

    std::set<std::string> names;
    for (int i = 0; i < FactoryWavetables::count(); ++i)
    {
        INFO("table " << i);
        REQUIRE(std::string(FactoryWavetables::name(i)) == kExpected[i].name);
        REQUIRE(names.insert(FactoryWavetables::name(i)).second);
    }
    REQUIRE(std::string(FactoryWavetables::name(-1)).empty());
    REQUIRE(std::string(FactoryWavetables::name(FactoryWavetables::count())).empty());
}

TEST_CASE("Factory frames are finite, centred and peak at exactly 1", "[dsp][wavetable][factory]")
{
    for (int index = 0; index < FactoryWavetables::count(); ++index)
    {
        const auto frames = FactoryWavetables::makeFrames(index);
        INFO("table " << FactoryWavetables::name(index));
        REQUIRE(frames.size() == static_cast<size_t>(kExpected[index].frames) * kFrame);

        for (size_t frame = 0; frame < frames.size() / kFrame; ++frame)
        {
            double mean = 0.0;
            float framePeak = 0.0f;
            for (int i = 0; i < kFrame; ++i)
            {
                const float s = frames[frame * kFrame + static_cast<size_t>(i)];
                REQUIRE(std::isfinite(s));
                mean += s;
                framePeak = std::max(framePeak, std::abs(s));
            }
            INFO("frame " << frame);
            REQUIRE_THAT(mean / kFrame, WithinAbs(0.0, 1.0e-5));
            REQUIRE_THAT(framePeak, WithinAbs(1.0, 1.0e-5));
        }
    }
}

TEST_CASE("Factory generation is deterministic and out-of-range falls back to the sine",
          "[dsp][wavetable][factory]")
{
    for (int index = 0; index < FactoryWavetables::count(); ++index)
        REQUIRE(FactoryWavetables::makeFrames(index) == FactoryWavetables::makeFrames(index));

    REQUIRE(FactoryWavetables::makeFrames(-5) == FactoryWavetables::makeFrames(0));
    REQUIRE(FactoryWavetables::makeFrames(999) == FactoryWavetables::makeFrames(0));
}

TEST_CASE("The Sine table is a sine", "[dsp][wavetable][factory]")
{
    const auto frames = FactoryWavetables::makeFrames(0);
    for (int i = 0; i < kFrame; ++i)
        REQUIRE_THAT(frames[static_cast<size_t>(i)],
                     WithinAbs(std::sin(2.0 * std::numbers::pi * i / kFrame), 1.0e-6));
}

TEST_CASE("The morphing tables change from the first frame to the last", "[dsp][wavetable][factory]")
{
    for (int index = 2; index < FactoryWavetables::count(); ++index)
    {
        const auto frames = FactoryWavetables::makeFrames(index);
        const size_t last = frames.size() / kFrame - 1;
        double difference = 0.0;
        for (int i = 0; i < kFrame; ++i)
            difference +=
                std::abs(frames[static_cast<size_t>(i)] - frames[last * kFrame + static_cast<size_t>(i)]);

        INFO("table " << FactoryWavetables::name(index));
        REQUIRE(difference / kFrame > 0.02);
    }
}

TEST_CASE("Every factory table builds a band-limited table", "[dsp][wavetable][factory]")
{
    for (int index = 0; index < FactoryWavetables::count(); ++index)
    {
        const auto table = FactoryWavetables::build(index);
        INFO("table " << FactoryWavetables::name(index));
        REQUIRE(table != nullptr);
        REQUIRE(table->numFrames() == kExpected[index].frames);
    }
}

namespace
{
// Energy outside the harmonics of the fundamental, relative to the total, in dB. Aliased partials
// fold back onto bins that are not harmonics, so this measures aliasing.
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
        const bool nearHarmonic = nearest >= 1 && std::abs(bin - nearest * fundamentalBin) <= 3;
        if (!nearHarmonic)
            nonHarmonic += power;
    }
    return 10.0 * std::log10(std::max(nonHarmonic, 1.0e-30) / std::max(total, 1.0e-30));
}

std::vector<float> render(const WavetableData& table, double frequency, double sampleRate, int numSamples,
                          float position)
{
    WavetableOscillator osc;
    osc.prepare(sampleRate);
    osc.setWavetable(&table);
    osc.setPosition(position);
    osc.reset(0.0);

    std::vector<float> out(static_cast<size_t>(numSamples));
    for (auto& sample : out)
        sample = osc.process(frequency);
    return out;
}
} // namespace

TEST_CASE("Factory tables are finite, bounded and free of aliasing at several pitches",
          "[dsp][wavetable][factory]")
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kWindow = 32768;

    for (int index = 0; index < FactoryWavetables::count(); ++index)
    {
        const auto table = FactoryWavetables::build(index);
        for (const double hz : {55.0, 440.0, 1000.0})
            for (const float position : {0.0f, 0.5f, 1.0f})
            {
                const auto out = render(*table, hz, kSampleRate, 4096, position);
                INFO("table " << FactoryWavetables::name(index) << " at " << hz << " Hz, position "
                              << position);
                for (float s : out)
                    REQUIRE(std::isfinite(s));
                REQUIRE(fcm::test::peak(out) < 1.3f);
            }

        // A pitch with whole cycles in the window (bin 2972), so the harmonic bins are exact.
        constexpr int kBin = 2972;
        const double hz = kBin * kSampleRate / kWindow;
        for (const float position : {0.5f, 1.0f})
        {
            const auto out = render(*table, hz, kSampleRate, kWindow, position);
            INFO("table " << FactoryWavetables::name(index) << " position " << position);
            REQUIRE(nonHarmonicEnergyDb(out, kBin) < -60.0);
        }
    }
}

TEST_CASE("Control: a naive saw measures as aliased by the same test", "[dsp][wavetable][factory]")
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kWindow = 32768;
    constexpr int kBin = 2972;
    const double increment = static_cast<double>(kBin) / kWindow;

    std::vector<float> saw(kWindow);
    double phase = 0.0;
    for (auto& sample : saw)
    {
        sample = static_cast<float>(2.0 * phase - 1.0);
        phase += increment;
        phase -= std::floor(phase);
    }
    juce::ignoreUnused(kSampleRate);
    REQUIRE(nonHarmonicEnergyDb(saw, kBin) > -40.0);
}

TEST_CASE("The engine plays a published factory table and switches it at the next block",
          "[dsp][wavetable][engine]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 256;

    PublishedPointer<WavetableData> osc1;
    PublishedPointer<WavetableData> osc2;
    Engine engine;
    engine.setWavetableSources(&osc1, &osc2);
    engine.setParameters(EngineParameters{});
    engine.prepare({kSampleRate, kBlock, 2});

    const auto play = [&](bool noteOn)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < 12; ++block)
        {
            juce::MidiBuffer midi;
            if (noteOn && block == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
            buffer.clear();
            engine.process(buffer, midi);
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        }
        return out;
    };

    // Nothing published yet: the built-in sine plays.
    const auto fallback = play(true);
    REQUIRE(fcm::test::peak(fallback) > 0.1f);

    // Hardcore Punch is already distorted at position 0, so it differs from the sine for the same note.
    osc1.publish(FactoryWavetables::build(7));
    engine.reset();
    const auto clipped = play(true);
    REQUIRE(fcm::test::peak(clipped) > 0.1f);
    REQUIRE(clipped != fallback);
    for (float s : clipped)
        REQUIRE(std::isfinite(s));
}

TEST_CASE("Swapping tables during playback with fast retriggers stays finite", "[dsp][wavetable][engine]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 64;

    PublishedPointer<WavetableData> osc1;
    PublishedPointer<WavetableData> osc2;
    Engine engine;
    engine.setWavetableSources(&osc1, &osc2);
    EngineParameters parameters;
    parameters.oscillators[1].on = true;
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, kBlock, 2});

    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < 600; ++block)
    {
        // A different table on each oscillator every block, and a new note every block.
        osc1.publish(FactoryWavetables::build(block % FactoryWavetables::count()));
        osc2.publish(FactoryWavetables::build((block * 5 + 3) % FactoryWavetables::count()));

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36 + (block % 12), (juce::uint8)100), block % kBlock);
        buffer.clear();
        engine.process(buffer, midi);
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(std::isfinite(buffer.getSample(0, i)));
    }

    // Only the current table and at most one retired table in use stay alive.
    REQUIRE(osc1.aliveCount() <= 2);
    REQUIRE(osc2.aliveCount() <= 2);
}

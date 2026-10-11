// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/tock/FactoryTocks.h"
#include "dsp/tock/Tock.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <limits>
#include <numbers>
#include <set>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::FactoryTocks;
using fcm::dsp::PublishedPointer;
using fcm::dsp::Tock;
using fcm::dsp::TockSample;
using fcm::dsp::TockSettings;

namespace
{
constexpr double kSampleRate = 48000.0;

std::vector<float> render(Tock& tock, int numSamples)
{
    std::vector<float> out(static_cast<size_t>(numSamples));
    for (auto& s : out)
        s = tock.process();
    return out;
}

double rms(const std::vector<float>& x, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return std::sqrt(sum / static_cast<double>(to - from));
}
} // namespace

TEST_CASE("The factory tocks are the documented ones, in order", "[dsp][tock][factory]")
{
    const char* expected[] = {"Click", "Knock", "Snap", "Tick"};
    REQUIRE(FactoryTocks::count() == 4);
    for (int i = 0; i < 4; ++i)
        REQUIRE(std::string(FactoryTocks::name(i)) == expected[i]);
    REQUIRE(std::string(FactoryTocks::name(-1)).empty());
    REQUIRE(std::string(FactoryTocks::name(4)).empty());
}

TEST_CASE("Factory tocks are finite, peak at 1, short and end near silence", "[dsp][tock][factory]")
{
    for (int i = 0; i < FactoryTocks::count(); ++i)
    {
        const auto sample = FactoryTocks::build(i);
        INFO("tock " << FactoryTocks::name(i));
        REQUIRE(sample != nullptr);
        REQUIRE(sample->sampleRate == FactoryTocks::kSampleRate);
        REQUIRE(sample->samples.size() > 50);
        REQUIRE(sample->samples.size() < static_cast<size_t>(0.2 * FactoryTocks::kSampleRate));

        float peak = 0.0f;
        double mean = 0.0;
        for (float s : sample->samples)
        {
            REQUIRE(std::isfinite(s));
            peak = std::max(peak, std::abs(s));
            mean += s;
        }
        REQUIRE_THAT(peak, WithinAbs(1.0, 1.0e-5));
        REQUIRE(std::abs(mean / static_cast<double>(sample->samples.size())) < 0.1);
        REQUIRE(std::abs(sample->samples.back()) < 0.05f);
    }
    // Deterministic, and out-of-range falls back to the first.
    REQUIRE(FactoryTocks::build(1)->samples == FactoryTocks::build(1)->samples);
    REQUIRE(FactoryTocks::build(-3)->samples == FactoryTocks::build(0)->samples);
    REQUIRE(FactoryTocks::build(99)->samples == FactoryTocks::build(0)->samples);
}

TEST_CASE("An idle Tock is exactly silent, a started one sounds and then stops", "[dsp][tock]")
{
    Tock tock;
    tock.prepare(kSampleRate);
    for (float s : render(tock, 100))
        REQUIRE(s == 0.0f);

    tock.start(TockSettings{});
    const auto out = render(tock, static_cast<int>(kSampleRate * 0.5));
    REQUIRE(fcm::test::peak(out) > 0.05f);
    REQUIRE_FALSE(tock.isRunning());
    for (size_t i = out.size() - 10; i < out.size(); ++i)
        REQUIRE(out[i] == 0.0f);
}

TEST_CASE("Synth decay falls by about 60 dB at the Decay time", "[dsp][tock]")
{
    for (const float decayMs : {10.0f, 40.0f, 120.0f})
    {
        Tock tock;
        tock.prepare(kSampleRate);
        TockSettings settings;
        settings.decayMs = decayMs;
        settings.tone = 1.0f;
        tock.start(settings);
        const auto out = render(tock, static_cast<int>(kSampleRate * 0.4));

        // Energy in a short window right at the start and one centred on the Decay time.
        const size_t window = static_cast<size_t>(0.001 * kSampleRate);
        const double early = rms(out, 0, window * 2);
        const auto at = static_cast<size_t>(decayMs * 0.001 * kSampleRate);
        const double late = rms(out, at - window / 2, at + window / 2);

        INFO("decay " << decayMs << " ms");
        const double dropDb = 20.0 * std::log10(early / std::max(late, 1.0e-12));
        // The window average is within a few dB of the envelope at those points.
        REQUIRE(dropDb > 45.0);
        REQUIRE(dropDb < 75.0);
    }
}

TEST_CASE("Start delays the Tock by that many samples", "[dsp][tock]")
{
    Tock tock;
    tock.prepare(kSampleRate);
    TockSettings settings;
    settings.startMs = 5.0f;
    tock.start(settings);
    const auto out = render(tock, 1000);

    const auto delay = static_cast<size_t>(5.0 * 0.001 * kSampleRate);
    for (size_t i = 0; i < delay; ++i)
        REQUIRE(out[i] == 0.0f);
    float after = 0.0f;
    for (size_t i = delay; i < delay + 100; ++i)
        after = std::max(after, std::abs(out[i]));
    REQUIRE(after > 0.01f);
}

TEST_CASE("Tune moves the synth tone by octaves", "[dsp][tock]")
{
    const auto frequencyAt = [](float tune)
    {
        Tock tock;
        tock.prepare(kSampleRate);
        TockSettings settings;
        settings.tuneSemitones = tune;
        settings.decayMs = 200.0f;
        settings.tone = 1.0f;
        tock.start(settings);
        // The noise falls with the cube of the envelope, the tone with the envelope itself, so
        // from 100 ms on the tone dominates and its zero crossings give its frequency.
        const auto out = render(tock, static_cast<int>(0.12 * kSampleRate));
        const auto from = static_cast<size_t>(0.1 * kSampleRate);
        return fcm::test::measureFrequency(out.data() + from, static_cast<int>(out.size() - from),
                                           kSampleRate);
    };

    const double base = frequencyAt(0.0f);
    REQUIRE_THAT(base, WithinAbs(1200.0, 200.0));
    REQUIRE_THAT(frequencyAt(12.0f) / base, WithinAbs(2.0, 0.3));
    REQUIRE_THAT(frequencyAt(-12.0f) / base, WithinAbs(0.5, 0.15));
}

TEST_CASE("Tune changes the playback rate in Sample mode", "[dsp][tock]")
{
    // A sample that is one second of a 1 kHz sine at 48 kHz.
    TockSample sample;
    sample.sampleRate = 48000.0;
    for (int i = 0; i < 48000; ++i)
        sample.samples.push_back(static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * i / 48000.0)));

    const auto frequencyAt = [&](float tune)
    {
        Tock tock;
        tock.prepare(kSampleRate);
        tock.setSample(&sample);
        TockSettings settings;
        settings.sampleMode = true;
        settings.tuneSemitones = tune;
        settings.decayMs = 200.0f;
        settings.tone = 1.0f;
        tock.start(settings);
        const auto out = render(tock, 2000);
        return fcm::test::measureFrequency(out.data(), static_cast<int>(out.size()), kSampleRate);
    };

    const double base = frequencyAt(0.0f);
    REQUIRE_THAT(base, WithinAbs(1000.0, 60.0));
    REQUIRE_THAT(frequencyAt(12.0f) / base, WithinAbs(2.0, 0.15));
}

TEST_CASE("A sample at another sample rate plays at the same pitch", "[dsp][tock]")
{
    TockSample sample;
    sample.sampleRate = 96000.0; // a 1 kHz sine recorded at 96 kHz
    for (int i = 0; i < 96000; ++i)
        sample.samples.push_back(static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * i / 96000.0)));

    Tock tock;
    tock.prepare(kSampleRate);
    tock.setSample(&sample);
    TockSettings settings;
    settings.sampleMode = true;
    settings.decayMs = 200.0f;
    settings.tone = 1.0f;
    tock.start(settings);
    const auto out = render(tock, 2000);
    REQUIRE_THAT(fcm::test::measureFrequency(out.data(), static_cast<int>(out.size()), kSampleRate),
                 WithinAbs(1000.0, 60.0));
}

TEST_CASE("Tone 0 is much duller than Tone 1", "[dsp][tock]")
{
    const auto highEnergy = [](float tone)
    {
        Tock tock;
        tock.prepare(kSampleRate);
        TockSettings settings;
        settings.tone = tone;
        settings.decayMs = 30.0f;
        tock.start(settings);
        const auto out = render(tock, 4800);

        // Energy above about 2 kHz: the signal minus a moving average of 12 samples (4 kHz low-pass).
        double energy = 0.0;
        for (size_t i = 12; i < out.size(); ++i)
        {
            double average = 0.0;
            for (size_t k = 0; k < 12; ++k)
                average += out[i - k];
            average /= 12.0;
            const double high = out[i] - average;
            energy += high * high;
        }
        return energy;
    };

    REQUIRE(highEnergy(1.0f) > 10.0 * highEnergy(0.0f));
}

TEST_CASE("Sample mode without a sample is silent", "[dsp][tock]")
{
    Tock tock;
    tock.prepare(kSampleRate);
    TockSettings settings;
    settings.sampleMode = true;
    tock.start(settings);
    for (float s : render(tock, 1000))
        REQUIRE(s == 0.0f);
}

TEST_CASE("Extreme and invalid settings give finite output at every sample rate", "[dsp][tock]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const auto sample = FactoryTocks::build(1);

    for (const double rate : {44100.0, 48000.0, 96000.0})
        for (const bool sampleMode : {false, true})
            for (const float tune : {-24.0f, 24.0f, 1000.0f, nan, inf, -inf})
                for (const float decay : {0.0f, 2.0f, 200.0f, 1.0e9f, nan, inf, -5.0f})
                    for (const float tone : {-3.0f, 0.0f, 1.0f, 9.0f, nan})
                    {
                        Tock tock;
                        tock.prepare(rate);
                        tock.setSample(sample.get());
                        TockSettings settings;
                        settings.sampleMode = sampleMode;
                        settings.tuneSemitones = tune;
                        settings.decayMs = decay;
                        settings.tone = tone;
                        settings.startMs = (decay > 100.0f) ? 20.0f : nan;
                        tock.start(settings);
                        for (int i = 0; i < 600; ++i)
                            REQUIRE(std::isfinite(tock.process()));
                    }
}

TEST_CASE("Reset makes two runs bit-identical", "[dsp][tock]")
{
    Tock tock;
    tock.prepare(kSampleRate);
    TockSettings settings;
    settings.decayMs = 50.0f;
    tock.start(settings);
    const auto first = render(tock, 3000);

    tock.reset();
    tock.start(settings);
    REQUIRE(render(tock, 3000) == first);
}

namespace
{
std::vector<float> playEngine(const EngineParameters& parameters, int blocks = 40)
{
    constexpr int kBlock = 256;
    PublishedPointer<TockSample> source;
    source.publish(FactoryTocks::build(0));

    Engine engine;
    engine.setTockSource(&source);
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, kBlock, 2});

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
        buffer.clear();
        engine.process(buffer, midi);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    return out;
}
} // namespace

TEST_CASE("With the Tock off the kick is exactly as before", "[dsp][tock][engine]")
{
    EngineParameters off; // tock.on is false by default
    EngineParameters withTockSettings = off;
    withTockSettings.tock.level = 1.0f;
    withTockSettings.tock.decayMs = 200.0f;

    REQUIRE(playEngine(off) == playEngine(withTockSettings));
}

TEST_CASE("The Tock adds energy to the start of the kick and leaves the tail alone", "[dsp][tock][engine]")
{
    EngineParameters off;
    EngineParameters on = off;
    on.tock.on = true;
    on.tock.level = 1.0f;
    on.tock.startMs = 2.0f;
    on.tock.decayMs = 12.0f;
    on.tock.tone = 1.0f;

    const auto without = playEngine(off);
    const auto with = playEngine(on);
    REQUIRE(with.size() == without.size());

    double difference = 0.0;
    for (size_t i = 0; i < static_cast<size_t>(0.03 * kSampleRate); ++i)
        difference += std::abs(with[i] - without[i]);
    REQUIRE(difference > 1.0);

    // Long after the Tock has faded, the two are the same.
    for (size_t i = static_cast<size_t>(0.2 * kSampleRate); i < with.size(); ++i)
        REQUIRE_THAT(with[i], WithinAbs(without[i], 1.0e-4));
    for (float s : with)
        REQUIRE(std::isfinite(s));
}

TEST_CASE("Tock settings are read when the note starts", "[dsp][tock][engine]")
{
    constexpr int kBlock = 128;
    EngineParameters parameters;
    parameters.tock.on = true;
    parameters.tock.level = 1.0f;
    parameters.tock.decayMs = 100.0f;

    Engine engine;
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, kBlock, 2});

    const auto run = [&](bool changeMidKick)
    {
        engine.reset();
        std::vector<float> out;
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < 12; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
            if (block == 3 && changeMidKick)
            {
                auto changed = parameters;
                changed.tock.tuneSemitones = 12.0f;
                changed.tock.decayMs = 5.0f;
                changed.tock.tone = 0.0f;
                engine.setParameters(changed);
            }
            buffer.clear();
            engine.process(buffer, midi);
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        }
        engine.setParameters(parameters);
        return out;
    };

    REQUIRE(run(true) == run(false));
}

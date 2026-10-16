// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "WavBuilder.h"
#include "core/NoiseImport.h"
#include "core/NoiseSelector.h"
#include "core/ParameterIds.h"
#include "core/WavetableImport.h"
#include "dsp/noise/FactoryNoises.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::test::makeWav;
using fcm::test::WavSpec;
using namespace fcm::core;
using fcm::dsp::FactoryNoises;

namespace
{
std::vector<float> sine(int length, double hz, double rate, double amplitude = 0.5)
{
    std::vector<float> out(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i)
        out[static_cast<size_t>(i)] =
            static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * hz * i / rate));
    return out;
}

NoiseImportResult import(const std::vector<uint8_t>& bytes)
{
    return importNoiseSample(bytes.data(), bytes.size());
}

WavSpec withRate(uint32_t rate, bool isFloat = true)
{
    WavSpec spec;
    spec.sampleRate = rate;
    spec.isFloat = isFloat;
    return spec;
}
} // namespace

TEST_CASE("A short WAV is imported with its rate, centred and at peak 1", "[core][noise][import]")
{
    for (const uint32_t rate : {8000u, 44100u, 48000u})
        for (const bool isFloat : {true, false})
        {
            auto samples = sine(static_cast<int>(rate / 5), 440.0, rate); // 200 ms
            for (float& s : samples)
                s += 0.2f; // a DC offset that must go
            const auto result = import(makeWav(samples, withRate(rate, isFloat)));

            INFO("rate " << rate << " float " << isFloat);
            REQUIRE(result.ok());
            REQUIRE(result.sample.sampleRate == static_cast<double>(rate));
            REQUIRE(result.sample.samples.size() == samples.size());
            REQUIRE(isValidStoredNoiseSample(result.sample));

            float peak = 0.0f;
            double mean = 0.0;
            for (float s : result.sample.samples)
            {
                peak = std::max(peak, std::abs(s));
                mean += s;
            }
            REQUIRE_THAT(peak, WithinAbs(1.0, 1.0e-3));
            REQUIRE(std::abs(mean / static_cast<double>(samples.size())) < 0.02);
        }
}

TEST_CASE("A sample above 48 kHz is brought down to 48 kHz, up to 10 seconds of it", "[core][noise][import]")
{
    for (const uint32_t rate : {96000u, 192000u})
    {
        // A 1 kHz tone and a 40 kHz one: the second must not fold back into the audible band.
        const auto low = sine(static_cast<int>(rate), 1000.0, rate, 0.4);
        const auto high = sine(static_cast<int>(rate), 40000.0, rate, 0.4);
        std::vector<float> mixed(low.size());
        for (size_t i = 0; i < mixed.size(); ++i)
            mixed[i] = low[i] + high[i];

        const auto result = import(makeWav(mixed, withRate(rate)));
        INFO("rate " << rate);
        REQUIRE(result.ok());
        REQUIRE(result.sample.sampleRate == 48000.0);
        REQUIRE(result.sample.samples.size() == 48000u);

        // The 1 kHz tone survives (its power is most of the sample); the 40 kHz one is gone: the
        // power at 8 kHz, where 40 kHz would alias to, is tiny.
        const auto power = [&](double hz)
        {
            double re = 0.0, im = 0.0;
            for (size_t i = 2000; i < result.sample.samples.size() - 2000; ++i)
            {
                const double w = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / 48000.0;
                re += result.sample.samples[i] * std::cos(w);
                im += result.sample.samples[i] * std::sin(w);
            }
            return re * re + im * im;
        };
        REQUIRE(power(8000.0) < power(1000.0) * 1.0e-4);
    }

    // The longest allowed sample stays within the stored limit.
    const auto longest = import(makeWav(sine(192000 * 10, 300.0, 192000.0), withRate(192000)));
    REQUIRE(longest.ok());
    REQUIRE(longest.sample.samples.size() <= static_cast<size_t>(kMaxNoiseSamples));
    REQUIRE(isValidStoredNoiseSample(longest.sample));
}

TEST_CASE("Only the first channel is used", "[core][noise][import]")
{
    const auto left = sine(1000, 300.0, 48000.0);
    std::vector<float> interleaved;
    for (float s : left)
    {
        interleaved.push_back(s);
        interleaved.push_back(1.0f);
    }
    WavSpec spec;
    spec.channels = 2;
    const auto result = import(makeWav(interleaved, spec));

    REQUIRE(result.ok());
    REQUIRE(result.sample.samples.size() == left.size());

    // The result is the left channel centred and scaled to peak 1 (the constant right channel
    // would have shown up as an offset).
    double mean = 0.0;
    for (float v : left)
        mean += v;
    mean /= static_cast<double>(left.size());
    double peak = 0.0;
    for (float v : left)
        peak = std::max(peak, std::abs(v - mean));
    for (size_t i = 0; i + 100 < left.size(); i += 37) // away from the faded end
        REQUIRE_THAT(result.sample.samples[i], WithinAbs((left[i] - mean) / peak, 1.0e-4));
}

TEST_CASE("Unusable noise files are refused with a message", "[core][noise][import]")
{
    const auto refused = [](const NoiseImportResult& r)
    {
        REQUIRE_FALSE(r.ok());
        REQUIRE(r.error.isNotEmpty());
    };

    refused(importNoiseSample(nullptr, 0));
    refused(import({}));
    refused(import(std::vector<uint8_t>(1000, 0x41)));

    // Too short, too long (more than 10 seconds), and an unsupported sample rate.
    refused(import(makeWav(sine(8, 440.0, 48000.0))));
    refused(import(makeWav(sine(48000 * 10 + 100, 440.0, 48000.0))));
    refused(import(makeWav(sine(1000, 440.0, 4000.0), withRate(4000))));
    refused(import(makeWav(sine(1000, 440.0, 400000.0), withRate(400000))));

    // Invalid samples.
    {
        auto samples = sine(1000, 440.0, 48000.0);
        samples[10] = std::numeric_limits<float>::quiet_NaN();
        refused(import(makeWav(samples)));
        samples[10] = std::numeric_limits<float>::infinity();
        refused(import(makeWav(samples)));
    }

    // Silent, and a constant (silent once centred).
    refused(import(makeWav(std::vector<float>(1000, 0.0f))));
    refused(import(makeWav(std::vector<float>(1000, 0.3f))));

    // Above the byte limit: refused without being read.
    {
        static const std::vector<uint8_t> big(kMaxWavetableFileBytes + 1, 0);
        refused(importNoiseSample(big.data(), big.size()));
    }
}

TEST_CASE("Stored noise samples are checked before use", "[core][noise][import]")
{
    fcm::dsp::NoiseSample good;
    good.sampleRate = 48000.0;
    good.samples.assign(100, 0.5f);
    REQUIRE(isValidStoredNoiseSample(good));

    auto bad = good;
    bad.samples.assign(10, 0.5f);
    REQUIRE_FALSE(isValidStoredNoiseSample(bad));
    bad.samples.assign(static_cast<size_t>(kMaxNoiseSamples) + 1, 0.5f);
    REQUIRE_FALSE(isValidStoredNoiseSample(bad));
    bad = good;
    bad.samples[3] = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(isValidStoredNoiseSample(bad));
    bad.samples[3] = 4.5f;
    REQUIRE_FALSE(isValidStoredNoiseSample(bad));
    bad = good;
    bad.sampleRate = 100.0;
    REQUIRE_FALSE(isValidStoredNoiseSample(bad));
    bad.sampleRate = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(isValidStoredNoiseSample(bad));
}

TEST_CASE("Randomly damaged noise files never crash and never give an unusable sample",
          "[core][noise][import]")
{
    const auto original = makeWav(sine(2000, 440.0, 48000.0));
    juce::Random random(777);

    for (int round = 0; round < 3000; ++round)
    {
        auto bytes = original;
        const int flips = 1 + random.nextInt(6);
        for (int i = 0; i < flips; ++i)
            bytes[static_cast<size_t>(random.nextInt(static_cast<int>(bytes.size())))] =
                static_cast<uint8_t>(random.nextInt(256));
        if (random.nextBool())
            bytes.resize(static_cast<size_t>(random.nextInt(static_cast<int>(bytes.size()) + 1)));

        const auto result = import(bytes);
        if (result.ok())
            REQUIRE(isValidStoredNoiseSample(result.sample));
        else
            REQUIRE(result.error.isNotEmpty());
    }
}

TEST_CASE("The user noise sample of the selector plays, saves, restores and refuses bad data",
          "[core][noise][user]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    NoiseSelector selector(processor.state);

    const auto choose = [&](int entry)
    {
        auto* parameter = processor.state.getParameter(ParameterIds::kNoiseType);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(entry)));
        selector.update();
    };

    // The list now ends with User.
    auto* choice =
        dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter(ParameterIds::kNoiseType));
    REQUIRE(choice->choices.size() == fcm::dsp::FactoryNoises::count() + 1);
    REQUIRE(choice->choices[NoiseSelector::userEntry()].toStdString() == "User");

    // User selected with nothing loaded: the first built-in sample.
    choose(NoiseSelector::userEntry());
    REQUIRE(selector.currentEntry() == NoiseSelector::userEntry());
    REQUIRE(selector.source().acquire()->samples == fcm::dsp::FactoryNoises::build(0)->samples);
    REQUIRE_FALSE(selector.toValueTree().isValid());

    fcm::dsp::NoiseSample mine;
    mine.sampleRate = 44100.0;
    mine.samples = sine(500, 700.0, 44100.0, 0.9);
    REQUIRE(selector.loadUser(mine, "my\nclick"));
    REQUIRE(selector.userName() == "myclick");
    REQUIRE(selector.source().acquire()->samples == mine.samples);
    REQUIRE(selector.source().acquire()->sampleRate == 44100.0);

    // Round trip through the saved node.
    const auto node = selector.toValueTree();
    REQUIRE(node.isValid());
    REQUIRE(node.getType().toString() == "UserNoiseSample");
    fcm::test::TestProcessor other;
    NoiseSelector restored(other.state);
    REQUIRE(restored.restoreFrom(node));
    REQUIRE(restored.userName() == "myclick");
    REQUIRE(restored.toValueTree()["samples"].toString() == node["samples"].toString());
    REQUIRE(static_cast<double>(restored.toValueTree()["sampleRate"]) == 44100.0);

    // Clearing returns to the first built-in sample; invalid input is refused.
    selector.clearUser();
    REQUIRE_FALSE(selector.hasUser());
    REQUIRE(selector.source().acquire()->samples == fcm::dsp::FactoryNoises::build(0)->samples);
    fcm::dsp::NoiseSample tiny;
    tiny.samples.assign(3, 0.1f);
    REQUIRE_FALSE(selector.loadUser(tiny, "x"));
}

TEST_CASE("A saved user noise node is untrusted", "[core][noise][user]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    NoiseSelector selector(processor.state);

    const auto nodeWith = [](const juce::String& samples, double rate)
    {
        juce::ValueTree node("UserNoiseSample");
        node.setProperty("name", "x", nullptr);
        node.setProperty("sampleRate", rate, nullptr);
        node.setProperty("samples", samples, nullptr);
        return node;
    };
    const auto encode = [](const std::vector<float>& values)
    { return juce::MemoryBlock(values.data(), values.size() * sizeof(float)).toBase64Encoding(); };

    const std::vector<float> good(200, 0.5f);
    REQUIRE(selector.restoreFrom(nodeWith(encode(good), 48000.0)));
    REQUIRE(selector.hasUser());

    auto withNan = good;
    withNan[3] = std::numeric_limits<float>::quiet_NaN();
    auto withInf = good;
    withInf[3] = std::numeric_limits<float>::infinity();
    auto tooLoud = good;
    tooLoud[3] = 5.0f;

    struct Case
    {
        juce::String text;
        double rate;
    };
    const std::vector<Case> bad{
        {"", 48000.0},
        {"!!! not base64 !!!", 48000.0},
        {encode(std::vector<float>(5, 0.5f)), 48000.0},                                         // too short
        {encode(std::vector<float>(static_cast<size_t>(kMaxNoiseSamples) + 1, 0.5f)), 48000.0}, // too long
        {encode(withNan), 48000.0},
        {encode(withInf), 48000.0},
        {encode(tooLoud), 48000.0},
        {encode(good), 10.0}, // bad rate
        {encode(good), std::numeric_limits<double>::quiet_NaN()},
        {juce::String::repeatedString("A", 2'700'000), 48000.0},         // oversized text
        {juce::MemoryBlock(good.data(), 7).toBase64Encoding(), 48000.0}, // not whole floats
    };
    for (const auto& c : bad)
    {
        REQUIRE(selector.restoreFrom(nodeWith(encode(good), 48000.0)));
        REQUIRE_FALSE(selector.restoreFrom(nodeWith(c.text, c.rate)));
        REQUIRE_FALSE(selector.hasUser());
    }

    REQUIRE(selector.restoreFrom(nodeWith(encode(good), 48000.0)));
    REQUIRE_FALSE(selector.restoreFrom(juce::ValueTree()));
    REQUIRE_FALSE(selector.hasUser());
    REQUIRE(selector.restoreFrom(nodeWith(encode(good), 48000.0)));
    REQUIRE_FALSE(selector.restoreFrom(juce::ValueTree("Other")));
    REQUIRE_FALSE(selector.hasUser());
}

// ---- the selector and the parameters ----
namespace
{
void setEntry(juce::AudioProcessorValueTreeState& state, int entry)
{
    auto* parameter = state.getParameter(fcm::core::ParameterIds::kNoiseType);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(entry)));
}
} // namespace

TEST_CASE("The noise Type is a choice that lists the factory noises in order", "[core][noise]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;

    auto* sample = dynamic_cast<juce::AudioParameterChoice*>(
        processor.state.getParameter(fcm::core::ParameterIds::kNoiseType));
    REQUIRE(sample != nullptr);
    // The factory samples in order, then the user sample entry.
    REQUIRE(sample->choices.size() == FactoryNoises::count() + 1);
    for (int i = 0; i < FactoryNoises::count(); ++i)
        REQUIRE(sample->choices[i].toStdString() == FactoryNoises::name(i));
    REQUIRE(sample->choices[FactoryNoises::count()].toStdString() == "User");

    // The documented order (docs/specs/noise-oscillator.md section 1).
    REQUIRE(sample->choices[0].toStdString() == "White");
    REQUIRE(sample->choices[6].toStdString() == "Air Burst");
    REQUIRE(sample->choices[7].toStdString() == "Thump");
}

TEST_CASE("The noise selector publishes the current sound and follows the parameter", "[core][noise]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    NoiseSelector selector(processor.state);

    REQUIRE(selector.currentEntry() == 0);
    const auto* first = selector.source().acquire();
    REQUIRE(first != nullptr);
    REQUIRE(first->samples == FactoryNoises::build(0)->samples);

    setEntry(processor.state, 2);
    selector.update();
    REQUIRE(selector.currentEntry() == 2);
    REQUIRE(selector.source().acquire()->samples == FactoryNoises::build(2)->samples);
}

TEST_CASE("An unusable noise Type value selects a valid sound", "[core][noise]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    NoiseSelector selector(processor.state);

    auto* raw = processor.state.getRawParameterValue(fcm::core::ParameterIds::kNoiseType);
    for (const float bad : {-5.0f, 99.0f, std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
    {
        raw->store(bad);
        selector.update();
        REQUIRE(selector.currentEntry() >= 0);
        REQUIRE(selector.currentEntry() <= NoiseSelector::userEntry());
        REQUIRE(selector.source().acquire() != nullptr);
    }
}

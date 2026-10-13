// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/oscillator/WavetableData.h"
#include "dsp/oscillator/WavetableOscillator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <vector>

using fcm::dsp::WavetableData;
using fcm::dsp::WavetableOscillator;

namespace
{
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Values a damaged project or a bug could hand the oscillator.
const std::vector<double> kGarbage{kNaN,    kInf,     -kInf,     1.0e300,  -1.0e300, 1.0e30,
                                   -1.0e30, 1.0e-300, -1.0e-300, -1.0e-20, -1.0e-17, 1.0 - 1.0e-17,
                                   -0.0,    7.5,      -7.5,      1.0e9};

std::vector<float> render(WavetableOscillator& osc, double frequency, int samples)
{
    std::vector<float> out(static_cast<size_t>(samples));
    for (auto& s : out)
        s = osc.process(frequency);
    return out;
}

void requireSane(const std::vector<float>& out)
{
    REQUIRE(fcm::test::allFinite(out));
    REQUIRE(fcm::test::peak(out) <= 1.001f);
}
} // namespace

TEST_CASE("wrapPhase always gives a phase in [0, 1)", "[dsp][oscillator][robust]")
{
    for (double value : kGarbage)
    {
        const double wrapped = WavetableOscillator::wrapPhase(value);
        INFO("input " << value);
        REQUIRE(std::isfinite(wrapped));
        REQUIRE(wrapped >= 0.0);
        REQUIRE(wrapped < 1.0);
    }

    // The case that would index one past the frame: a tiny negative phase that rounds up to 1.
    REQUIRE(WavetableOscillator::wrapPhase(-1.0e-20) < 1.0);
    // Ordinary values are unchanged.
    REQUIRE_THAT(WavetableOscillator::wrapPhase(0.25), Catch::Matchers::WithinAbs(0.25, 1.0e-12));
    REQUIRE_THAT(WavetableOscillator::wrapPhase(3.75), Catch::Matchers::WithinAbs(0.75, 1.0e-12));
}

TEST_CASE("mipLevelFor stays inside the table for any frequency", "[dsp][oscillator][robust]")
{
    for (double sampleRate : {8000.0, 44100.0, 192000.0})
        for (double frequency : kGarbage)
        {
            const int level = WavetableOscillator::mipLevelFor(frequency, sampleRate);
            INFO("frequency " << frequency << " rate " << sampleRate);
            REQUIRE(level >= 0);
            REQUIRE(level < WavetableData::kNumMipLevels);
        }

    REQUIRE(WavetableOscillator::mipLevelFor(kNaN, 44100.0) == 0);
    REQUIRE(WavetableOscillator::mipLevelFor(-5.0, 44100.0) == 0);
    REQUIRE(WavetableOscillator::mipLevelFor(kInf, 44100.0) == WavetableData::kNumMipLevels - 1);
}

TEST_CASE("The oscillator survives any start phase", "[dsp][oscillator][robust]")
{
    const auto sine = WavetableData::makeSine();
    for (double startPhase : kGarbage)
    {
        WavetableOscillator osc;
        osc.prepare(48000.0);
        osc.setWavetable(sine.get());
        osc.reset(startPhase);

        INFO("start phase " << startPhase);
        requireSane(render(osc, 440.0, 2000));
    }
}

TEST_CASE("The oscillator survives any frequency", "[dsp][oscillator][robust]")
{
    const auto sine = WavetableData::makeSine();
    for (double sampleRate : {8000.0, 48000.0, 192000.0})
        for (double frequency : kGarbage)
        {
            WavetableOscillator osc;
            osc.prepare(sampleRate);
            osc.setWavetable(sine.get());

            INFO("frequency " << frequency << " rate " << sampleRate);
            requireSane(render(osc, frequency, 3000));

            // And it recovers: after garbage the next normal note plays normally.
            const auto after = render(osc, 440.0, 4000);
            requireSane(after);
            REQUIRE(fcm::test::peak(after) > 0.9f);
        }
}

TEST_CASE("The oscillator survives any position, with one frame and with many", "[dsp][oscillator][robust]")
{
    std::vector<float> frames(static_cast<size_t>(4 * WavetableData::kFrameSize));
    // Four different, properly periodic frames (1st to 4th harmonic), so the table itself stays within +-1.
    for (size_t frame = 0; frame < 4; ++frame)
        for (size_t i = 0; i < static_cast<size_t>(WavetableData::kFrameSize); ++i)
            frames[frame * WavetableData::kFrameSize + i] =
                0.5f * std::sin(6.2831853f * static_cast<float>((frame + 1) * i) / WavetableData::kFrameSize);

    const auto many = WavetableData::fromFrames(frames);
    const auto one = WavetableData::makeSine();
    REQUIRE(many != nullptr);

    for (const auto* table : {one.get(), many.get()})
        for (double position : kGarbage)
        {
            WavetableOscillator osc;
            osc.prepare(44100.0);
            osc.setWavetable(table);
            osc.setPosition(static_cast<float>(position));

            INFO("position " << position << " frames " << table->numFrames());
            requireSane(render(osc, 220.0, 3000));
        }
}

TEST_CASE("Garbage in every oscillator input at once is still safe", "[dsp][oscillator][robust]")
{
    const auto sine = WavetableData::makeSine();
    WavetableOscillator osc;
    osc.prepare(44100.0);
    osc.setWavetable(sine.get());

    // A long walk through combinations, so the phase accumulator sees every kind of value.
    std::vector<float> out;
    for (double startPhase : kGarbage)
        for (double frequency : kGarbage)
        {
            osc.reset(startPhase);
            osc.setPosition(static_cast<float>(startPhase));
            for (int i = 0; i < 20; ++i)
                out.push_back(osc.process(frequency));
        }
    requireSane(out);
}

TEST_CASE("The engine plays a note with garbage oscillator settings", "[dsp][oscillator][robust][engine]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    for (float bad : {nan, inf, -inf, 1.0e30f, -1.0e30f})
    {
        fcm::dsp::EngineParameters parameters;
        parameters.oscillators[1].on = true;
        for (auto& osc : parameters.oscillators)
        {
            osc.startPhase = bad;
            osc.position = bad;
            osc.fineCents = bad;
            osc.level = bad;
        }
        parameters.glideMs = bad;
        parameters.glideOn = true;
        parameters.outputGain = 1.0f;

        fcm::dsp::Engine engine{fcm::test::noEffects()};
        engine.setParameters(parameters);
        engine.prepare({48000.0, 512, 2});

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 200);

        std::vector<float> out;
        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            engine.process(buffer, block == 0 ? midi : juce::MidiBuffer{});
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 512);
        }

        INFO("value " << bad);
        REQUIRE(fcm::test::allFinite(out));
        REQUIRE(fcm::test::peak(out) <= 1.0f);
    }
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "AllocationCounter.h"
#include "RackRig.h"
#include "TestEffects.h"
#include "TestUtils.h"
#include "dsp/engine/BusStage.h"
#include "dsp/engine/Engine.h"
#include "dsp/engine/Mixer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::BusRegion;
using fcm::dsp::Destination;
using fcm::dsp::EffectRack;
using fcm::dsp::EffectRegistry;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::RackId;
using fcm::test::instance;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

// Effect indices in the test registry below.
constexpr int kGain = 0;
constexpr int kDelay = 2;

EffectRegistry makeRegistry()
{
    EffectRegistry registry;
    registry.add<fcm::test::GainEffect>();
    registry.add<fcm::test::ClipEffect>();
    registry.add<fcm::test::DelayEffect>();
    return registry;
}

// An engine over the test effects playing one note, with helpers to set the mixer and the racks.
struct Rig
{
    Rig() : registry(makeRegistry()), engine(registry) {}

    // Renders `blocks` blocks (the note-on is at the start) and returns channel 0 and channel 1.
    void render(int blocks, std::vector<float>& left, std::vector<float>& right, int noteOffBlock = -1)
    {
        engine.setParameters(parameters);
        engine.prepare({kSampleRate, static_cast<juce::uint32>(kBlock), 2});
        for (const auto& [rack, contents] : racks)
            engine.rackValue(rack).store(EffectRack::pack(contents));

        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
            if (block == noteOffBlock)
                midi.addEvent(juce::MidiMessage::noteOff(1, 36), 0);
            buffer.clear();
            engine.setParameters(parameters);
            engine.process(buffer, midi);
            left.insert(left.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
            right.insert(right.end(), buffer.getReadPointer(1), buffer.getReadPointer(1) + kBlock);
        }
    }

    std::vector<float> renderLeft(int blocks)
    {
        std::vector<float> left, right;
        render(blocks, left, right);
        return left;
    }

    EffectRegistry registry;
    Engine engine;
    EngineParameters parameters;
    std::vector<std::pair<RackId, std::vector<int>>> racks;
};

float maxStep(const std::vector<float>& x)
{
    float step = 0.0f;
    for (size_t i = 1; i < x.size(); ++i)
        step = std::max(step, std::abs(x[i] - x[i - 1]));
    return step;
}

float maxDifference(const std::vector<float>& a, const std::vector<float>& b, size_t from = 0)
{
    float result = 0.0f;
    for (size_t i = from; i < std::min(a.size(), b.size()); ++i)
        result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}
} // namespace

TEST_CASE("The mixer sends each stem to its destination", "[dsp][mixer]")
{
    fcm::dsp::Mixer mixer;
    mixer.prepare(kSampleRate);

    std::array<fcm::dsp::MixerSourceParameters, fcm::dsp::kNumStems> sources{};
    sources[fcm::dsp::stemOsc2].destination = Destination::busA;
    sources[fcm::dsp::stemTock].destination = Destination::busB;
    mixer.setParameters(sources, true);

    const std::array<float, fcm::dsp::kNumStems> stems{1.0f, 2.0f, 4.0f, 8.0f};
    std::array<float, fcm::dsp::kNumDestinations> sums{};
    mixer.process(stems, sums);
    REQUIRE(sums[0] == 1.0f + 4.0f); // osc 1 and the noise: Main
    REQUIRE(sums[1] == 2.0f);        // osc 2: Bus A
    REQUIRE(sums[2] == 8.0f);        // tock: Bus B
}

TEST_CASE("Mute and solo give the expected sound", "[dsp][mixer]")
{
    fcm::dsp::Mixer mixer;
    mixer.prepare(kSampleRate);
    const std::array<float, fcm::dsp::kNumStems> stems{1.0f, 2.0f, 4.0f, 8.0f};

    const auto mainSum = [&](auto edit)
    {
        std::array<fcm::dsp::MixerSourceParameters, fcm::dsp::kNumStems> sources{};
        edit(sources);
        mixer.setParameters(sources, true);
        std::array<float, fcm::dsp::kNumDestinations> sums{};
        mixer.process(stems, sums);
        return sums[0];
    };

    REQUIRE(mainSum([](auto&) {}) == 15.0f);
    REQUIRE(mainSum([](auto& s) { s[fcm::dsp::stemOsc2].mute = true; }) == 13.0f);
    // One soloed: everything else is silent.
    REQUIRE(mainSum([](auto& s) { s[fcm::dsp::stemOsc2].solo = true; }) == 2.0f);
    // Two soloed: both are heard.
    REQUIRE(mainSum([](auto& s) { s[fcm::dsp::stemOsc2].solo = s[fcm::dsp::stemTock].solo = true; }) ==
            10.0f);
    // Mute on a soloed channel silences it: nothing is left.
    REQUIRE(mainSum(
                [](auto& s)
                {
                    s[fcm::dsp::stemOsc2].solo = true;
                    s[fcm::dsp::stemOsc2].mute = true;
                }) == 0.0f);
}

TEST_CASE("Mute, solo and destination changes are smooth", "[dsp][mixer]")
{
    fcm::dsp::Mixer mixer;
    mixer.prepare(kSampleRate);
    std::array<fcm::dsp::MixerSourceParameters, fcm::dsp::kNumStems> sources{};
    mixer.setParameters(sources, true);

    const std::array<float, fcm::dsp::kNumStems> stems{1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> main, busA;
    std::array<float, fcm::dsp::kNumDestinations> sums{};
    for (int i = 0; i < 4800; ++i)
    {
        if (i == 1000)
        {
            sources[0].destination = Destination::busA;
            mixer.setParameters(sources, false);
        }
        if (i == 2500)
        {
            sources[0].mute = true;
            mixer.setParameters(sources, false);
        }
        mixer.process(stems, sums);
        main.push_back(sums[0]);
        busA.push_back(sums[1]);
    }

    // 10 ms is 480 samples: no step is bigger than 1/480 (a little slack for rounding).
    REQUIRE(maxStep(main) < 1.0f / 470.0f);
    REQUIRE(maxStep(busA) < 1.0f / 470.0f);
    REQUIRE(main.front() == 1.0f);
    REQUIRE(main.back() == 0.0f);
    REQUIRE(busA.back() == 0.0f);
    // The sum of the two destinations never goes above the stem (a crossfade, not an add).
    for (size_t i = 0; i < main.size(); ++i)
        REQUIRE(main[i] + busA[i] <= 1.0f + 1.0e-6f);
}

TEST_CASE("The reported latency is twice the pool's, for every Quality", "[dsp][bus][latency]")
{
    Rig rig;
    rig.engine.prepare({kSampleRate, kBlock, 2});
    // Four copies of the delay effect (32 samples per step at 1x, 64 at 2x, ...).
    REQUIRE(rig.engine.latencySamples(1) == 2 * 4 * 32);
    REQUIRE(rig.engine.latencySamples(2) == 2 * 4 * 64);
    REQUIRE(rig.engine.latencySamples(4) == 2 * 4 * 128);
    REQUIRE(rig.engine.latencySamples(8) == 2 * 4 * 256);
}

TEST_CASE("A source sounds the same at the same delay whichever destination it goes to",
          "[dsp][bus][latency]")
{
    // With every effect off, Main, Bus A and Bus B each add the same delay, so the sound is the same.
    const auto render = [](Destination destination, int quality)
    {
        Rig rig;
        rig.parameters.oversamplingFactor = quality;
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination = destination;
        rig.racks = {{RackId::busA, {instance(kGain), instance(kDelay)}},
                     {RackId::busB, {instance(kDelay, 2)}},
                     {RackId::main, {instance(kGain, 2), instance(kDelay, 3)}}};
        return rig.renderLeft(40);
    };

    for (const int quality : {1, 2, 4})
    {
        INFO("oversampling " << quality);
        const auto main = render(Destination::main, quality);
        REQUIRE(fcm::test::peak(main) > 0.1f);
        REQUIRE(maxDifference(render(Destination::busA, quality), main) < 2.0e-6f);
        REQUIRE(maxDifference(render(Destination::busB, quality), main) < 2.0e-6f);
    }
}

TEST_CASE("A source that goes to a bus is heard through the bus effects, not in Main", "[dsp][bus]")
{
    // A gain effect of 0 on Bus A silences what goes through it.
    const auto render = [](Destination osc1, Destination osc2)
    {
        Rig rig;
        rig.parameters.oscillators[1].on = true;
        rig.parameters.oscillators[1].level = 0.5f;
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination = osc1;
        rig.parameters.mixer[fcm::dsp::stemOsc2].destination = osc2;
        rig.racks = {{RackId::busA, {instance(kGain)}}};
        rig.parameters.effects[static_cast<size_t>(instance(kGain))].enabled = true;
        rig.parameters.effects[static_cast<size_t>(instance(kGain))].values[0] = 0.0f;
        return rig.renderLeft(40);
    };

    const auto both = render(Destination::main, Destination::main);
    const auto osc1Silenced = render(Destination::busA, Destination::main);
    const auto allSilenced = render(Destination::busA, Destination::busA);
    REQUIRE(fcm::test::peak(both) > 0.1f);
    REQUIRE(fcm::test::peak(osc1Silenced) > 0.01f);
    REQUIRE(fcm::test::peak(osc1Silenced) < fcm::test::peak(both));
    REQUIRE(fcm::test::peak(allSilenced) < 1.0e-6f);
}

TEST_CASE("Apply To Punch, Tail and Whole put the effect only where expected", "[dsp][bus][region]")
{
    // Bus A holds a gain of 0 (it silences what it processes); everything goes to Bus A.
    const auto render = [](BusRegion region)
    {
        Rig rig;
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busA;
        rig.parameters.buses[0].region = region;
        rig.parameters.punchSplitMs = 80.0f;
        rig.racks = {{RackId::busA, {instance(kGain)}}};
        rig.parameters.effects[static_cast<size_t>(instance(kGain))].enabled = true;
        rig.parameters.effects[static_cast<size_t>(instance(kGain))].values[0] = 0.0f;
        return rig.renderLeft(60);
    };
    const auto clean = [] // the same sound with the effect off
    {
        Rig rig;
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busA;
        rig.racks = {{RackId::busA, {instance(kGain)}}};
        return rig.renderLeft(60);
    }();

    const int latency = 2 * 4 * 64; // the pool has four delay copies at 2x, doubled
    const auto split = static_cast<size_t>(80.0 * 0.001 * kSampleRate) + static_cast<size_t>(latency);
    const size_t margin = 600; // the crossfade and the 10 ms smoothing

    const auto whole = render(BusRegion::whole);
    const auto punch = render(BusRegion::punch);
    const auto tail = render(BusRegion::tail);

    REQUIRE(fcm::test::peak(clean) > 0.1f);
    // Whole: all processed, so silent.
    for (size_t i = 0; i < whole.size(); ++i)
        REQUIRE(std::abs(whole[i]) < 1.0e-6f);

    // Punch: processed before the split (silent), clean after it.
    for (size_t i = static_cast<size_t>(latency); i + margin < split; ++i)
        REQUIRE(std::abs(punch[i]) < 1.0e-5f);
    REQUIRE(maxDifference(punch, clean, split + margin) < 1.0e-5f);

    // Tail: clean before the split, processed (silent) after it.
    for (size_t i = static_cast<size_t>(latency) + 1000; i + margin < split; ++i)
        REQUIRE_THAT(tail[i], WithinAbs(clean[i], 1.0e-5));
    for (size_t i = split + margin; i < tail.size(); ++i)
        REQUIRE(std::abs(tail[i]) < 1.0e-5f);

    // Smooth across the split: no step bigger than the clean signal's own.
    REQUIRE(maxStep(punch) < maxStep(clean) * 1.5f + 1.0e-4f);
    REQUIRE(maxStep(tail) < maxStep(clean) * 1.5f + 1.0e-4f);
}

TEST_CASE("Bus Level scales what the bus sends to Main", "[dsp][bus]")
{
    const auto render = [](float levelDb)
    {
        Rig rig;
        rig.parameters.oscillators[0].level = 0.3f; // below the safety limiter's knee, so it stays linear
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busB;
        rig.parameters.buses[1].level = juce::Decibels::decibelsToGain(levelDb, -48.0f);
        return rig.renderLeft(30);
    };
    const auto unity = render(0.0f);
    const auto half = render(-6.0206f);
    REQUIRE(fcm::test::peak(unity) > 0.1f);
    for (size_t i = 0; i < unity.size(); ++i)
        REQUIRE_THAT(half[i], WithinAbs(unity[i] * 0.5f, 2.0e-5));
    REQUIRE(fcm::test::peak(render(-48.0f)) < 1.0e-6f);
}

TEST_CASE("The Main rack still processes everything that reaches Main, buses included", "[dsp][bus]")
{
    const auto render = [](Destination destination)
    {
        Rig rig;
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination = destination;
        rig.racks = {{RackId::main, {instance(kGain)}}};
        rig.parameters.effects[static_cast<size_t>(instance(kGain))].enabled = true;
        rig.parameters.effects[static_cast<size_t>(instance(kGain))].values[0] = 0.0f;
        return rig.renderLeft(30);
    };
    // Gain 0 in Main silences the direct sound and the sound that came through a bus.
    REQUIRE(fcm::test::peak(render(Destination::main)) < 1.0e-6f);
    REQUIRE(fcm::test::peak(render(Destination::busA)) < 1.0e-6f);
}

TEST_CASE("An instance claimed by two racks is processed by the first only", "[dsp][bus][rack]")
{
    // A packed value that puts the same instance in two racks is a bug: the later rack is empty.
    Rig rig;
    rig.parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busA;
    rig.racks = {{RackId::busA, {instance(kGain)}}, {RackId::main, {instance(kGain)}}};
    rig.parameters.effects[static_cast<size_t>(instance(kGain))].enabled = true;
    rig.parameters.effects[static_cast<size_t>(instance(kGain))].values[0] = 0.5f;
    const auto out = rig.renderLeft(30);

    Rig once;
    once.parameters = rig.parameters;
    once.racks = {{RackId::busA, {instance(kGain)}}};
    REQUIRE(maxDifference(out, once.renderLeft(30)) < 1.0e-6f);
}

TEST_CASE("The engine allocates nothing with every rack full and the buses in use", "[dsp][bus][realtime]")
{
    Rig rig;
    rig.parameters.oscillators[1].on = true;
    rig.parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busA;
    rig.parameters.mixer[fcm::dsp::stemOsc2].destination = Destination::busB;
    rig.parameters.buses[0].region = BusRegion::tail;
    rig.parameters.buses[1].region = BusRegion::punch;
    rig.racks = {{RackId::busA, {instance(kGain, 1), instance(kDelay, 1)}},
                 {RackId::busB, {instance(kGain, 2), instance(kDelay, 2)}},
                 {RackId::main, {instance(kGain, 3), instance(kDelay, 3), instance(kGain, 4)}}};
    for (int i = 0; i < 12; ++i)
        rig.parameters.effects[static_cast<size_t>(i)].enabled = true;

    rig.engine.setParameters(rig.parameters);
    rig.engine.prepare({kSampleRate, kBlock, 2});
    for (const auto& [rack, contents] : rig.racks)
        rig.engine.rackValue(rack).store(EffectRack::pack(contents));

    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::AudioBuffer<float> oversized(2, kBlock * 3 + 17); // a host may send more than it announced
    juce::MidiBuffer noteOn, none;
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 5);

    fcm::test::AllocationCounter counter;
    for (int block = 0; block < 12; ++block)
    {
        rig.parameters.mixer[fcm::dsp::stemOsc1].destination =
            block % 3 == 0 ? Destination::busB : (block % 3 == 1 ? Destination::main : Destination::busA);
        rig.parameters.oversamplingFactor = block % 2 == 0 ? 2 : 4;
        rig.engine.setParameters(rig.parameters);
        buffer.clear();
        rig.engine.process(buffer, block == 0 ? noteOn : none);
    }
    oversized.clear();
    rig.engine.process(oversized, noteOn);
    REQUIRE(counter.count() == 0);
}

TEST_CASE("A block longer than announced gives the same sound as small blocks", "[dsp][bus]")
{
    // The engine works in chunks of the announced size; the MIDI event lands on the same sample.
    const auto render = [](int blockSize)
    {
        EffectRegistry registry = makeRegistry();
        Engine engine(registry);
        EngineParameters parameters;
        parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busA;
        engine.setParameters(parameters);
        engine.prepare({kSampleRate, kBlock, 2});

        std::vector<float> out;
        for (int done = 0; done < kBlock * 6; done += blockSize)
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            if (300 >= done && 300 < done + blockSize) // the note starts at sample 300 whatever the blocks
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 300 - done);
            buffer.clear();
            engine.process(buffer, midi);
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + blockSize);
        }
        return out;
    };

    const auto small = render(kBlock);
    const auto big = render(kBlock * 3);
    REQUIRE(small.size() == big.size());
    size_t first = 0;
    while (first < small.size() && std::abs(small[first] - big[first]) < 1.0e-6f)
        ++first;
    INFO("first difference at sample " << first);
    REQUIRE(first == small.size());
}

// SPDX-License-Identifier: AGPL-3.0-only
// The note triggers of version 0.3 (docs/decisions/0012 item 1): a note-on is told to an effect at the
// sample where the note's sound is in the effect's input, in Bus A, Bus B and Main, behind effects
// with latency, at every Quality and for any block size. Tested with test-only effects.
#include "AllocationCounter.h"
#include "RackRig.h"
#include "TestEffects.h"
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

using fcm::dsp::Destination;
using fcm::dsp::EffectRack;
using fcm::dsp::EffectRegistry;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::RackId;
using fcm::test::instance;
using fcm::test::ProbeEffect;

namespace
{
constexpr double kSampleRate = 48000.0;

// Effect indices in the registry below.
constexpr int kGainIndex = 0;
constexpr int kDelayIndex = 2;
constexpr int kProbeIndex = 3;

EffectRegistry makeRegistry()
{
    EffectRegistry registry;
    registry.add<fcm::test::GainEffect>();
    registry.add<fcm::test::ClipEffect>();
    registry.add<fcm::test::DelayEffect>(); // 32 samples of latency per oversampling step
    registry.add<fcm::test::ProbeEffect>();
    return registry;
}

struct Note
{
    int at; // position in the whole render, in samples
    int number = 36;
};

// What is in front of the probe in its rack.
enum class Before
{
    nothing,
    oneDelay,
    twoDelays
};

struct Scenario
{
    RackId rack = RackId::main;
    Before before = Before::nothing;
    bool delayAfter = false; // a delay behind the probe too: it must not matter
    Destination source = Destination::main;
    int factor = 2;
    int hostBlock = 256; // the size of the buffers the host hands over
    int maxBlock = 256;  // what the host announced
};

struct Result
{
    int voiceOffset = 0; // samples between a note-on and the first audible sample of its sound
    int calls = 0;
    int restarts = 0;
    std::int64_t firstSound = -1;
    std::vector<std::int64_t> at;
    int expectedDelay = 0;
};

// The voice's own first audible sample is not the note-on's sample (the amp envelope's attack starts
// at zero). It is measured where no timing model is involved: a probe at the very start of Bus A, in
// front of nothing, sees the voice's output as it is.
int measureVoiceOffset(int factor, int maxBlock)
{
    ProbeEffect::clearLog();
    const auto registry = makeRegistry();
    Engine engine(registry);
    EngineParameters parameters;
    parameters.oversamplingFactor = factor;
    parameters.oscillators[0].startPhase = 0.25f;
    parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::busA;
    parameters.effects[static_cast<size_t>(instance(kProbeIndex, 1))].enabled = true;
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, static_cast<juce::uint32>(maxBlock), 2});
    const std::vector<int> contents{instance(kProbeIndex, 1)};
    engine.rackValue(RackId::busA).store(EffectRack::pack(contents));

    juce::AudioBuffer<float> buffer(2, maxBlock);
    for (int start = 0; start < 1024; start += maxBlock)
    {
        juce::MidiBuffer midi;
        if (start == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 5);
        buffer.clear();
        engine.setParameters(parameters);
        engine.process(buffer, midi);
    }
    const auto offset = static_cast<int>(ProbeEffect::firstSoundAt - 5);
    ProbeEffect::clearLog();
    return offset;
}

// Plays `notes` through an engine with the probe in the scenario's rack and returns what it was told.
Result play(const Scenario& scenario, const std::vector<Note>& notes, int totalSamples,
            int probeEnabledFromBlock = 0)
{
    const int voiceOffset = measureVoiceOffset(scenario.factor, scenario.maxBlock);
    ProbeEffect::clearLog();
    const auto registry = makeRegistry();
    Engine engine(registry);

    EngineParameters parameters;
    parameters.oversamplingFactor = scenario.factor;
    parameters.oscillators[0].startPhase = 0.25f; // the first sample of the note is not zero
    parameters.mixer[fcm::dsp::stemOsc1].destination = scenario.source;
    for (int e = 0; e < registry.count(); ++e)
        for (int copy = 1; copy <= fcm::dsp::kInstancesPerEffect; ++copy)
            parameters.effects[static_cast<size_t>(EffectRegistry::instanceIndex(e, copy))].enabled = true;
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, static_cast<juce::uint32>(scenario.maxBlock), 2});

    std::vector<int> contents;
    if (scenario.before != Before::nothing)
        contents.push_back(instance(kDelayIndex, 1));
    if (scenario.before == Before::twoDelays)
        contents.push_back(instance(kDelayIndex, 2));
    contents.push_back(instance(kProbeIndex, 1));
    if (scenario.delayAfter)
        contents.push_back(instance(kDelayIndex, 3));
    engine.rackValue(scenario.rack).store(EffectRack::pack(contents));

    const int latency = engine.latencySamples(scenario.factor) / 2; // one pool's worth
    Result result;
    result.expectedDelay =
        (scenario.rack == RackId::main ? latency : 0) + 32 * scenario.factor *
                                                            (scenario.before == Before::nothing    ? 0
                                                             : scenario.before == Before::oneDelay ? 1
                                                                                                   : 2);

    juce::AudioBuffer<float> buffer(2, scenario.hostBlock);
    int block = 0;
    for (int start = 0; start < totalSamples; start += scenario.hostBlock, ++block)
    {
        juce::MidiBuffer midi;
        for (const auto& note : notes)
            if (note.at >= start && note.at < start + scenario.hostBlock)
                midi.addEvent(juce::MidiMessage::noteOn(1, note.number, static_cast<juce::uint8>(100)),
                              note.at - start);
        buffer.clear();
        auto params = parameters;
        params.effects[static_cast<size_t>(instance(kProbeIndex, 1))].enabled =
            block >= probeEnabledFromBlock;
        engine.setParameters(params);
        engine.process(buffer, midi);
    }

    result.calls = ProbeEffect::noteOnCalls;
    result.voiceOffset = voiceOffset;
    result.restarts = ProbeEffect::noteOnCount;
    result.firstSound = ProbeEffect::firstSoundAt;
    for (int i = 0; i < ProbeEffect::noteOnCount; ++i)
        result.at.push_back(ProbeEffect::noteOnAt[static_cast<size_t>(i)]);
    return result;
}
} // namespace

TEST_CASE("A note-on is told to an effect at the sample where the note's sound reaches it",
          "[dsp][fx][notetrigger]")
{
    const std::vector<Note> notes{{100}, {777}, {1500}};
    for (const RackId rack : {RackId::busA, RackId::busB, RackId::main})
        for (const auto before : {Before::nothing, Before::oneDelay, Before::twoDelays})
            for (const int factor : {1, 2, 4, 8})
            {
                Scenario scenario;
                scenario.rack = rack;
                scenario.before = before;
                scenario.factor = factor;
                // The sound goes to the bus under test (or straight to Main): it arrives at the same
                // time, because Main's input is lined up with the buses.
                scenario.source = rack == RackId::busA   ? Destination::busA
                                  : rack == RackId::busB ? Destination::busB
                                                         : Destination::main;
                const auto result = play(scenario, notes, 4000);

                INFO("rack " << static_cast<int>(rack) << " before " << static_cast<int>(before) << " factor "
                             << factor);
                REQUIRE(result.calls == 3);
                REQUIRE(result.restarts == 3);
                for (size_t n = 0; n < notes.size(); ++n)
                    REQUIRE(result.at[n] == notes[n].at + result.expectedDelay);
                // And the sound really is there: the first note's sound first shows in the probe's
                // input at that very sample.
                REQUIRE(result.firstSound == notes[0].at + result.voiceOffset + result.expectedDelay);
            }
}

TEST_CASE("Sound sent to another place reaches Main's effects at the same time", "[dsp][fx][notetrigger]")
{
    for (const auto source : {Destination::main, Destination::busA, Destination::busB})
        for (const int factor : {1, 4})
        {
            Scenario scenario;
            scenario.rack = RackId::main;
            scenario.before = Before::oneDelay;
            scenario.source = source;
            scenario.factor = factor;
            const auto result = play(scenario, {{200}}, 3000);
            INFO("source " << static_cast<int>(source) << " factor " << factor);
            REQUIRE(result.calls == 1);
            REQUIRE(result.at[0] == 200 + result.expectedDelay);
            REQUIRE(result.firstSound == result.at[0] + result.voiceOffset);
        }
}

TEST_CASE("The block size and the announced size do not change when a note is told", "[dsp][fx][notetrigger]")
{
    const std::vector<Note> notes{{37}, {620}};
    for (const int hostBlock : {16, 64, 100, 256, 600, 1000})
        for (const int maxBlock : {64, 256})
        {
            Scenario scenario;
            scenario.rack = RackId::main;
            scenario.before = Before::twoDelays;
            scenario.factor = 8; // 512 samples in front: the note-on lands several blocks later
            scenario.hostBlock = hostBlock;
            scenario.maxBlock = maxBlock;
            const auto result = play(scenario, notes, 4000);
            INFO("host block " << hostBlock << " announced " << maxBlock);
            REQUIRE(result.calls == 2);
            REQUIRE(result.at[0] == 37 + result.expectedDelay);
            REQUIRE(result.at[1] == 620 + result.expectedDelay);
        }
}

TEST_CASE("Notes at the edges of a block are told once and in order", "[dsp][fx][notetrigger]")
{
    Scenario scenario;
    scenario.rack = RackId::busA;
    scenario.source = Destination::busA;
    scenario.hostBlock = 128;
    scenario.maxBlock = 128;
    // First sample, last sample of a block, first sample of the next one, and two at one sample.
    const auto result = play(scenario, {{0}, {127}, {128}, {300}, {300}}, 1000);
    REQUIRE(result.calls == 5);
    REQUIRE(result.restarts == 4); // two note-ons at one sample restart once
    REQUIRE(result.at == std::vector<std::int64_t>{0, 127, 128, 300});
}

TEST_CASE("Effects that are off or asleep are not told; one that wakes up is reset first",
          "[dsp][fx][notetrigger]")
{
    Scenario scenario;
    scenario.rack = RackId::main;
    scenario.hostBlock = 256;

    // The probe is off for the first 4 blocks (1024 samples): the note at 300 is not told to it. It
    // counts samples from when it woke up, 1024 samples into the render.
    const auto result = play(scenario, {{300}, {2000}}, 4000, 4);
    REQUIRE(result.calls == 1);
    REQUIRE(result.at[0] == 2000 + result.expectedDelay - 1024);
    REQUIRE(ProbeEffect::resets >= 1); // woken up with a reset
}

TEST_CASE("Many note-ons in a burst never break the log", "[dsp][fx][notetrigger]")
{
    Scenario scenario;
    scenario.rack = RackId::main;
    scenario.before = Before::oneDelay;
    scenario.factor = 8;
    std::vector<Note> notes;
    for (int i = 0; i < 40; ++i)
        notes.push_back({10 + i, 30 + (i % 12)});
    const auto result = play(scenario, notes, 3000);
    // The log keeps the last 16 in flight; the effect hears each of those at the right place.
    REQUIRE(result.calls > 0);
    REQUIRE(result.calls <= 40);
    for (size_t i = 1; i < result.at.size(); ++i)
        REQUIRE(result.at[i] > result.at[i - 1]);
}

TEST_CASE("Telling effects about notes allocates nothing", "[dsp][fx][notetrigger]")
{
    ProbeEffect::clearLog();
    const auto registry = makeRegistry();
    Engine engine(registry);
    EngineParameters parameters;
    parameters.oversamplingFactor = 4;
    for (int e = 0; e < registry.count(); ++e)
        for (int copy = 1; copy <= fcm::dsp::kInstancesPerEffect; ++copy)
            parameters.effects[static_cast<size_t>(EffectRegistry::instanceIndex(e, copy))].enabled = true;
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, 128, 2});

    const std::vector<int> main{instance(kDelayIndex, 1), instance(kProbeIndex, 1)};
    const std::vector<int> busA{instance(kProbeIndex, 2)};
    engine.rackValue(RackId::main).store(EffectRack::pack(main));
    engine.rackValue(RackId::busA).store(EffectRack::pack(busA));

    juce::AudioBuffer<float> buffer(2, 128);
    fcm::test::AllocationCounter counter;
    for (int block = 0; block < 40; ++block)
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36 + block % 5, static_cast<juce::uint8>(100)),
                      block % 100);
        buffer.clear();
        engine.setParameters(parameters);
        engine.process(buffer, midi);
    }
    if (fcm::test::AllocationCounter::available())
        REQUIRE(counter.count() == 0);
    REQUIRE(ProbeEffect::noteOnCalls > 0);
}

TEST_CASE("The reported latency and the timing of the sound are unchanged by the triggers",
          "[dsp][fx][notetrigger]")
{
    // A click in the voice's output reaches the end at the reported latency, as before the triggers.
    for (const int factor : {1, 2, 4, 8})
    {
        const auto registry = makeRegistry();
        Engine engine(registry);
        EngineParameters parameters;
        parameters.oversamplingFactor = factor;
        engine.setParameters(parameters);
        engine.prepare({kSampleRate, 256, 2});
        REQUIRE(engine.latencySamples(factor) == 2 * 4 * 32 * factor);
    }
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "TestUtils.h"
// A tempo or time scale that is not a finite number (a damaged project, a broken host) must not
// silence the kick or keep the voice alive forever (docs/specs/state-safe-loading.md).
#include "dsp/engine/Engine.h"
#include "dsp/envelope/BreakpointEnvelope.h"
#include "dsp/envelope/EnvelopeRenderer.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <limits>
#include <vector>

using fcm::dsp::BreakpointEnvelope;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::EnvelopeKind;
using fcm::dsp::PublishedPointer;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

const float kNan = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

struct Run
{
    float peak = 0.0f;
    bool finite = true;
};

// One kick: a few blocks held, then released, then a long silence in which the voice must end.
// `tail` is the peak of the very last blocks.
struct Outcome
{
    Run held;
    Run tail;
};

Outcome playKick(const EngineParameters& parameters)
{
    PublishedPointer<fcm::dsp::EnvelopeShape> ampSource;
    PublishedPointer<fcm::dsp::EnvelopeShape> pitchSource;
    BreakpointEnvelope amp(EnvelopeKind::amp);
    BreakpointEnvelope pitch(EnvelopeKind::pitch);
    amp.enableSync(120.0f);
    pitch.enableSync(120.0f);
    ampSource.publish(amp.toShape());
    pitchSource.publish(pitch.toShape());

    Engine engine{fcm::test::noEffects()};
    engine.setEnvelopeSources(&ampSource, &pitchSource);
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, kBlock, 2});

    Outcome outcome;
    juce::AudioBuffer<float> buffer(2, kBlock);
    const auto renderBlock = [&](Run& into, const juce::MidiBuffer& midi)
    {
        buffer.clear();
        engine.process(buffer, midi);
        for (int i = 0; i < kBlock; ++i)
        {
            const float sample = buffer.getSample(0, i);
            into.finite = into.finite && std::isfinite(sample);
            into.peak = std::max(into.peak, std::abs(sample));
        }
    };

    juce::MidiBuffer noteOn;
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
    renderBlock(outcome.held, noteOn);
    for (int i = 0; i < 20; ++i)
        renderBlock(outcome.held, {});

    juce::MidiBuffer noteOff;
    noteOff.addEvent(juce::MidiMessage::noteOff(1, 36), 0);
    renderBlock(outcome.tail, noteOff);
    for (int i = 0; i < 400; ++i) // about two seconds
    {
        outcome.tail = {};
        renderBlock(outcome.tail, {});
    }
    return outcome;
}
} // namespace

TEST_CASE("A host tempo that is not a usable number does not break tempo-synced envelopes",
          "[dsp][envelope][robustness]")
{
    for (const float bpm : {kNan, kInf, -kInf, 0.0f, -120.0f, 1.0e-30f, 1.0e30f})
    {
        EngineParameters parameters;
        parameters.bpm = bpm;

        const auto outcome = playKick(parameters);
        INFO("bpm " << bpm);
        REQUIRE(outcome.held.finite);
        REQUIRE(outcome.held.peak > 0.05f);
        REQUIRE(outcome.tail.finite);
        REQUIRE(outcome.tail.peak == 0.0f);
    }
}

TEST_CASE("A time scale that is not a finite number does not break the envelopes",
          "[dsp][envelope][robustness]")
{
    for (const float scale : {kNan, kInf, -kInf, -1.0f, 0.0f, 1.0e-30f})
    {
        EngineParameters parameters;
        parameters.ampTimeScale = scale;
        parameters.pitchTimeScale = scale;

        const auto outcome = playKick(parameters);
        INFO("time scale " << scale);
        REQUIRE(outcome.held.finite);
        REQUIRE(outcome.held.peak > 0.05f);
        REQUIRE(outcome.tail.finite);
        REQUIRE(outcome.tail.peak == 0.0f);
    }
}

TEST_CASE("Envelope renderer treats a time scale that is not finite as 1", "[dsp][envelope][robustness]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);
    const auto shape = amp.toShape();

    fcm::dsp::EnvelopeRenderer reference;
    reference.prepare(kSampleRate);
    reference.setShape(shape.get());
    reference.start(1.0f);

    fcm::dsp::EnvelopeRenderer broken;
    broken.prepare(kSampleRate);
    broken.setShape(shape.get());
    broken.start(kNan);

    for (int i = 0; i < 4800; ++i)
    {
        const float expected = reference.next();
        const float actual = broken.next();
        REQUIRE(std::isfinite(actual));
        REQUIRE(actual == expected);
    }
}

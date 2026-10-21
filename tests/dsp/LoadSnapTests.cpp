// SPDX-License-Identifier: AGPL-3.0-only
// Loading a preset puts the effects' Mix at its target at once (docs/specs/project-state.md): a note
// that follows the load within 10 ms is the loaded sound, not a blend with the effects that were off.
#include "TestEffects.h"
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using fcm::dsp::Destination;
using fcm::dsp::EffectRack;
using fcm::dsp::EffectRegistry;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::RackId;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

// The peak of the first block of a note that starts at once after a "load" that switches on an effect
// that silences the sound (gain 0, Mix 100 %). If the effect is fully on the block is silent; a ramp
// of the Mix lets the dry sound through.
float firstBlockPeak(bool requestSnap)
{
    EffectRegistry registry;
    registry.add<fcm::test::GainEffect>();
    Engine engine(registry);

    EngineParameters parameters;
    parameters.mixer[fcm::dsp::stemOsc1].destination = Destination::main;
    const size_t instance = static_cast<size_t>(EffectRegistry::instanceIndex(0, 1));
    parameters.effects[instance].enabled = false;
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, static_cast<juce::uint32>(kBlock), 2});
    const std::vector<int> contents{static_cast<int>(instance)};
    engine.rackValue(RackId::main).store(EffectRack::pack(contents));

    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < 8; ++block) // running, nothing sounding, the effect off
    {
        buffer.clear();
        juce::MidiBuffer midi;
        engine.setParameters(parameters);
        engine.process(buffer, midi);
    }

    // The load: the effect is now on and silences everything; a note starts in the same block.
    parameters.effects[instance].enabled = true;
    parameters.effects[instance].mix = 1.0f;
    parameters.effects[instance].values[0] = 0.0f;
    if (requestSnap)
        engine.requestSnap();
    engine.setParameters(parameters);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    buffer.clear();
    engine.process(buffer, midi);
    return buffer.getMagnitude(0, 0, kBlock);
}
} // namespace

TEST_CASE("After a load the effects are fully on from the first sample", "[dsp][engine][load]")
{
    // Without the request the Mix ramps over 10 ms and part of the first hit gets through untouched...
    REQUIRE(firstBlockPeak(false) > 0.001f);
    // ...with it the effect silences the whole block.
    REQUIRE(firstBlockPeak(true) == 0.0f);
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "AllocationCounter.h"
#include "RackRig.h"
#include "TestEffects.h"
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/fx/EffectRegistry.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <numbers>
#include <utility>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::EffectRack;
using fcm::dsp::EffectRegistry;
using fcm::dsp::EffectSlotParameters;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::kMaxEffects;
using fcm::dsp::kMaxInstances;
using fcm::test::ClipEffect;
using fcm::test::DelayEffect;
using fcm::test::GainEffect;
using fcm::test::instance;
using fcm::test::RackRig;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

// Effect indices in the test registry below.
constexpr int kGain = fcm::test::instance(0);
constexpr int kClip = fcm::test::instance(1);
constexpr int kDelay = fcm::test::instance(2);

EffectRegistry makeRegistry()
{
    EffectRegistry registry;
    registry.add<GainEffect>();
    registry.add<ClipEffect>();
    registry.add<DelayEffect>();
    return registry;
}

struct Rig
{
    Rig() : registry(makeRegistry()), chain(registry)
    {
        chain.prepare({kSampleRate, kBlock, 2});
        for (auto& slot : slots)
            slot = {};
    }

    // Runs `numBlocks` blocks of `input` (the same signal in both channels) and returns channel 0.
    std::vector<float> run(const std::vector<float>& input, int factor = 2)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer(2, kBlock);
        for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kBlock; ++i)
                    buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
            chain.setParameters(slots, factor);
            chain.process(buffer);
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
        }
        return out;
    }

    EffectRegistry registry;
    RackRig chain;
    std::array<EffectSlotParameters, kMaxInstances> slots;
};

std::vector<float> sineSignal(int samples, double hz, float amplitude = 1.0f)
{
    std::vector<float> out(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i)
        out[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / kSampleRate));
    return out;
}

std::vector<float> noiseSignal(int samples, float amplitude = 1.0f)
{
    juce::Random random(4242);
    std::vector<float> out(static_cast<size_t>(samples));
    for (auto& s : out)
        s = amplitude * (random.nextFloat() * 2.0f - 1.0f);
    return out;
}

float maxStep(const std::vector<float>& x)
{
    float step = 0.0f;
    for (size_t i = 1; i < x.size(); ++i)
        step = std::max(step, std::abs(x[i] - x[i - 1]));
    return step;
}
} // namespace

TEST_CASE("The registry lists effects in registration order", "[dsp][fx][registry]")
{
    const auto registry = makeRegistry();
    REQUIRE(registry.count() == 3);
    REQUIRE(registry.descriptor(0)->id == "testgain");
    REQUIRE(registry.descriptor(1)->id == "testclip");
    REQUIRE(registry.descriptor(2)->id == "testdelay");
    REQUIRE(registry.descriptor(3) == nullptr);
    REQUIRE(registry.descriptor(-1) == nullptr);
    REQUIRE(registry.indexOf("testclip") == 1);
    REQUIRE(registry.indexOf("missing") == -1);
    REQUIRE(registry.parameters(0).size() == 1);
    REQUIRE(registry.parameters(1).size() == 4);
    REQUIRE(registry.parameters(2).empty());
    REQUIRE(registry.parameters(9).empty());
    REQUIRE(registry.create(0) != nullptr);
    REQUIRE(registry.create(7) == nullptr);
}

TEST_CASE("The registry refuses duplicates, nulls and too many effects", "[dsp][fx][registry]")
{
    EffectRegistry registry;
    REQUIRE(registry.add<GainEffect>());
    REQUIRE_FALSE(registry.add<GainEffect>()); // same ID
    REQUIRE_FALSE(registry.add(nullptr));
    REQUIRE(registry.count() == 1);

    EffectRegistry full;
    const auto addAll = [&]<int... Ns>(std::integer_sequence<int, Ns...>)
    { return (full.add<fcm::test::NumberedEffect<Ns>>() && ...); };
    REQUIRE(addAll(std::make_integer_sequence<int, kMaxEffects>{}));
    REQUIRE(full.count() == kMaxEffects);
    REQUIRE_FALSE(full.add<fcm::test::NumberedEffect<16>>());
    REQUIRE(full.count() == kMaxEffects);
}

TEST_CASE("Every parameter spec of the test effects is valid", "[dsp][fx][registry]")
{
    const auto registry = makeRegistry();
    for (int i = 0; i < registry.count(); ++i)
        for (const auto& spec : registry.parameters(i))
        {
            INFO(registry.descriptor(i)->id << "." << spec.idSuffix);
            REQUIRE_FALSE(spec.idSuffix.empty());
            REQUIRE_FALSE(spec.name.empty());
            REQUIRE(spec.minimum < spec.maximum);
            REQUIRE(spec.defaultValue >= spec.minimum);
            REQUIRE(spec.defaultValue <= spec.maximum);
            if (spec.kind == fcm::dsp::ParameterKind::choice)
                REQUIRE(static_cast<float>(spec.choices.size()) == spec.maximum + 1.0f);
        }
}

TEST_CASE("With everything off the signal is the dry signal delayed by the total latency", "[dsp][fx][chain]")
{
    Rig rig;
    const auto input = noiseSignal(kBlock * 12, 0.5f);
    const auto out = rig.run(input);

    const int latency = rig.chain.latencySamples(2);
    REQUIRE(latency == 4 * 64); // only the delay effect has latency, and the pool has four copies of it
    for (size_t i = 0; i < out.size(); ++i)
        REQUIRE(out[i] ==
                (i >= static_cast<size_t>(latency) ? input[i - static_cast<size_t>(latency)] : 0.0f));
}

TEST_CASE("An effect that is on at full mix gives the wet signal, at half mix the average",
          "[dsp][fx][chain]")
{
    const auto input = noiseSignal(kBlock * 10, 0.5f);

    Rig full;
    full.slots[kGain].enabled = true;
    full.slots[kGain].values[0] = 0.5f;
    const auto wet = full.run(input);

    Rig half;
    half.slots[kGain].enabled = true;
    half.slots[kGain].values[0] = 0.5f;
    half.slots[kGain].mix = 0.5f;
    const auto mixed = half.run(input);

    // The sound always arrives 256 samples late (four copies of the delay effect at 2x), whatever is on.
    for (size_t i = 256; i < input.size(); ++i)
    {
        const float delayed = input[i - 256];
        REQUIRE_THAT(wet[i], WithinAbs(delayed * 0.5f, 1.0e-6));
        // Gain 0.5 at mix 0.5 is the average of the dry (x1) and the wet (x0.5): x0.75.
        REQUIRE_THAT(mixed[i], WithinAbs(delayed * 0.75f, 1.0e-6));
    }
}

TEST_CASE("The delay effect delays the wet signal as much as the dry one at any mix", "[dsp][fx][chain]")
{
    const auto input = noiseSignal(kBlock * 10, 0.5f);
    for (const float mix : {0.0f, 0.3f, 1.0f})
    {
        Rig rig;
        rig.slots[kDelay].enabled = true;
        rig.slots[kDelay].mix = mix;
        const auto out = rig.run(input);
        INFO("mix " << mix);
        for (size_t i = 256; i < out.size(); ++i)
            REQUIRE_THAT(out[i], WithinAbs(input[i - 256], 1.0e-6));
    }
}

TEST_CASE("The latency follows the oversampling factor and the delay lines follow it", "[dsp][fx][chain]")
{
    Rig rig;
    REQUIRE(rig.chain.latencySamples(1) == 4 * 32);
    REQUIRE(rig.chain.latencySamples(2) == 4 * 64);
    REQUIRE(rig.chain.latencySamples(4) == 4 * 128);
    REQUIRE(rig.chain.latencySamples(8) == 4 * 256);
    REQUIRE(rig.chain.latencySamples(3) == rig.chain.latencySamples(1)); // not a factor: treated as 1

    const auto input = noiseSignal(kBlock * 12, 0.5f);
    const auto out = rig.run(input, 4);
    // After the delay line settled at the new factor, the output is the input 512 samples later.
    for (size_t i = kBlock * 4; i < out.size(); ++i)
        REQUIRE(out[i] == input[i - 512]);
}

TEST_CASE("The order of the effects changes the result when they do not commute", "[dsp][fx][chain]")
{
    const auto input = sineSignal(kBlock * 8, 200.0, 1.0f);

    const auto peakFor = [&](std::vector<int> order)
    {
        Rig rig;
        rig.slots[kGain].enabled = true;
        rig.slots[kGain].values[0] = 0.5f;
        rig.slots[kClip].enabled = true;
        rig.slots[kClip].values[0] = 0.3f;
        rig.chain.setContents(order);
        const auto out = rig.run(input);
        float peak = 0.0f;
        for (size_t i = kBlock * 4; i < out.size(); ++i)
            peak = std::max(peak, std::abs(out[i]));
        return peak;
    };

    // Gain then clip: 0.5 clipped at 0.3. Clip then gain: 0.3 halved.
    REQUIRE_THAT(peakFor({kGain, kClip, kDelay}), WithinAbs(0.3, 1.0e-4));
    REQUIRE_THAT(peakFor({kClip, kGain, kDelay}), WithinAbs(0.15, 1.0e-4));
}

TEST_CASE("A packed rack that is not valid is an empty rack", "[dsp][fx][chain]")
{
    const auto input = sineSignal(kBlock * 8, 200.0, 1.0f);
    const auto render = [&](std::uint64_t packed)
    {
        Rig rig;
        rig.slots[kGain].enabled = true;
        rig.slots[kGain].values[0] = 0.5f;
        rig.slots[kClip].enabled = true;
        rig.slots[kClip].values[0] = 0.3f;
        rig.chain.rack.value().store(packed);
        return rig.run(input);
    };

    // An empty rack passes the sound through, delayed by the constant latency.
    const auto reference = render(0);
    REQUIRE(render(0xFFFFFFFFFFFFFFFFull) == reference); // a count and indices that cannot be
    REQUIRE(render(EffectRack::pack(std::vector<int>{kGain, kGain, kDelay})) == reference); // a repeat
    REQUIRE(render(EffectRack::pack(std::vector<int>{kGain, kClip, 40})) == reference);     // not in the pool
    REQUIRE(render(EffectRack::pack(std::vector<int>{kGain, kClip, kDelay})) !=
            reference); // a good one differs
}

TEST_CASE("Switching an effect on and off crossfades, and a reorder does not jump", "[dsp][fx][chain]")
{
    constexpr int kBlocks = 40;
    const auto input = sineSignal(kBlock * kBlocks, 100.0, 0.5f);
    const float inputStep = maxStep(input);

    Rig rig;
    rig.slots[kGain].values[0] = 2.0f;

    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < kBlocks; ++block)
    {
        // On at block 5, off at block 20; the clip is moved at block 30 (it is off, so nothing changes).
        rig.slots[kGain].enabled = block >= 5 && block < 20;
        if (block == 30)
            rig.chain.setContents(std::vector<int>{kClip, kGain, kDelay});

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[static_cast<size_t>(block * kBlock + i)]);
        rig.chain.setParameters(rig.slots, 2);
        rig.chain.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }

    // The signal can grow to twice its size over 10 ms: the sample-to-sample step stays small.
    REQUIRE(maxStep(out) < inputStep * 2.5f);
}

TEST_CASE("A slot that wakes up after being off is reset, an active one is not", "[dsp][fx][chain]")
{
    Rig rig;
    const auto input = sineSignal(kBlock * 4, 100.0, 0.5f);

    GainEffect::resets = 0;
    rig.slots[kGain].enabled = true;
    rig.run(input);
    REQUIRE(GainEffect::resets == 1); // woke up once

    rig.run(input);
    REQUIRE(GainEffect::resets == 1); // still active: not reset again

    rig.slots[kGain].enabled = false;
    rig.run(sineSignal(kBlock * 6, 100.0, 0.5f)); // long enough to fade out and fall asleep
    rig.slots[kGain].enabled = true;
    rig.run(input);
    REQUIRE(GainEffect::resets == 2);
}

TEST_CASE("The values reach the effect in spec order and bad numbers keep the last good one",
          "[dsp][fx][chain]")
{
    Rig rig;
    const auto input = sineSignal(kBlock * 2, 100.0, 0.5f);

    rig.slots[kGain].enabled = true;
    rig.slots[kGain].values[0] = 1.5f;
    rig.run(input);
    REQUIRE(GainEffect::lastCount == 1);
    REQUIRE(GainEffect::lastValues[0] == 1.5f);

    rig.slots[kGain].values[0] = std::numeric_limits<float>::quiet_NaN();
    rig.run(input);
    REQUIRE(GainEffect::lastValues[0] == 1.5f);
}

TEST_CASE("The chain output is finite for silence, noise and a sweep with every effect on",
          "[dsp][fx][chain]")
{
    std::vector<float> sweep(kBlock * 32);
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        const double t = static_cast<double>(i) / kSampleRate;
        sweep[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi * (20.0 * t + 8000.0 * t * t)));
    }

    for (const auto& input : {std::vector<float>(kBlock * 8, 0.0f), noiseSignal(kBlock * 8, 1.0f), sweep})
    {
        Rig rig;
        for (auto& slot : rig.slots)
            slot.enabled = true;
        rig.slots[kGain].values[0] = 4.0f;
        for (float s : rig.run(input))
            REQUIRE(std::isfinite(s));
    }
}

TEST_CASE("Processing allocates nothing", "[dsp][fx][chain][realtime]")
{
    Rig rig;
    for (auto& slot : rig.slots)
        slot.enabled = true;
    const auto input = noiseSignal(kBlock * 4, 0.5f);
    juce::AudioBuffer<float> buffer(2, kBlock);

    fcm::test::AllocationCounter counter;
    for (int block = 0; block < 4; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[static_cast<size_t>(block * kBlock + i)]);
        rig.chain.setParameters(rig.slots, block % 2 == 0 ? 2 : 4);
        rig.chain.process(buffer);
    }

    REQUIRE(counter.count() == 0);
}

TEST_CASE("The engine runs the chain on both channels, before the gain and the limiter", "[dsp][fx][engine]")
{
    const auto registry = makeRegistry();
    Engine engine(registry);

    EngineParameters parameters;
    parameters.effects[kGain].enabled = true;
    parameters.effects[kGain].values[0] = 0.0f; // silences everything
    engine.setParameters(parameters);
    engine.prepare({kSampleRate, kBlock, 2});
    engine.rackValue(fcm::dsp::RackId::main).store(EffectRack::pack(std::vector<int>{kGain, kClip, kDelay}));

    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int block = 0; block < 20; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
        buffer.clear();
        engine.process(buffer, midi);
        if (block > 4) // after the 10 ms fade of the mix
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kBlock; ++i)
                    REQUIRE(buffer.getSample(c, i) == 0.0f);
    }
}

TEST_CASE("With no effects the engine output is the same on both channels and unchanged", "[dsp][fx][engine]")
{
    const EffectRegistry none;
    Engine engine(none);
    engine.setParameters(EngineParameters{});
    engine.prepare({kSampleRate, kBlock, 2});

    juce::AudioBuffer<float> buffer(2, kBlock);
    float peak = 0.0f;
    for (int block = 0; block < 10; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
        buffer.clear();
        engine.process(buffer, midi);
        for (int i = 0; i < kBlock; ++i)
        {
            REQUIRE(buffer.getSample(0, i) == buffer.getSample(1, i));
            peak = std::max(peak, std::abs(buffer.getSample(0, i)));
        }
    }
    REQUIRE(peak > 0.1f);
    REQUIRE(engine.latencySamples(2) == 0);
}

TEST_CASE("With the real effects registered but off, the engine is the same sound delayed by the latency",
          "[dsp][fx][engine]")
{
    const EffectRegistry none;
    const auto renderWith =
        [&](const EffectRegistry& registry, std::vector<float>& left, std::vector<float>& right)
    {
        Engine engine(registry);
        engine.setParameters(EngineParameters{});
        engine.prepare({kSampleRate, kBlock, 2});
        std::vector<int> contents;
        for (int effect = 0; effect < registry.count(); ++effect)
            contents.push_back(fcm::test::instance(effect));
        engine.rackValue(fcm::dsp::RackId::main).store(EffectRack::pack(contents));

        juce::AudioBuffer<float> buffer(2, kBlock);
        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
            buffer.clear();
            engine.process(buffer, midi);
            left.insert(left.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
            right.insert(right.end(), buffer.getReadPointer(1), buffer.getReadPointer(1) + kBlock);
        }
        return engine.latencySamples(2);
    };

    std::vector<float> plainLeft, plainRight, fxLeft, fxRight;
    REQUIRE(renderWith(none, plainLeft, plainRight) == 0);
    const int latency = renderWith(fcm::dsp::defaultEffectRegistry(), fxLeft, fxRight);
    REQUIRE(latency >= 0);

    for (size_t i = static_cast<size_t>(latency); i < plainLeft.size(); ++i)
    {
        REQUIRE(fxLeft[i] == plainLeft[i - static_cast<size_t>(latency)]);
        REQUIRE(fxRight[i] == plainRight[i - static_cast<size_t>(latency)]);
    }
}

TEST_CASE("The delay is the same whatever the rack holds", "[dsp][fx][rack]")
{
    const auto input = noiseSignal(kBlock * 10, 0.5f);

    const auto render = [&](std::vector<int> contents)
    {
        Rig rig;
        rig.chain.setContents(contents);
        for (auto& slot : rig.slots)
            slot.enabled = true;
        rig.slots[kGain].values[0] = 1.0f;
        rig.slots[kClip].values[0] = 1.0f; // never clips the 0.5 noise
        return rig.run(input);
    };

    // Unity effects only: the output is the input delayed by the pool latency (256 at 2x) however
    // many effects, and which, are in the rack.
    for (const auto& contents :
         {std::vector<int>{}, std::vector<int>{kGain}, std::vector<int>{kDelay},
          std::vector<int>{kClip, kDelay, kGain}, std::vector<int>{kDelay, fcm::test::instance(2, 3), kGain}})
    {
        INFO("effects in the rack: " << contents.size());
        const auto out = render(contents);
        for (size_t i = 256; i < out.size(); ++i)
            REQUIRE_THAT(out[i], WithinAbs(input[i - 256], 1.0e-6));
    }
}

TEST_CASE("Copies of an effect are separate: each has its own settings", "[dsp][fx][rack]")
{
    const auto input = sineSignal(kBlock * 6, 200.0, 0.5f);
    Rig rig;
    rig.chain.setContents(std::vector<int>{kGain, instance(0, 2), instance(0, 3)});
    for (int copy = 1; copy <= 3; ++copy)
        rig.slots[static_cast<size_t>(instance(0, copy))].enabled = true;
    rig.slots[static_cast<size_t>(instance(0, 1))].values[0] = 2.0f;
    rig.slots[static_cast<size_t>(instance(0, 2))].values[0] = 0.5f;
    rig.slots[static_cast<size_t>(instance(0, 3))].values[0] = 0.5f;

    const auto out = rig.run(input);
    float peak = 0.0f;
    for (size_t i = kBlock * 4; i < out.size(); ++i)
        peak = std::max(peak, std::abs(out[i]));
    REQUIRE_THAT(peak, WithinAbs(0.5 * 2.0 * 0.5 * 0.5, 1.0e-3));
}

TEST_CASE("An effect that joins the rack starts from a reset", "[dsp][fx][rack]")
{
    Rig rig;
    const auto input = sineSignal(kBlock * 4, 100.0, 0.5f);
    rig.chain.setContents(std::vector<int>{});
    rig.slots[kGain].enabled = true;

    GainEffect::resets = 0;
    rig.run(input);
    REQUIRE(GainEffect::resets == 0); // not in a rack: untouched

    rig.chain.setContents(std::vector<int>{kGain});
    rig.run(input);
    REQUIRE(GainEffect::resets == 1);
}

TEST_CASE("Instance IDs follow the pool and read back unambiguously", "[dsp][fx][registry]")
{
    const auto registry = makeRegistry();
    REQUIRE(registry.instanceCount() == 3 * fcm::dsp::kInstancesPerEffect);
    REQUIRE(registry.instanceId(instance(1, 1)) == "testclip");
    REQUIRE(registry.instanceId(instance(1, 2)) == "testclip2");
    REQUIRE(registry.instanceId(instance(1, 4)) == "testclip4");
    REQUIRE(registry.instanceId(registry.instanceCount()).empty());
    REQUIRE(registry.instanceId(-1).empty());

    for (int i = 0; i < registry.instanceCount(); ++i)
        REQUIRE(registry.indexOfInstance(registry.instanceId(i)) == i);

    REQUIRE(registry.indexOfInstance("testclip5") == -1);
    REQUIRE(registry.indexOfInstance("testclip1") == -1);
    REQUIRE(registry.indexOfInstance("testclip22") == -1);
    REQUIRE(registry.indexOfInstance("") == -1);
    REQUIRE(registry.indexOfInstance("testcli") == -1);
}

TEST_CASE("The registry refuses an ID that would read as a copy of another", "[dsp][fx][registry]")
{
    EffectRegistry registry;
    REQUIRE(registry.add<fcm::test::NumberedEffect<0>>()); // "n00"

    // "n002" would be copy 2 of "n00", so a type with that ID cannot exist.
    struct Copy2 : fcm::dsp::Effect
    {
        const fcm::dsp::EffectDescriptor& descriptor() const override
        {
            static const fcm::dsp::EffectDescriptor d{"n002", "Clash", 1};
            return d;
        }
        std::span<const fcm::dsp::ParameterSpec> parameters() const override { return {}; }
        void prepare(const fcm::dsp::EffectSpec&) override {}
        void reset() noexcept override {}
        void setParameters(std::span<const float>) noexcept override {}
        void process(juce::AudioBuffer<float>&) noexcept override {}
    };
    REQUIRE_FALSE(registry.add<Copy2>());
}

TEST_CASE("The packed rack value round trips and rejects bad ones", "[dsp][fx][rack]")
{
    std::array<int, fcm::dsp::kMaxRackEffects> out{};
    const std::vector<int> contents{12, 0, 63, 5, 7, 8, 9, 10};
    const auto packed = EffectRack::pack(contents);
    REQUIRE(EffectRack::unpack(packed, 64, out) == 8);
    for (size_t i = 0; i < contents.size(); ++i)
        REQUIRE(out[i] == contents[i]);

    REQUIRE(EffectRack::unpack(0, 64, out) == 0);
    REQUIRE(EffectRack::unpack(packed, 40, out) == 0); // 63 is not in a pool of 40
    REQUIRE(EffectRack::unpack(9, 64, out) == 0);      // a count above the limit
    REQUIRE(EffectRack::memberMask(EffectRack::pack(std::vector<int>{1, 3}), 64) == 0b1010u);
    REQUIRE(EffectRack::memberMask(EffectRack::pack(std::vector<int>{1, 1}), 64) == 0u);
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "TestEffects.h"
#include "TestUtils.h"
#include "dsp/engine/Engine.h"
#include "dsp/fx/EffectChain.h"
#include "dsp/fx/EffectRegistry.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdlib>
#include <juce_dsp/juce_dsp.h>
#include <new>
#include <numbers>
#include <utility>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::EffectChain;
using fcm::dsp::EffectRegistry;
using fcm::dsp::EffectSlotParameters;
using fcm::dsp::Engine;
using fcm::dsp::EngineParameters;
using fcm::dsp::kMaxEffects;
using fcm::test::ClipEffect;
using fcm::test::DelayEffect;
using fcm::test::GainEffect;

// Counts heap allocations made through operator new while armed, to show process() makes none.
// Under the address sanitizer replacing operator new clashes with its own, so the count is only
// available in ordinary builds (the sanitizer run checks memory safety instead).
#if defined(__SANITIZE_ADDRESS__)
#define FCM_COUNT_ALLOCATIONS 0
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FCM_COUNT_ALLOCATIONS 0
#endif
#endif
#ifndef FCM_COUNT_ALLOCATIONS
#define FCM_COUNT_ALLOCATIONS 1
#endif

namespace
{
std::atomic<bool> gCounting{false};
std::atomic<int> gAllocations{0};
} // namespace

#if FCM_COUNT_ALLOCATIONS
void* operator new(std::size_t size)
{
    if (gCounting)
        ++gAllocations;
    if (void* pointer = std::malloc(size > 0 ? size : 1))
        return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return operator new(size);
}
void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

// Effect indices in the test registry below.
constexpr int kGain = 0;
constexpr int kClip = 1;
constexpr int kDelay = 2;

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
    EffectChain chain;
    std::array<EffectSlotParameters, kMaxEffects> slots;
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
    REQUIRE(latency == 64); // only the delay effect has latency
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

    // The delay slot always delays by 64 samples (its latency at 2x), whatever is on.
    for (size_t i = 64; i < input.size(); ++i)
    {
        const float delayed = input[i - 64];
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
        for (size_t i = 64; i < out.size(); ++i)
            REQUIRE_THAT(out[i], WithinAbs(input[i - 64], 1.0e-6));
    }
}

TEST_CASE("The latency follows the oversampling factor and the delay lines follow it", "[dsp][fx][chain]")
{
    Rig rig;
    REQUIRE(rig.chain.latencySamples(1) == 32);
    REQUIRE(rig.chain.latencySamples(2) == 64);
    REQUIRE(rig.chain.latencySamples(4) == 128);
    REQUIRE(rig.chain.latencySamples(8) == 256);
    REQUIRE(rig.chain.latencySamples(3) == rig.chain.latencySamples(1)); // not a factor: treated as 1

    const auto input = noiseSignal(kBlock * 12, 0.5f);
    const auto out = rig.run(input, 4);
    // After the delay line settled at the new factor, the output is the input 128 samples later.
    for (size_t i = kBlock * 2; i < out.size(); ++i)
        REQUIRE(out[i] == input[i - 128]);
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
        rig.chain.order().store(EffectChain::packOrder(order));
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

TEST_CASE("An order that is not a permutation plays in registration order", "[dsp][fx][chain]")
{
    const auto input = sineSignal(kBlock * 8, 200.0, 1.0f);
    const auto render = [&](std::uint64_t packed)
    {
        Rig rig;
        rig.slots[kGain].enabled = true;
        rig.slots[kGain].values[0] = 0.5f;
        rig.slots[kClip].enabled = true;
        rig.slots[kClip].values[0] = 0.3f;
        rig.chain.order().store(packed);
        return rig.run(input);
    };

    const auto reference = render(EffectChain::registrationOrder(3));
    REQUIRE(render(0) == reference);                                                 // all zeros
    REQUIRE(render(0xFFFFFFFFFFFFFFFFull) == reference);                             // out of range
    REQUIRE(render(EffectChain::packOrder(std::vector<int>{0, 0, 2})) == reference); // a repeat
    REQUIRE(render(EffectChain::packOrder(std::vector<int>{0, 1, 5})) == reference); // an unknown index
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
            rig.chain.order().store(EffectChain::packOrder(std::vector<int>{kClip, kGain, kDelay}));

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

    gAllocations = 0;
    gCounting = true;
    for (int block = 0; block < 4; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[static_cast<size_t>(block * kBlock + i)]);
        rig.chain.setParameters(rig.slots, block % 2 == 0 ? 2 : 4);
        rig.chain.process(buffer);
    }
    gCounting = false;

#if FCM_COUNT_ALLOCATIONS
    REQUIRE(gAllocations.load() == 0);
#endif
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
    Engine engine; // the default registry has no effect yet
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

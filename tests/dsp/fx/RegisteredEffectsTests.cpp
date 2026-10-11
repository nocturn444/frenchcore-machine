// SPDX-License-Identifier: AGPL-3.0-only
// The tests every registered effect must pass (docs/ADDING_AN_EFFECT.md step 4). They pick up a new
// effect from the default registry automatically.
#include "../AllocationCounter.h"
#include "dsp/fx/EffectRegistry.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>
#include <set>
#include <string>
#include <vector>

using fcm::dsp::Effect;
using fcm::dsp::EffectSpec;

namespace
{
constexpr int kBlock = 256;
constexpr int kFactors[] = {1, 2, 4, 8};

std::vector<float> makeSignal(int kind, int numSamples, double sampleRate)
{
    std::vector<float> out(static_cast<size_t>(numSamples), 0.0f);
    juce::Random random(99);
    for (int i = 0; i < numSamples; ++i)
    {
        const double t = i / sampleRate;
        switch (kind)
        {
        case 0:
            break; // silence
        case 1:
            out[static_cast<size_t>(i)] = random.nextFloat() * 2.0f - 1.0f;
            break; // full-scale noise
        default:   // full-scale sine sweep, 20 Hz upwards
            out[static_cast<size_t>(i)] =
                static_cast<float>(std::sin(2.0 * std::numbers::pi * (20.0 * t + 6000.0 * t * t)));
        }
    }
    return out;
}

// All parameter values of an effect at their defaults, at their minimum or at their maximum.
std::vector<float> valuesAt(const Effect& effect, int which)
{
    std::vector<float> values;
    for (const auto& spec : effect.parameters())
        values.push_back(which == 0 ? spec.defaultValue : which == 1 ? spec.minimum : spec.maximum);
    return values;
}

std::vector<float> run(Effect& effect, const std::vector<float>& input, int factor)
{
    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (size_t start = 0; start + kBlock <= input.size(); start += kBlock)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample(c, i, input[start + static_cast<size_t>(i)]);
        effect.process(buffer);
        out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock);
    }
    return out;
}
} // namespace

TEST_CASE("Registered effects have valid, unique identities and parameter specs", "[dsp][fx][registered]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    std::set<std::string> ids;

    for (int index = 0; index < registry.count(); ++index)
    {
        const auto& descriptor = *registry.descriptor(index);
        INFO("effect " << descriptor.id);
        REQUIRE_FALSE(descriptor.id.empty());
        REQUIRE_FALSE(descriptor.displayName.empty());
        REQUIRE(descriptor.version >= 1);
        REQUIRE(ids.insert(std::string(descriptor.id)).second);
        for (char c : descriptor.id)
            REQUIRE(((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')));

        std::set<std::string> suffixes;
        for (const auto& spec : registry.parameters(index))
        {
            INFO("parameter " << spec.idSuffix);
            REQUIRE_FALSE(spec.idSuffix.empty());
            REQUIRE(suffixes.insert(std::string(spec.idSuffix)).second);
            REQUIRE(spec.idSuffix != "enabled"); // the chain adds these two
            REQUIRE(spec.idSuffix != "mix");
            REQUIRE_FALSE(spec.name.empty());
            REQUIRE(spec.minimum < spec.maximum);
            REQUIRE(spec.defaultValue >= spec.minimum);
            REQUIRE(spec.defaultValue <= spec.maximum);
            if (spec.kind == fcm::dsp::ParameterKind::choice)
            {
                REQUIRE(static_cast<float>(spec.choices.size()) == spec.maximum + 1.0f);
                REQUIRE(spec.minimum == 0.0f);
            }
            if (spec.kind == fcm::dsp::ParameterKind::toggle)
            {
                REQUIRE(spec.minimum == 0.0f);
                REQUIRE(spec.maximum == 1.0f);
            }
        }
    }
}

TEST_CASE("Every registered effect gives finite output for silence, noise and a sweep",
          "[dsp][fx][registered]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    for (int index = 0; index < registry.count(); ++index)
        for (const double sampleRate : {44100.0, 48000.0, 96000.0})
            for (const int factor : kFactors)
                for (const int which : {0, 1, 2}) // defaults, minimums, maximums
                    for (int kind = 0; kind < 3; ++kind)
                    {
                        auto effect = registry.create(index);
                        effect->prepare({sampleRate, kBlock, 2});
                        effect->setOversampling(factor);
                        effect->setParameters(valuesAt(*effect, which));

                        const auto out = run(*effect, makeSignal(kind, kBlock * 24, sampleRate), factor);
                        INFO(effect->descriptor().id << " rate " << sampleRate << " factor " << factor
                                                     << " values " << which << " signal " << kind);
                        for (float s : out)
                            REQUIRE(std::isfinite(s));
                    }
}

TEST_CASE("Every registered effect makes sense with one channel and with damaged input",
          "[dsp][fx][registered]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    for (int index = 0; index < registry.count(); ++index)
    {
        auto effect = registry.create(index);
        effect->prepare({48000.0, kBlock, 2});
        effect->setOversampling(2);
        effect->setParameters(valuesAt(*effect, 0));

        juce::AudioBuffer<float> mono(1, kBlock);
        for (int i = 0; i < kBlock; ++i)
            mono.setSample(0, i,
                           i % 7 == 0 ? std::numeric_limits<float>::quiet_NaN()
                                      : (i % 5 == 0 ? std::numeric_limits<float>::infinity() : 0.5f));
        effect->process(mono);
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(std::isfinite(mono.getSample(0, i)));
    }
}

TEST_CASE("Registered effects allocate nothing while processing", "[dsp][fx][registered][realtime]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    for (int index = 0; index < registry.count(); ++index)
        for (const int factor : kFactors)
        {
            auto effect = registry.create(index);
            effect->prepare({48000.0, kBlock, 2});
            effect->setOversampling(factor);
            effect->setParameters(valuesAt(*effect, 2));

            const auto input = makeSignal(1, kBlock * 4, 48000.0);
            juce::AudioBuffer<float> buffer(2, kBlock);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kBlock; ++i)
                    buffer.setSample(c, i, input[static_cast<size_t>(i)]);

            const std::array<std::vector<float>, 3> sets{valuesAt(*effect, 0), valuesAt(*effect, 1),
                                                         valuesAt(*effect, 2)};
            fcm::test::AllocationCounter counter;
            for (int block = 0; block < 4; ++block)
            {
                effect->setParameters(sets[static_cast<size_t>(block % 3)]);
                effect->reset();
                effect->process(buffer);
            }
            // Read before INFO: building its message allocates.
            const int allocations = counter.count();
            INFO(effect->descriptor().id << " factor " << factor);
            REQUIRE(allocations == 0);
        }
}

TEST_CASE("reset() makes the output of every registered effect deterministic", "[dsp][fx][registered]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    for (int index = 0; index < registry.count(); ++index)
        for (const int factor : kFactors)
        {
            auto effect = registry.create(index);
            effect->prepare({48000.0, kBlock, 2});
            effect->setOversampling(factor);
            effect->setParameters(valuesAt(*effect, 0));

            const auto input = makeSignal(2, kBlock * 8, 48000.0);
            const auto first = run(*effect, input, factor);

            effect->reset();
            effect->setParameters(valuesAt(*effect, 0)); // parameter smoothers restart too
            const auto second = run(*effect, input, factor);

            INFO(effect->descriptor().id << " factor " << factor);
            REQUIRE(first == second);
        }
}

TEST_CASE("The latency of every registered effect is constant, non-negative and zero at 1x",
          "[dsp][fx][registered]")
{
    const auto& registry = fcm::dsp::defaultEffectRegistry();
    for (int index = 0; index < registry.count(); ++index)
    {
        auto effect = registry.create(index);
        effect->prepare({48000.0, kBlock, 2});
        for (const int factor : kFactors)
        {
            const int first = effect->latencySamples(factor);
            INFO(effect->descriptor().id << " factor " << factor);
            REQUIRE(first >= 0);
            effect->setParameters(valuesAt(*effect, 2));
            REQUIRE(effect->latencySamples(factor) == first); // never depends on the parameters
        }
        REQUIRE(effect->latencySamples(1) == 0);
    }
}

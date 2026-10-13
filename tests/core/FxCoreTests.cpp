// SPDX-License-Identifier: AGPL-3.0-only
#include "../dsp/TestEffects.h"
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/FxOrderState.h"
#include "core/ParameterIds.h"
#include "dsp/fx/EffectChain.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>

using Catch::Matchers::WithinAbs;
using fcm::core::FxOrderState;
using fcm::dsp::EffectRegistry;
using fcm::test::ClipEffect;
using fcm::test::DelayEffect;
using fcm::test::GainEffect;

namespace
{
EffectRegistry makeRegistry()
{
    EffectRegistry registry;
    registry.add<GainEffect>();
    registry.add<ClipEffect>();
    registry.add<DelayEffect>();
    return registry;
}

const std::vector<std::string> kRegistration{"testgain", "testclip", "testdelay"};

void setValue(juce::AudioProcessorValueTreeState& state, const std::string& id, float value)
{
    auto* parameter = state.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
} // namespace

TEST_CASE("The FX order starts in registration order and publishes the packed value", "[core][fx][order]")
{
    const auto registry = makeRegistry();
    std::atomic<std::uint64_t> packed{0};
    FxOrderState order(registry, packed);

    REQUIRE(order.ids() == kRegistration);
    REQUIRE(packed.load() == fcm::dsp::EffectChain::registrationOrder(3));

    order.set({"testdelay", "testgain", "testclip"});
    REQUIRE(packed.load() == fcm::dsp::EffectChain::packOrder(std::vector<int>{2, 0, 1}));
}

TEST_CASE("Setting the order drops unknown and repeated IDs and appends missing effects", "[core][fx][order]")
{
    const auto registry = makeRegistry();
    std::atomic<std::uint64_t> packed{0};
    FxOrderState order(registry, packed);

    order.set({"testclip", "unknown", "testclip", "testdelay"});
    REQUIRE(order.ids() == std::vector<std::string>{"testclip", "testdelay", "testgain"});

    order.set({});
    REQUIRE(order.ids() == kRegistration);
}

TEST_CASE("Moving an effect reorders the others around it", "[core][fx][order]")
{
    const auto registry = makeRegistry();
    std::atomic<std::uint64_t> packed{0};
    FxOrderState order(registry, packed);

    order.move(0, 2);
    REQUIRE(order.ids() == std::vector<std::string>{"testclip", "testdelay", "testgain"});
    order.move(2, 0);
    REQUIRE(order.ids() == kRegistration);
    order.move(-5, 99); // clamped: the first goes to the end
    REQUIRE(order.ids() == std::vector<std::string>{"testclip", "testdelay", "testgain"});
    order.move(1, 1);
    REQUIRE(order.ids() == std::vector<std::string>{"testclip", "testdelay", "testgain"});
}

TEST_CASE("The FX order round-trips through the saved node", "[core][fx][order]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto registry = makeRegistry();
    std::atomic<std::uint64_t> packedA{0};
    std::atomic<std::uint64_t> packedB{0};
    FxOrderState saved(registry, packedA);
    saved.set({"testdelay", "testclip", "testgain"});

    const auto node = saved.toValueTree();
    REQUIRE(node.getType().toString() == "FxChain");
    REQUIRE(node["order"].toString() == "testdelay,testclip,testgain");

    FxOrderState restored(registry, packedB);
    REQUIRE(restored.restoreFrom(node));
    REQUIRE(restored.ids() == saved.ids());
    REQUIRE(packedB.load() == packedA.load());
}

TEST_CASE("A damaged saved FX order never fails the load", "[core][fx][order]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto registry = makeRegistry();
    std::atomic<std::uint64_t> packed{0};
    FxOrderState order(registry, packed);

    const auto nodeWith = [](const juce::String& text)
    {
        juce::ValueTree node("FxChain");
        node.setProperty("order", text, nullptr);
        return node;
    };

    // Unknown IDs (an older or newer version) are dropped, the missing ones appended.
    REQUIRE(order.restoreFrom(nodeWith("neweffect, testclip ,testclip,testdelay")));
    REQUIRE(order.ids() == std::vector<std::string>{"testclip", "testdelay", "testgain"});

    // Unusable nodes give the registration order.
    for (const auto& bad : {nodeWith(""), nodeWith(juce::String::repeatedString("x,", 5000)),
                            juce::ValueTree("Other"), juce::ValueTree()})
    {
        order.set({"testdelay", "testclip", "testgain"});
        REQUIRE_FALSE(order.restoreFrom(bad));
        REQUIRE(order.ids() == kRegistration);
    }

    // Odd text is harmless.
    REQUIRE(order.restoreFrom(nodeWith(",,, ,\n\t,testdelay")));
    REQUIRE(order.ids().front() == "testdelay");
}

TEST_CASE("Every registered effect gets its on switch, its mix and its own parameters", "[core][fx][layout]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto registry = makeRegistry();
    fcm::test::TestProcessor processor(registry);

    for (const char* id : {"testgain", "testclip", "testdelay"})
    {
        auto* enabled = dynamic_cast<juce::AudioParameterBool*>(
            processor.state.getParameter(fcm::core::ParameterIds::effect(id, "enabled")));
        REQUIRE(enabled != nullptr);
        REQUIRE_FALSE(enabled->get()); // effects start off

        auto* mix = dynamic_cast<juce::AudioParameterFloat*>(
            processor.state.getParameter(fcm::core::ParameterIds::effect(id, "mix")));
        REQUIRE(mix != nullptr);
        REQUIRE_THAT(mix->get(), WithinAbs(100.0, 1.0e-4));
        REQUIRE_THAT(mix->getNormalisableRange().end, WithinAbs(100.0, 1.0e-6));
    }

    // testgain.gain: a number with its range and default.
    auto* gain = dynamic_cast<juce::AudioParameterFloat*>(processor.state.getParameter("testgain.gain"));
    REQUIRE(gain != nullptr);
    REQUIRE_THAT(gain->getNormalisableRange().start, WithinAbs(0.0, 1.0e-6));
    REQUIRE_THAT(gain->getNormalisableRange().end, WithinAbs(4.0, 1.0e-6));
    REQUIRE_THAT(gain->get(), WithinAbs(1.0, 1.0e-4));

    // Every kind of parameter is built from its spec.
    REQUIRE(dynamic_cast<juce::AudioParameterFloat*>(processor.state.getParameter("testclip.threshold")) !=
            nullptr);
    auto* mode = dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter("testclip.mode"));
    REQUIRE(mode != nullptr);
    REQUIRE(mode->choices.size() == 2);
    REQUIRE(mode->choices[1].toStdString() == "Soft");
    REQUIRE(mode->getIndex() == 1);
    auto* flag = dynamic_cast<juce::AudioParameterBool*>(processor.state.getParameter("testclip.flag"));
    REQUIRE(flag != nullptr);
    REQUIRE(flag->get());
    auto* steps = dynamic_cast<juce::AudioParameterInt*>(processor.state.getParameter("testclip.steps"));
    REQUIRE(steps != nullptr);
    REQUIRE(steps->getRange().getStart() == -3);
    REQUIRE(steps->getRange().getEnd() == 3);
    REQUIRE(steps->get() == 2);

    // The global Quality choice.
    auto* quality = dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter("global.quality"));
    REQUIRE(quality != nullptr);
    REQUIRE(quality->choices.size() == 4);
    REQUIRE(quality->choices[0].toStdString() == "1x");
    REQUIRE(quality->choices[3].toStdString() == "8x");
    REQUIRE(quality->getIndex() == 1); // 2x
}

TEST_CASE("The reader hands the effects' values to the engine in spec order", "[core][fx][reader]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto registry = makeRegistry();
    fcm::test::TestProcessor processor(registry);
    fcm::core::EngineParameterReader reader(processor.state, registry);

    // Defaults.
    auto read = reader.read();
    REQUIRE_FALSE(read.effects[0].enabled);
    REQUIRE_THAT(read.effects[0].mix, WithinAbs(1.0, 1.0e-4));
    REQUIRE_THAT(read.effects[0].values[0], WithinAbs(1.0, 1.0e-4)); // gain
    REQUIRE_THAT(read.effects[1].values[0], WithinAbs(1.0, 1.0e-4)); // threshold
    REQUIRE_THAT(read.effects[1].values[1], WithinAbs(1.0, 1.0e-3)); // mode: Soft
    REQUIRE_THAT(read.effects[1].values[2], WithinAbs(1.0, 1.0e-3)); // flag
    REQUIRE_THAT(read.effects[1].values[3], WithinAbs(2.0, 1.0e-3)); // steps
    REQUIRE(read.oversamplingFactor == 2);

    setValue(processor.state, "testgain.enabled", 1.0f);
    setValue(processor.state, "testgain.mix", 40.0f);
    setValue(processor.state, "testgain.gain", 2.5f);
    setValue(processor.state, "testclip.mode", 0.0f);
    setValue(processor.state, "testclip.steps", -2.0f);
    setValue(processor.state, "global.quality", 3.0f);
    read = reader.read();
    REQUIRE(read.effects[0].enabled);
    REQUIRE_THAT(read.effects[0].mix, WithinAbs(0.4, 1.0e-3));
    REQUIRE_THAT(read.effects[0].values[0], WithinAbs(2.5, 1.0e-3));
    REQUIRE_THAT(read.effects[1].values[1], WithinAbs(0.0, 1.0e-3));
    REQUIRE_THAT(read.effects[1].values[3], WithinAbs(-2.0, 1.0e-3));
    REQUIRE(read.oversamplingFactor == 8);
}

TEST_CASE("Values that are not finite numbers fall back to the effect's defaults", "[core][fx][reader]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto registry = makeRegistry();
    fcm::test::TestProcessor processor(registry);
    fcm::core::EngineParameterReader reader(processor.state, registry);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const char* id : {"testgain.gain", "testgain.mix", "testgain.enabled", "global.quality",
                           "testclip.threshold", "testclip.mode", "testclip.steps"})
        for (const float bad : {nan, inf, -inf})
        {
            processor.state.getRawParameterValue(id)->store(bad);
            const auto read = reader.read();
            INFO(id << " " << bad);
            for (const auto& slot : read.effects)
            {
                REQUIRE(std::isfinite(slot.mix));
                for (float v : slot.values)
                    REQUIRE(std::isfinite(v));
            }
            REQUIRE((read.oversamplingFactor == 1 || read.oversamplingFactor == 2 ||
                     read.oversamplingFactor == 4 || read.oversamplingFactor == 8));
            processor.state.getRawParameterValue(id)->store(processor.state.getParameter(id)->convertFrom0to1(
                processor.state.getParameter(id)->getDefaultValue()));
        }
}

TEST_CASE("The position under a drag is the slot the mouse is over", "[core][fx][rack]")
{
    constexpr double kWidth = 300.0;
    REQUIRE(FxOrderState::positionAt(0.0, kWidth, 4) == 0);
    REQUIRE(FxOrderState::positionAt(299.9, kWidth, 4) == 0);
    REQUIRE(FxOrderState::positionAt(300.0, kWidth, 4) == 1); // each boundary belongs to the next slot
    REQUIRE(FxOrderState::positionAt(599.9, kWidth, 4) == 1);
    REQUIRE(FxOrderState::positionAt(600.0, kWidth, 4) == 2);
    REQUIRE(FxOrderState::positionAt(1199.0, kWidth, 4) == 3);

    // Outside the row it clamps to the first or last slot.
    REQUIRE(FxOrderState::positionAt(-50.0, kWidth, 4) == 0);
    REQUIRE(FxOrderState::positionAt(5000.0, kWidth, 4) == 3);
}

TEST_CASE("An unusable drag position or row gives the first slot", "[core][fx][rack]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    REQUIRE(FxOrderState::positionAt(nan, 300.0, 4) == 0);
    REQUIRE(FxOrderState::positionAt(inf, 300.0, 4) == 0);
    REQUIRE(FxOrderState::positionAt(100.0, 0.0, 4) == 0);
    REQUIRE(FxOrderState::positionAt(100.0, -5.0, 4) == 0);
    REQUIRE(FxOrderState::positionAt(100.0, nan, 4) == 0);
    REQUIRE(FxOrderState::positionAt(100.0, 300.0, 0) == 0);
    REQUIRE(FxOrderState::positionAt(900.0, 300.0, 1) == 0);
}

TEST_CASE("Dragging an effect across the row reorders and publishes the chain", "[core][fx][rack]")
{
    const auto registry = makeRegistry();
    std::atomic<std::uint64_t> packed{0};
    FxOrderState order(registry, packed);

    // Drag the first of three effects to the far right, as the rack does while the mouse moves.
    constexpr double kWidth = 100.0;
    for (const double x : {50.0, 120.0, 190.0, 260.0, 295.0})
    {
        const auto& ids = order.ids();
        const int current = static_cast<int>(std::find(ids.begin(), ids.end(), "testgain") - ids.begin());
        const int target = FxOrderState::positionAt(x, kWidth, 3);
        if (target != current)
            order.move(current, target);
    }
    REQUIRE(order.ids() == std::vector<std::string>{"testclip", "testdelay", "testgain"});
    REQUIRE(packed.load() == fcm::dsp::EffectChain::packOrder(std::vector<int>{1, 2, 0}));

    // And back to the left.
    for (const double x : {250.0, 150.0, 50.0, -10.0})
    {
        const auto& ids = order.ids();
        const int current = static_cast<int>(std::find(ids.begin(), ids.end(), "testgain") - ids.begin());
        const int target = FxOrderState::positionAt(x, kWidth, 3);
        if (target != current)
            order.move(current, target);
    }
    REQUIRE(order.ids() == kRegistration);
    REQUIRE(packed.load() == fcm::dsp::EffectChain::registrationOrder(3));
}

TEST_CASE("The position under a point in a stack of slots of different sizes", "[core][fx][rack]")
{
    const std::vector<double> sizes{130.0, 290.0, 130.0};
    REQUIRE(FxOrderState::positionIn(sizes, 0.0) == 0);
    REQUIRE(FxOrderState::positionIn(sizes, 129.9) == 0);
    REQUIRE(FxOrderState::positionIn(sizes, 130.0) == 1); // each boundary belongs to the next slot
    REQUIRE(FxOrderState::positionIn(sizes, 419.9) == 1);
    REQUIRE(FxOrderState::positionIn(sizes, 420.0) == 2);
    REQUIRE(FxOrderState::positionIn(sizes, 549.0) == 2);

    // Outside the stack the first or last slot.
    REQUIRE(FxOrderState::positionIn(sizes, -30.0) == 0);
    REQUIRE(FxOrderState::positionIn(sizes, 5000.0) == 2);

    // Unusable input is safe.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(FxOrderState::positionIn(sizes, nan) == 0);
    REQUIRE(FxOrderState::positionIn({}, 10.0) == 0);
    REQUIRE(FxOrderState::positionIn({100.0}, 900.0) == 0);
    REQUIRE(FxOrderState::positionIn({nan, 100.0, -5.0}, 50.0) == 1); // bad sizes count as 0
}

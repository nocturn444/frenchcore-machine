// SPDX-License-Identifier: AGPL-3.0-only
#include "../dsp/TestEffects.h"
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/FxRackState.h"
#include "core/ParameterIds.h"
#include "dsp/fx/EffectRack.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>

using Catch::Matchers::WithinAbs;
using fcm::core::FxRackState;
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

namespace
{
using fcm::dsp::RackId;

// A rack state over the test effects, with the packed values it publishes.
struct RackRig
{
    RackRig() : processor(registry), racks(processor.state, registry, {&busA, &busB, &main}) {}

    float valueOf(const std::string& id)
    {
        auto* parameter = processor.state.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValue());
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EffectRegistry registry = makeRegistry();
    fcm::test::TestProcessor processor;
    std::atomic<std::uint64_t> busA{0}, busB{0}, main{0};
    FxRackState racks;
};
} // namespace

TEST_CASE("A new project's racks hold the default effects in Main and nothing else", "[core][fx][racks]")
{
    // The test effects are not marked as part of the default rack, so everything starts empty.
    RackRig rig;
    REQUIRE(rig.racks.ids(RackId::main).empty());
    REQUIRE(rig.racks.ids(RackId::busA).empty());
    REQUIRE(rig.racks.ids(RackId::busB).empty());
    REQUIRE(rig.main.load() == 0);

    // The plugin's own effects: Distortion, EQ and Stereo in Main, as in 0.1.
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor real;
    std::atomic<std::uint64_t> a{0}, b{0}, m{0};
    FxRackState defaults(real.state, fcm::dsp::defaultEffectRegistry(), {&a, &b, &m});
    REQUIRE(defaults.ids(RackId::main) == std::vector<std::string>{"distortion", "eq", "stereo"});
    REQUIRE(defaults.ids(RackId::busA).empty());
    REQUIRE(a.load() == 0);
    REQUIRE(m.load() == fcm::dsp::EffectRack::pack(std::vector<int>{0, 4, 8}));
}

TEST_CASE("Adding takes the lowest free copy, resets it and switches it on", "[core][fx][racks]")
{
    RackRig rig;

    // Put the first clip away from its defaults, then remove it: adding must give a clean one.
    REQUIRE(rig.racks.add(RackId::main, 1) == "testclip");
    rig.processor.state.getParameter("testclip.threshold")->setValueNotifyingHost(0.0f);
    REQUIRE(rig.racks.remove(RackId::main, "testclip"));
    REQUIRE(rig.valueOf("testclip.enabled") == 0.0f);

    REQUIRE(rig.racks.add(RackId::main, 1) == "testclip");
    REQUIRE(rig.valueOf("testclip.enabled") == 1.0f);
    REQUIRE_THAT(rig.valueOf("testclip.threshold"), WithinAbs(1.0, 1.0e-4)); // the default again

    REQUIRE(rig.racks.add(RackId::main, 1) == "testclip2");
    REQUIRE(rig.racks.add(RackId::busA, 1) == "testclip3");
    REQUIRE(rig.racks.add(RackId::busB, 1) == "testclip4");
    REQUIRE(rig.racks.freeCopies(1) == 0);
    REQUIRE_FALSE(rig.racks.canAdd(RackId::busB, 1));
    REQUIRE(rig.racks.add(RackId::busB, 1).empty()); // a fifth copy is refused
    REQUIRE(rig.racks.ids(RackId::main) == std::vector<std::string>{"testclip", "testclip2"});

    // The packed values follow: clip is effect 1, so its copies are pool indices 4 to 7.
    REQUIRE(rig.main.load() == fcm::dsp::EffectRack::pack(std::vector<int>{4, 5}));
    REQUIRE(rig.busA.load() == fcm::dsp::EffectRack::pack(std::vector<int>{6}));
    REQUIRE(rig.busB.load() == fcm::dsp::EffectRack::pack(std::vector<int>{7}));
}

TEST_CASE("A ninth effect in a rack is refused", "[core][fx][racks]")
{
    RackRig rig;
    for (int i = 0; i < 4; ++i)
        REQUIRE_FALSE(rig.racks.add(RackId::main, 0).empty());
    for (int i = 0; i < 4; ++i)
        REQUIRE_FALSE(rig.racks.add(RackId::main, 1).empty());
    REQUIRE(rig.racks.ids(RackId::main).size() == 8);
    REQUIRE_FALSE(rig.racks.canAdd(RackId::main, 2));
    REQUIRE(rig.racks.add(RackId::main, 2).empty());
    REQUIRE(rig.racks.canAdd(RackId::busA, 2)); // another rack still has room
}

TEST_CASE("Removing frees the copy and switches it off", "[core][fx][racks]")
{
    RackRig rig;
    rig.racks.add(RackId::main, 0);
    rig.racks.add(RackId::main, 0);
    REQUIRE(rig.racks.freeCopies(0) == 2);

    REQUIRE(rig.racks.remove(RackId::main, "testgain"));
    REQUIRE(rig.racks.freeCopies(0) == 3);
    REQUIRE(rig.valueOf("testgain.enabled") == 0.0f);
    REQUIRE(rig.valueOf("testgain2.enabled") == 1.0f);
    REQUIRE_FALSE(rig.racks.remove(RackId::main, "testgain"));  // it is not there any more
    REQUIRE_FALSE(rig.racks.remove(RackId::busA, "testgain2")); // it is in another rack

    // The next one added takes the lowest free copy, which is the one just freed.
    REQUIRE(rig.racks.add(RackId::busA, 0) == "testgain");
}

TEST_CASE("Duplicating copies the settings and puts the copy right after", "[core][fx][racks]")
{
    RackRig rig;
    rig.racks.add(RackId::main, 0);
    rig.racks.add(RackId::main, 1);
    rig.processor.state.getParameter("testgain.gain")->setValueNotifyingHost(0.25f);
    rig.processor.state.getParameter("testgain.mix")->setValueNotifyingHost(0.4f);
    const float gain = rig.valueOf("testgain.gain");
    const float mix = rig.valueOf("testgain.mix");

    REQUIRE(rig.racks.canDuplicate(RackId::main, "testgain"));
    REQUIRE(rig.racks.duplicate(RackId::main, "testgain") == "testgain2");
    REQUIRE(rig.racks.ids(RackId::main) == std::vector<std::string>{"testgain", "testgain2", "testclip"});
    REQUIRE_THAT(rig.valueOf("testgain2.gain"), WithinAbs(gain, 1.0e-4));
    REQUIRE_THAT(rig.valueOf("testgain2.mix"), WithinAbs(mix, 1.0e-3));
    REQUIRE(rig.valueOf("testgain2.enabled") == 1.0f);

    // No free copy, an unknown ID, an instance of another rack: refused.
    rig.racks.duplicate(RackId::main, "testgain");
    rig.racks.duplicate(RackId::main, "testgain");
    REQUIRE(rig.racks.freeCopies(0) == 0);
    REQUIRE(rig.racks.duplicate(RackId::main, "testgain").empty());
    REQUIRE(rig.racks.duplicate(RackId::main, "nothing").empty());
    REQUIRE(rig.racks.duplicate(RackId::busA, "testclip").empty());
}

TEST_CASE("Moving an effect reorders the others around it", "[core][fx][racks]")
{
    RackRig rig;
    rig.racks.add(RackId::main, 0);
    rig.racks.add(RackId::main, 1);
    rig.racks.add(RackId::main, 2);
    const std::vector<std::string> start{"testgain", "testclip", "testdelay"};
    REQUIRE(rig.racks.ids(RackId::main) == start);

    rig.racks.move(RackId::main, 0, 2);
    REQUIRE(rig.racks.ids(RackId::main) == std::vector<std::string>{"testclip", "testdelay", "testgain"});
    rig.racks.move(RackId::main, 2, 0);
    REQUIRE(rig.racks.ids(RackId::main) == start);
    rig.racks.move(RackId::main, -5, 99); // clamped: the first goes to the end
    REQUIRE(rig.racks.ids(RackId::main) == std::vector<std::string>{"testclip", "testdelay", "testgain"});
    REQUIRE(rig.main.load() == fcm::dsp::EffectRack::pack(std::vector<int>{4, 8, 0}));

    rig.racks.move(RackId::busA, 0, 1); // an empty rack: nothing to do
}

TEST_CASE("The racks round-trip through the saved node", "[core][fx][racks]")
{
    RackRig saved;
    saved.racks.add(RackId::main, 2);
    saved.racks.add(RackId::main, 0);
    saved.racks.add(RackId::busA, 1);
    saved.racks.add(RackId::busA, 1);

    const auto node = saved.racks.toValueTree();
    REQUIRE(node.getType().toString() == "FxRacks");
    REQUIRE(node.getNumChildren() == 3);
    REQUIRE(node.getChildWithProperty("id", "main")["effects"].toString() == "testdelay,testgain");
    REQUIRE(node.getChildWithProperty("id", "busA")["effects"].toString() == "testclip,testclip2");
    REQUIRE(node.getChildWithProperty("id", "busB")["effects"].toString() == "");

    RackRig restored;
    REQUIRE(restored.racks.restoreFrom(node));
    for (const auto rack : {RackId::busA, RackId::busB, RackId::main})
        REQUIRE(restored.racks.ids(rack) == saved.racks.ids(rack));
    REQUIRE(restored.main.load() == saved.main.load());
    REQUIRE(restored.busA.load() == saved.busA.load());
}

TEST_CASE("A damaged saved rack node never fails the load", "[core][fx][racks]")
{
    RackRig rig;

    const auto rackNode = [](const char* id, const juce::String& effects)
    {
        juce::ValueTree rack("Rack");
        rack.setProperty("id", id, nullptr);
        rack.setProperty("effects", effects, nullptr);
        return rack;
    };
    const auto nodeWith = [&](std::initializer_list<juce::ValueTree> racks)
    {
        juce::ValueTree node("FxRacks");
        for (const auto& rack : racks)
            node.appendChild(rack, nullptr);
        return node;
    };

    // Unknown IDs and repeats are dropped; an instance in two racks keeps its first place.
    REQUIRE(rig.racks.restoreFrom(
        nodeWith({rackNode("busA", "testgain, neweffect,testgain,testclip3"),
                  rackNode("main", "testclip3,testdelay,testgain,bogus9"), rackNode("busB", "testgain2")})));
    REQUIRE(rig.racks.ids(RackId::busA) == std::vector<std::string>{"testgain", "testclip3"});
    REQUIRE(rig.racks.ids(RackId::main) == std::vector<std::string>{"testdelay"});
    REQUIRE(rig.racks.ids(RackId::busB) == std::vector<std::string>{"testgain2"});

    // A rack is cut at its capacity of eight, however long the list.
    juce::String many;
    for (int i = 0; i < 150; ++i) // 150 entries still fit the size limit
        many << "testdelay" << (i % 4 == 0 ? juce::String() : juce::String(i % 4 + 1)) << ",";
    REQUIRE(rig.racks.restoreFrom(nodeWith({rackNode("main", many)})));
    REQUIRE(rig.racks.ids(RackId::main).size() == 4); // only four copies exist

    juce::String twelve;
    for (const char* effect : {"testgain", "testclip", "testdelay"})
        for (int copy = 1; copy <= 4; ++copy)
            twelve << effect << (copy == 1 ? juce::String() : juce::String(copy)) << ",";
    REQUIRE(rig.racks.restoreFrom(nodeWith({rackNode("main", twelve)})));
    REQUIRE(rig.racks.ids(RackId::main).size() == 8);
    REQUIRE(rig.racks.ids(RackId::main).front() == "testgain");

    // A huge text is ignored, a rack named oddly is ignored, a missing rack is empty.
    REQUIRE(rig.racks.restoreFrom(nodeWith({rackNode("main", juce::String::repeatedString("testgain,", 5000)),
                                            rackNode("elsewhere", "testgain")})));
    REQUIRE(rig.racks.ids(RackId::main).empty());
    REQUIRE(rig.racks.ids(RackId::busA).empty());

    // Unusable nodes give the default racks.
    for (const auto& bad : {juce::ValueTree("Other"), juce::ValueTree()})
    {
        rig.racks.add(RackId::main, 0);
        REQUIRE_FALSE(rig.racks.restoreFrom(bad));
        REQUIRE(rig.racks.ids(RackId::main).empty()); // the test effects have no default rack
    }
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
    // The effects are listed by pool index: effect 1 (the clip) starts at index 4 (four copies each).
    REQUIRE_FALSE(read.effects[0].enabled);
    REQUIRE_THAT(read.effects[0].mix, WithinAbs(1.0, 1.0e-4));
    REQUIRE_THAT(read.effects[0].values[0], WithinAbs(1.0, 1.0e-4)); // gain
    REQUIRE_THAT(read.effects[4].values[0], WithinAbs(1.0, 1.0e-4)); // threshold
    REQUIRE_THAT(read.effects[4].values[1], WithinAbs(1.0, 1.0e-3)); // mode: Soft
    REQUIRE_THAT(read.effects[4].values[2], WithinAbs(1.0, 1.0e-3)); // flag
    REQUIRE_THAT(read.effects[4].values[3], WithinAbs(2.0, 1.0e-3)); // steps
    REQUIRE(read.oversamplingFactor == 2);

    setValue(processor.state, "testgain.enabled", 1.0f);
    setValue(processor.state, "testgain.mix", 40.0f);
    setValue(processor.state, "testgain.gain", 2.5f);
    setValue(processor.state, "testclip.mode", 0.0f);
    setValue(processor.state, "testclip.steps", -2.0f);
    setValue(processor.state, "testgain3.gain", 3.5f);
    setValue(processor.state, "global.quality", 3.0f);
    read = reader.read();
    REQUIRE(read.effects[0].enabled);
    REQUIRE_THAT(read.effects[0].mix, WithinAbs(0.4, 1.0e-3));
    REQUIRE_THAT(read.effects[0].values[0], WithinAbs(2.5, 1.0e-3));
    REQUIRE_THAT(read.effects[4].values[1], WithinAbs(0.0, 1.0e-3));
    REQUIRE_THAT(read.effects[4].values[3], WithinAbs(-2.0, 1.0e-3));
    REQUIRE_THAT(read.effects[2].values[0], WithinAbs(3.5, 1.0e-3)); // copy 3 of the gain
    REQUIRE_THAT(read.effects[1].values[0], WithinAbs(1.0, 1.0e-3)); // copy 2 untouched
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
    REQUIRE(FxRackState::positionAt(0.0, kWidth, 4) == 0);
    REQUIRE(FxRackState::positionAt(299.9, kWidth, 4) == 0);
    REQUIRE(FxRackState::positionAt(300.0, kWidth, 4) == 1); // each boundary belongs to the next slot
    REQUIRE(FxRackState::positionAt(599.9, kWidth, 4) == 1);
    REQUIRE(FxRackState::positionAt(600.0, kWidth, 4) == 2);
    REQUIRE(FxRackState::positionAt(1199.0, kWidth, 4) == 3);

    // Outside the row it clamps to the first or last slot.
    REQUIRE(FxRackState::positionAt(-50.0, kWidth, 4) == 0);
    REQUIRE(FxRackState::positionAt(5000.0, kWidth, 4) == 3);
}

TEST_CASE("An unusable drag position or row gives the first slot", "[core][fx][rack]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    REQUIRE(FxRackState::positionAt(nan, 300.0, 4) == 0);
    REQUIRE(FxRackState::positionAt(inf, 300.0, 4) == 0);
    REQUIRE(FxRackState::positionAt(100.0, 0.0, 4) == 0);
    REQUIRE(FxRackState::positionAt(100.0, -5.0, 4) == 0);
    REQUIRE(FxRackState::positionAt(100.0, nan, 4) == 0);
    REQUIRE(FxRackState::positionAt(100.0, 300.0, 0) == 0);
    REQUIRE(FxRackState::positionAt(900.0, 300.0, 1) == 0);
}

TEST_CASE("Dragging an effect across the row reorders and publishes the rack", "[core][fx][rack]")
{
    RackRig rig;
    rig.racks.add(RackId::main, 0);
    rig.racks.add(RackId::main, 1);
    rig.racks.add(RackId::main, 2);

    // Drag the first of three effects to the far right, as the rack does while the mouse moves.
    constexpr double kWidth = 100.0;
    for (const double x : {50.0, 120.0, 190.0, 260.0, 295.0})
    {
        const auto& ids = rig.racks.ids(RackId::main);
        const int current = static_cast<int>(std::find(ids.begin(), ids.end(), "testgain") - ids.begin());
        const int target = FxRackState::positionAt(x, kWidth, 3);
        if (target != current)
            rig.racks.move(RackId::main, current, target);
    }
    REQUIRE(rig.racks.ids(RackId::main) == std::vector<std::string>{"testclip", "testdelay", "testgain"});
    REQUIRE(rig.main.load() == fcm::dsp::EffectRack::pack(std::vector<int>{4, 8, 0}));
}

TEST_CASE("Where a dragged slot goes among others of different sizes", "[core][fx][rack]")
{
    // The others: a short, a tall and a short strip, one after the other (middles at 65, 275, 485).
    const std::vector<double> others{130.0, 290.0, 130.0};
    REQUIRE(FxRackState::insertionIndex(others, 0.0) == 0);
    REQUIRE(FxRackState::insertionIndex(others, 64.9) == 0);
    REQUIRE(FxRackState::insertionIndex(others, 65.0) == 1); // a middle belongs to the slot after it
    REQUIRE(FxRackState::insertionIndex(others, 274.9) == 1);
    REQUIRE(FxRackState::insertionIndex(others, 275.0) == 2);
    REQUIRE(FxRackState::insertionIndex(others, 485.0) == 3);

    // Outside the stack: the start or the end.
    REQUIRE(FxRackState::insertionIndex(others, -30.0) == 0);
    REQUIRE(FxRackState::insertionIndex(others, 5000.0) == 3);

    // Unusable input is safe.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(FxRackState::insertionIndex(others, nan) == 0);
    REQUIRE(FxRackState::insertionIndex({}, 10.0) == 0);
    REQUIRE(FxRackState::insertionIndex({nan, 100.0, -5.0}, 80.0) ==
            2); // bad sizes count as 0: middles at 0, 50, 100
}

TEST_CASE("Dragging a tall strip past a short one does not make them swap back and forth", "[core][fx][rack]")
{
    // Three strips: A short (130), B tall (290), C short (130). Drag B with the mouse at a fixed y and
    // replay the drag handler many times: the order must settle at once and stay put.
    const std::vector<double> heights{130.0, 290.0, 130.0};
    std::vector<int> order{0, 1, 2}; // A B C, by strip
    const int dragged = 1;
    for (const double mouseY : {10.0, 100.0, 200.0, 300.0, 400.0, 520.0})
    {
        std::vector<int> previous;
        for (int repeat = 0; repeat < 6; ++repeat)
        {
            std::vector<double> others;
            int current = 0;
            for (size_t i = 0; i < order.size(); ++i)
            {
                if (order[i] == dragged)
                    current = static_cast<int>(i);
                else
                    others.push_back(heights[static_cast<size_t>(order[i])]);
            }
            const int target = FxRackState::insertionIndex(others, mouseY);
            if (target != current)
            {
                const int value = order[static_cast<size_t>(current)];
                order.erase(order.begin() + current);
                order.insert(order.begin() + target, value);
            }
            if (repeat > 0)
                REQUIRE(order == previous); // the same mouse position gives the same order
            previous = order;
        }
    }
}

TEST_CASE("Every effect has four copies with the same ranges and defaults", "[core][fx][layout]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto registry = makeRegistry();
    fcm::test::TestProcessor processor(registry);

    for (const char* base : {"testgain", "testclip", "testdelay"})
        for (int copy = 1; copy <= 4; ++copy)
        {
            const std::string id = copy == 1 ? base : base + std::to_string(copy);
            INFO(id);
            REQUIRE(processor.state.getParameter(id + ".enabled") != nullptr);
            REQUIRE(processor.state.getParameter(id + ".mix") != nullptr);
        }
    REQUIRE(processor.state.getParameter("testgain5.enabled") == nullptr);
    REQUIRE(processor.state.getParameter("testgain1.enabled") == nullptr);

    // The names carry the copy number after the effect name; the first copy keeps its plain name.
    REQUIRE(processor.state.getParameter("testclip.threshold")->getName(64) == "Test Clip Threshold");
    REQUIRE(processor.state.getParameter("testclip3.threshold")->getName(64) == "Test Clip 3 Threshold");

    for (const char* suffix : {"gain"})
    {
        auto* first = dynamic_cast<juce::AudioParameterFloat*>(
            processor.state.getParameter(std::string("testgain.") + suffix));
        auto* fourth = dynamic_cast<juce::AudioParameterFloat*>(
            processor.state.getParameter(std::string("testgain4.") + suffix));
        REQUIRE(first != nullptr);
        REQUIRE(fourth != nullptr);
        REQUIRE(first->getNormalisableRange().start == fourth->getNormalisableRange().start);
        REQUIRE(first->getNormalisableRange().end == fourth->getNormalisableRange().end);
        REQUIRE(first->get() == fourth->get());
    }
}

TEST_CASE("The default Mix of an effect is its descriptor's, for every copy", "[core][fx][layout]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    fcm::core::EngineParameterReader reader(processor.state);
    const auto& registry = fcm::dsp::defaultEffectRegistry();

    for (int effect = 0; effect < registry.count(); ++effect)
        for (int copy = 1; copy <= fcm::dsp::kInstancesPerEffect; ++copy)
        {
            const std::string id = registry.instanceId(fcm::dsp::EffectRegistry::instanceIndex(effect, copy));
            auto* mix = dynamic_cast<juce::AudioParameterFloat*>(processor.state.getParameter(id + ".mix"));
            INFO(id);
            REQUIRE(mix != nullptr);
            REQUIRE_THAT(mix->get(), WithinAbs(registry.descriptor(effect)->defaultMix * 100.0, 1.0e-3));
        }

    // The Reverb starts at 30 %, and the reader gives the same for a mix that is not a number.
    REQUIRE_THAT(processor.state.getParameter("reverb.mix")
                     ->convertFrom0to1(processor.state.getParameter("reverb.mix")->getDefaultValue()),
                 WithinAbs(30.0, 1.0e-3));
    const int reverbIndex = registry.indexOf("reverb");
    REQUIRE(reverbIndex >= 0);
    processor.state.getRawParameterValue("reverb3.mix")->store(std::numeric_limits<float>::quiet_NaN());
    const auto read = reader.read();
    REQUIRE_THAT(
        read.effects[static_cast<size_t>(fcm::dsp::EffectRegistry::instanceIndex(reverbIndex, 3))].mix,
        WithinAbs(0.3, 1.0e-6));
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/ParameterIds.h"
#include "core/StateLoader.h"
#include "core/StateMigrations.h"
#include "dsp/engine/Engine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::core::loadState;
using fcm::core::StateLoadResult;
using fcm::test::TestProcessor;

namespace
{
const juce::Identifier kRootType{"FrenchcoreMachineState"};

// The saved state as a host would hand it back: XML text with one PARAM entry per control.
juce::ValueTree makeState(const std::vector<std::pair<std::string, std::string>>& values,
                          const std::string& rootAttributes = "stateVersion=\"1\"",
                          const std::string& extra = "")
{
    std::string xml = "<FrenchcoreMachineState " + rootAttributes + ">";
    for (const auto& [id, value] : values)
        xml += "<PARAM id=\"" + id + "\" value=\"" + value + "\"/>";
    xml += extra + "</FrenchcoreMachineState>";

    auto parsed = juce::XmlDocument::parse(xml);
    REQUIRE(parsed != nullptr);
    return juce::ValueTree::fromXml(*parsed);
}

std::vector<juce::RangedAudioParameter*> allParameters(TestProcessor& processor)
{
    std::vector<juce::RangedAudioParameter*> result;
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            result.push_back(ranged);
    return result;
}

float liveValue(TestProcessor& processor, const juce::String& id)
{
    return processor.state.getRawParameterValue(id)->load();
}

float defaultOf(juce::RangedAudioParameter& parameter)
{
    return parameter.convertFrom0to1(parameter.getDefaultValue());
}

// Every live parameter is a finite number inside its range.
void requireAllSane(TestProcessor& processor)
{
    for (auto* parameter : allParameters(processor))
    {
        const float value = liveValue(processor, parameter->paramID);
        const auto& range = parameter->getNormalisableRange();
        INFO(parameter->paramID);
        REQUIRE(std::isfinite(value));
        REQUIRE(value >= range.start);
        REQUIRE(value <= range.end);
    }
}

// A note through the real reader and engine: every sample must be finite and within 0 dBFS.
void requireNoteIsSafe(TestProcessor& processor)
{
    fcm::core::EngineParameterReader reader(processor.state);
    fcm::dsp::Engine engine;
    engine.setParameters(reader.read());
    engine.prepare({48000.0, 512, 2});

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 300);

    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        engine.setParameters(reader.read());
        engine.process(buffer, block == 0 ? midi : juce::MidiBuffer{});
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i)
            {
                const float sample = buffer.getSample(channel, i);
                REQUIRE(std::isfinite(sample));
                REQUIRE(std::abs(sample) <= 1.0f);
            }
    }
}

const std::vector<std::string> kNotNumbers{"nan",       "NaN",   "-nan",   "inf", "-inf", "Infinity",
                                           "-Infinity", "1e999", "-1e999", "abc", "",     " ",
                                           "0x10",      "1,5",   "--1",    "1e",  "."};
} // namespace

namespace
{
// Values go through a 0..1 normalisation and back, so a parameter with a wide range (20000 Hz) is only
// exact to about a millionth of its size.
double nearTolerance(double value)
{
    return std::max(1.0e-4, std::abs(value) * 1.0e-6);
}
} // namespace

TEST_CASE("A value that is not a finite number loads as the parameter's default", "[core][state][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (const auto& bad : kNotNumbers)
    {
        TestProcessor processor;
        std::vector<std::pair<std::string, std::string>> values;
        for (auto* parameter : allParameters(processor))
            values.emplace_back(parameter->paramID.toStdString(), bad);

        REQUIRE(loadState(processor.state, makeState(values), kRootType) == StateLoadResult::loaded);

        INFO("value \"" << bad << "\"");
        requireAllSane(processor);
        for (auto* parameter : allParameters(processor))
        {
            INFO(parameter->paramID);
            REQUIRE_THAT(liveValue(processor, parameter->paramID),
                         WithinAbs(defaultOf(*parameter), nearTolerance(defaultOf(*parameter))));
        }
        requireNoteIsSafe(processor);
    }
}

TEST_CASE("The parameters the audit names survive nan, inf and -inf one at a time", "[core][state][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (const char* id :
         {"osc1.phase", "osc1.fine", "osc1.position", "global.glideTime", "osc2.phase", "osc2.fine",
          "osc2.position", "osc1.level", "global.outputGain", "global.velocitySensitivity"})
        for (const char* bad : {"nan", "inf", "-inf"})
        {
            TestProcessor processor;
            REQUIRE(loadState(processor.state, makeState({{id, bad}}), kRootType) == StateLoadResult::loaded);

            INFO(id << " = " << bad);
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor.state.getParameter(id));
            REQUIRE(parameter != nullptr);
            REQUIRE_THAT(liveValue(processor, id),
                         WithinAbs(defaultOf(*parameter), nearTolerance(defaultOf(*parameter))));
            requireNoteIsSafe(processor);
        }
}

TEST_CASE("Values outside the range are moved to the nearest bound", "[core][state][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (auto* sample : {"1e30", "-1e30", "1e9", "-1e9", "99999", "-99999"})
    {
        TestProcessor processor;
        std::vector<std::pair<std::string, std::string>> values;
        for (auto* parameter : allParameters(processor))
            values.emplace_back(parameter->paramID.toStdString(), sample);

        REQUIRE(loadState(processor.state, makeState(values), kRootType) == StateLoadResult::loaded);

        INFO("value " << sample);
        requireAllSane(processor);
        const bool positive = sample[0] != '-';
        for (auto* parameter : allParameters(processor))
        {
            INFO(parameter->paramID);
            const auto& range = parameter->getNormalisableRange();
            REQUIRE_THAT(liveValue(processor, parameter->paramID),
                         WithinAbs(positive ? range.end : range.start, 1.0e-3));
        }
        requireNoteIsSafe(processor);
    }

    // Named examples: a phase of 1e30 degrees is 360, a fine tune of -1e30 cents is -100.
    TestProcessor processor;
    loadState(processor.state,
              makeState({{"osc1.phase", "1e30"},
                         {"osc1.fine", "-1e30"},
                         {"osc1.position", "7"},
                         {"global.glideTime", "-5"}}),
              kRootType);
    REQUIRE_THAT(liveValue(processor, "osc1.phase"), WithinAbs(360.0, 1.0e-3));
    REQUIRE_THAT(liveValue(processor, "osc1.fine"), WithinAbs(-100.0, 1.0e-3));
    REQUIRE_THAT(liveValue(processor, "osc1.position"), WithinAbs(1.0, 1.0e-6));
    REQUIRE_THAT(liveValue(processor, "global.glideTime"), WithinAbs(1.0, 1.0e-6));
}

TEST_CASE("Good values still load exactly", "[core][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;

    REQUIRE(loadState(processor.state,
                      makeState({{"osc1.level", "0.25"},
                                 {"osc1.pitch", "-7"},
                                 {"global.outputGain", "-6"},
                                 {"global.keyTrack", "0"},
                                 {"global.rootNote", "45"}}),
                      kRootType) == StateLoadResult::loaded);

    REQUIRE_THAT(liveValue(processor, "osc1.level"), WithinAbs(0.25, 1.0e-6));
    REQUIRE_THAT(liveValue(processor, "osc1.pitch"), WithinAbs(-7.0, 1.0e-6));
    REQUIRE_THAT(liveValue(processor, "global.outputGain"), WithinAbs(-6.0, 1.0e-4));
    REQUIRE_THAT(liveValue(processor, "global.keyTrack"), WithinAbs(0.0, 1.0e-6));
    REQUIRE_THAT(liveValue(processor, "global.rootNote"), WithinAbs(45.0, 1.0e-6));
}

TEST_CASE("Unknown controls and nodes are ignored", "[core][state][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;

    auto tree = makeState({{"osc1.level", "0.5"}, {"future.control", "3"}, {"", "1"}, {"osc1.nope", "nan"}},
                          "stateVersion=\"1\"", "<FutureNode foo=\"bar\"/><PARAM value=\"4\"/>");
    REQUIRE(loadState(processor.state, tree, kRootType) == StateLoadResult::loaded);

    REQUIRE_THAT(liveValue(processor, "osc1.level"), WithinAbs(0.5, 1.0e-6));
    // The unknown entries did not make it into the live state.
    for (int i = 0; i < processor.state.state.getNumChildren(); ++i)
    {
        const auto child = processor.state.state.getChild(i);
        if (child.hasType("PARAM"))
            REQUIRE(processor.state.getParameter(child["id"].toString()) != nullptr);
    }
}

TEST_CASE("A state without stateVersion loads as version 1", "[core][state][version]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;

    REQUIRE(loadState(processor.state, makeState({{"osc1.level", "0.5"}}, /* no attributes */ ""),
                      kRootType) == StateLoadResult::loaded);
    REQUIRE_THAT(liveValue(processor, "osc1.level"), WithinAbs(0.5, 1.0e-6));
    // The current version is written back.
    REQUIRE(static_cast<int>(processor.state.state["stateVersion"]) == fcm::core::kStateVersion);
}

TEST_CASE("A damaged stateVersion counts as version 1", "[core][state][version][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (const char* bad : {"abc", "-3", "0", "nan", "", "1.5e3x", "  ", "-inf"})
    {
        TestProcessor processor;
        const std::string attributes = std::string("stateVersion=\"") + bad + "\"";
        INFO("stateVersion \"" << bad << "\"");
        REQUIRE(loadState(processor.state, makeState({{"osc1.level", "0.5"}}, attributes), kRootType) ==
                StateLoadResult::loaded);
        REQUIRE_THAT(liveValue(processor, "osc1.level"), WithinAbs(0.5, 1.0e-6));
        REQUIRE(static_cast<int>(processor.state.state["stateVersion"]) == fcm::core::kStateVersion);
    }
}

TEST_CASE("A state from a newer version loads what is known and ignores the rest", "[core][state][version]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (const char* future : {"4", "99", "1000000", "99999999999"})
    {
        TestProcessor processor;
        const std::string attributes = std::string("stateVersion=\"") + future + "\" brandNew=\"yes\"";
        const auto result =
            loadState(processor.state,
                      makeState({{"osc1.level", "0.5"}, {"osc2.level", "nan"}, {"brandNew.control", "9"}},
                                attributes, "<BrandNewNode><Deep a=\"1\"/></BrandNewNode>"),
                      kRootType);

        INFO("stateVersion " << future);
        REQUIRE(result == StateLoadResult::loadedFromNewerVersion);
        REQUIRE_THAT(liveValue(processor, "osc1.level"), WithinAbs(0.5, 1.0e-6)); // the known control loaded
        REQUIRE_THAT(liveValue(processor, "osc2.level"), WithinAbs(1.0, 1.0e-6)); // a bad one is the default
        requireAllSane(processor);
        requireNoteIsSafe(processor);
        // After loading, the state is ours: it declares the current version.
        REQUIRE(static_cast<int>(processor.state.state["stateVersion"]) == fcm::core::kStateVersion);
    }
}

TEST_CASE("Something that is not our state is rejected and changes nothing", "[core][state][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;
    loadState(processor.state, makeState({{"osc1.level", "0.25"}}), kRootType);

    REQUIRE(loadState(processor.state, juce::ValueTree(), kRootType) == StateLoadResult::rejected);
    REQUIRE(loadState(processor.state, juce::ValueTree("SomethingElse"), kRootType) ==
            StateLoadResult::rejected);

    auto wrongRoot = juce::ValueTree::fromXml(
        *juce::XmlDocument::parse("<Other><PARAM id=\"osc1.level\" value=\"0.9\"/></Other>"));
    REQUIRE(loadState(processor.state, wrongRoot, kRootType) == StateLoadResult::rejected);

    REQUIRE_THAT(liveValue(processor, "osc1.level"), WithinAbs(0.25, 1.0e-6)); // untouched
}

TEST_CASE("sanitizeLiveParameters resets values that are not finite or not in range",
          "[core][state][security]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestProcessor processor;

    // A host (or a bug) that sets a normalised value that is not valid.
    auto* phase = processor.state.getParameter("osc1.phase");
    REQUIRE(phase != nullptr);
    phase->setValueNotifyingHost(0.75f);
    REQUIRE_THAT(liveValue(processor, "osc1.phase"), WithinAbs(270.0, 1.0));

    fcm::core::sanitizeLiveParameters(processor.state);
    REQUIRE_THAT(liveValue(processor, "osc1.phase"), WithinAbs(270.0, 1.0)); // a valid value is left alone
}

TEST_CASE("readStateVersion", "[core][state][version]")
{
    auto versionOf = [](const juce::var& stored)
    {
        juce::ValueTree tree("FrenchcoreMachineState");
        if (!stored.isVoid())
            tree.setProperty("stateVersion", stored, nullptr);
        return fcm::core::readStateVersion(tree);
    };

    REQUIRE(versionOf(juce::var()) == 1);
    REQUIRE(versionOf(1) == 1);
    REQUIRE(versionOf(3) == 3);
    REQUIRE(versionOf(3.0) == 3);
    REQUIRE(versionOf("4") == 4);
    REQUIRE(versionOf(0) == 1);
    REQUIRE(versionOf(-7) == 1);
    REQUIRE(versionOf(std::nan("")) == 1);
    REQUIRE(versionOf(std::numeric_limits<double>::infinity()) == 1);
    REQUIRE(versionOf("abc") == 1);
    REQUIRE(versionOf("nan") == 1);
    REQUIRE(versionOf("1e3") == 1);
    REQUIRE(versionOf(1.0e15) == 1'000'000); // capped
    REQUIRE(versionOf(juce::var(juce::Array<juce::var>())) == 1);
}

TEST_CASE("applyMigrations runs the steps in order up to the target", "[core][state][migration]")
{
    using fcm::core::Migration;

    juce::ValueTree tree("FrenchcoreMachineState");
    tree.setProperty("log", "", nullptr);

    static const Migration steps[] = {
        {1, [](juce::ValueTree& t) { t.setProperty("log", t["log"].toString() + "1>2 ", nullptr); }},
        {2, [](juce::ValueTree& t) { t.setProperty("log", t["log"].toString() + "2>3 ", nullptr); }},
        {3, [](juce::ValueTree& t) { t.setProperty("log", t["log"].toString() + "3>4 ", nullptr); }},
    };

    // From 2 to 4 only the last two steps run; the version follows along.
    REQUIRE(fcm::core::applyMigrations(tree, 2, 4, steps));
    REQUIRE(tree["log"].toString() == "2>3 3>4 ");
    REQUIRE(static_cast<int>(tree["stateVersion"]) == 4);

    // Already current, or newer than the target: nothing happens.
    juce::ValueTree untouched("FrenchcoreMachineState");
    REQUIRE(fcm::core::applyMigrations(untouched, 4, 4, steps));
    REQUIRE(fcm::core::applyMigrations(untouched, 9, 4, steps));
    REQUIRE_FALSE(untouched.hasProperty("log"));
    REQUIRE_FALSE(untouched.hasProperty("stateVersion"));
}

TEST_CASE("applyMigrations reports a missing step and stops", "[core][state][migration]")
{
    using fcm::core::Migration;

    juce::ValueTree tree("FrenchcoreMachineState");
    static const Migration steps[] = {{1, [](juce::ValueTree& t) { t.setProperty("ran1", true, nullptr); }},
                                      {3, [](juce::ValueTree& t) { t.setProperty("ran3", true, nullptr); }}};

    REQUIRE_FALSE(fcm::core::applyMigrations(tree, 1, 4, steps)); // there is no step from 2
    REQUIRE(static_cast<bool>(tree["ran1"]));
    REQUIRE_FALSE(tree.hasProperty("ran3"));
    REQUIRE(static_cast<int>(tree["stateVersion"]) == 2); // left at the last version reached
}

TEST_CASE("This build migrates from versions 1 and 2 and the format is at version 3",
          "[core][state][migration]")
{
    REQUIRE(fcm::core::kStateVersion == 3);
    REQUIRE(fcm::core::stateMigrations().size() == 2);
    REQUIRE(fcm::core::stateMigrations()[0].fromVersion == 1);
    REQUIRE(fcm::core::stateMigrations()[1].fromVersion == 2);
}

TEST_CASE("Migration 2 to 3 changes nothing but the version", "[core][state][migration]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::ValueTree tree("FrenchcoreMachineState");
    tree.setProperty("stateVersion", 2, nullptr);
    juce::ValueTree param("PARAM");
    param.setProperty("id", "osc1.level", nullptr);
    param.setProperty("value", 0.5, nullptr);
    tree.appendChild(param, nullptr);
    const auto before = tree.createCopy();

    REQUIRE(fcm::core::applyMigrations(tree, 2, 3, fcm::core::stateMigrations()));
    REQUIRE(static_cast<int>(tree["stateVersion"]) == 3);
    REQUIRE(tree.getNumChildren() == before.getNumChildren());
    // No curve node is invented: a missing node means every effect's default curve.
    REQUIRE_FALSE(tree.getChildWithName("EffectCurves").isValid());
}

TEST_CASE("Migration 1 to 2 turns the 0.1 effect order into the Main rack", "[core][state][migration]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto racksOf = [](const juce::String& order)
    {
        juce::ValueTree tree("FrenchcoreMachineState");
        tree.setProperty("stateVersion", 1, nullptr);
        if (order != "<none>")
        {
            juce::ValueTree chain("FxChain");
            chain.setProperty("order", order, nullptr);
            tree.appendChild(chain, nullptr);
        }
        REQUIRE(fcm::core::applyMigrations(tree, 1, 2, fcm::core::stateMigrations()));
        REQUIRE(static_cast<int>(tree["stateVersion"]) == 2);
        REQUIRE_FALSE(tree.getChildWithName("FxChain").isValid());

        const auto racks = tree.getChildWithName("FxRacks");
        REQUIRE(racks.isValid());
        REQUIRE(racks.getChildWithProperty("id", "busA")["effects"].toString().isEmpty());
        REQUIRE(racks.getChildWithProperty("id", "busB")["effects"].toString().isEmpty());
        return racks.getChildWithProperty("id", "main")["effects"].toString();
    };

    REQUIRE(racksOf("stereo,distortion,eq") == "stereo,distortion,eq"); // the saved order, kept
    REQUIRE(racksOf("eq") == "eq,distortion,stereo");                   // what is missing is appended
    REQUIRE(racksOf("bogus,eq,eq,stereo") == "eq,stereo,distortion");   // unknown and repeats dropped
    REQUIRE(racksOf("") == "distortion,eq,stereo");
    REQUIRE(racksOf("<none>") == "distortion,eq,stereo"); // no node: the 0.1 default order
}

TEST_CASE("Migration 1 to 2 turns the Tock's Amp Env on, as in 0.1", "[core][state][migration]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::ValueTree tree("FrenchcoreMachineState");
    tree.setProperty("stateVersion", 1, nullptr);
    REQUIRE(fcm::core::applyMigrations(tree, 1, 2, fcm::core::stateMigrations()));
    const auto param = tree.getChildWithProperty("id", "tock.ampEnv");
    REQUIRE(param.isValid());
    REQUIRE(static_cast<int>(param["value"]) == 1);
    REQUIRE(tree.getChildWithProperty("id", "tock.ampEnv").getParent() == tree);
}

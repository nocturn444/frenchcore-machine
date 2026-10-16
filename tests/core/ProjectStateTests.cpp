// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/ParameterIds.h"
#include "core/ProjectState.h"
#include "dsp/envelope/EnvelopePresets.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <numbers>

using Catch::Matchers::WithinAbs;
using fcm::core::ProjectState;
using fcm::core::StateLoadResult;
using fcm::dsp::AmpKnob;
using fcm::dsp::BreakpointEnvelope;
using fcm::dsp::EnvelopeKind;
using fcm::dsp::RackId;
namespace Ids = fcm::core::ParameterIds;

namespace
{
const juce::Identifier kRoot{"FrenchcoreMachineState"};

// All the pieces of a plugin's state, without the audio engine.
struct Rig
{
    Rig()
        : project(
              ProjectState::Parts{processor.state, kRoot, amp, pitch, fx, {&wavetable1, &wavetable2}, tock})
    {
    }

    void setParameter(const std::string& id, float value)
    {
        auto* parameter = processor.state.getParameter(id);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    float getParameter(const std::string& id)
    {
        auto* parameter = processor.state.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValue());
    }

    float defaultOf(const std::string& id)
    {
        auto* parameter = processor.state.getParameter(id);
        return parameter->convertFrom0to1(parameter->getDefaultValue());
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    std::atomic<std::uint64_t> packedBusA{0};
    std::atomic<std::uint64_t> packedBusB{0};
    std::atomic<std::uint64_t> packedMain{0};
    fcm::core::EnvelopeState amp{EnvelopeKind::amp};
    fcm::core::EnvelopeState pitch{EnvelopeKind::pitch};
    fcm::core::FxRackState fx{
        processor.state, fcm::dsp::defaultEffectRegistry(), {&packedBusA, &packedBusB, &packedMain}};
    fcm::core::WavetableSelector wavetable1{processor.state, 0};
    fcm::core::WavetableSelector wavetable2{processor.state, 1};
    fcm::core::TockSelector tock{processor.state};
    ProjectState project;
};

std::vector<float> sineFrames(int frames)
{
    std::vector<float> out;
    for (int f = 0; f < frames; ++f)
        for (int i = 0; i < 2048; ++i)
            out.push_back(static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * (f + 1) * i / 2048.0)));
    return out;
}

// Puts every kind of state away from its default.
void personalise(Rig& rig)
{
    rig.setParameter(Ids::kOutputGain, -9.0f);
    rig.setParameter(Ids::kKeyTrack, 0.0f);
    rig.setParameter(Ids::kRootNote, 41.0f);
    rig.setParameter(Ids::kTockOn, 1.0f);
    rig.setParameter(Ids::kTockLevel, -3.0f);
    rig.setParameter(Ids::oscillator(1, Ids::Oscillator::kOn), 1.0f);
    rig.setParameter(Ids::oscillator(0, Ids::Oscillator::kPhase), 90.0f);
    rig.setParameter("distortion.enabled", 1.0f);
    rig.setParameter("distortion.drive", 77.0f);
    rig.setParameter("eq.b1.gain", 4.5f);
    rig.setParameter(Ids::kQuality, 3.0f);

    rig.amp.edit(
        [](BreakpointEnvelope& envelope)
        {
            envelope.setKnob(AmpKnob::sustain, 0.35f);
            envelope.enableSync(150.0f);
        });
    rig.pitch.set(fcm::dsp::EnvelopePresets::all(EnvelopeKind::pitch)[1].envelope);

    rig.fx.move(RackId::main, 2, 0); // Stereo first
    rig.fx.add(RackId::busA, 0);     // a second kind of content: Distortion on Bus A
    rig.fx.add(RackId::busA, 0);

    REQUIRE(rig.wavetable1.loadUser(sineFrames(3), "Osc One"));
    REQUIRE(rig.wavetable2.loadUser(sineFrames(1), "Osc Two"));
    rig.setParameter(Ids::oscillator(0, Ids::Oscillator::kWavetable),
                     static_cast<float>(fcm::core::WavetableSelector::userEntry()));
    rig.wavetable1.update(); // the parameter schedules this; do it now

    fcm::dsp::TockSample sample;
    sample.sampleRate = 44100.0;
    sample.samples.assign(400, 0.5f);
    REQUIRE(rig.tock.loadUser(sample, "my tock"));
}

// The same state: parameter values equal up to the float noise of the 0..1 normalisation (a value that
// went through a skewed range can come back as 0.99999994 instead of 1), every other node exactly.
bool sameState(const juce::ValueTree& a, const juce::ValueTree& b)
{
    if (a.getType() != b.getType() || a.getNumChildren() != b.getNumChildren())
        return false;
    if (static_cast<int>(a["stateVersion"]) != static_cast<int>(b["stateVersion"]))
        return false;

    for (int i = 0; i < a.getNumChildren(); ++i)
    {
        const auto x = a.getChild(i);
        const auto y = b.getChild(i);
        if (x.getType() != y.getType())
            return false;
        if (x.getType().toString() == "PARAM")
        {
            const double u = x["value"];
            const double v = y["value"];
            if (x["id"] != y["id"] || std::abs(u - v) > std::max(1.0e-4, std::abs(u) * 1.0e-6))
                return false;
        }
        else if (!x.isEquivalentTo(y))
        {
            return false;
        }
    }
    return true;
}

juce::ValueTree parseAndWrap(const juce::String& text)
{
    auto xml = juce::XmlDocument::parse(text);
    REQUIRE(xml != nullptr);
    return juce::ValueTree::fromXml(*xml);
}
} // namespace

TEST_CASE("A fresh state captures with its version and every piece that exists", "[core][project]")
{
    Rig rig;
    const auto tree = rig.project.capture();
    REQUIRE(tree.getType() == kRoot);
    REQUIRE(static_cast<int>(tree["stateVersion"]) == fcm::core::kStateVersion);
    REQUIRE(tree.getChildWithName("AmpEnvelope").isValid());
    REQUIRE(tree.getChildWithName("PitchEnvelope").isValid());
    REQUIRE(tree.getChildWithName("FxRacks").isValid());
    REQUIRE_FALSE(tree.getChildWithName("FxChain").isValid());
    // Nothing user-loaded yet.
    REQUIRE_FALSE(tree.getChildWithName("UserWavetable1").isValid());
    REQUIRE_FALSE(tree.getChildWithName("UserTockSample").isValid());
}

TEST_CASE("Everything survives a save and a load into a fresh instance", "[core][project][roundtrip]")
{
    Rig source;
    personalise(source);
    const auto saved = source.project.capture();
    REQUIRE(saved.getChildWithName("UserWavetable1").isValid());
    REQUIRE(saved.getChildWithName("UserWavetable2").isValid());
    REQUIRE(saved.getChildWithName("UserTockSample").isValid());

    // Through the XML text the host stores, then into a fresh set of pieces.
    const auto xml = source.project.toXml();
    REQUIRE(xml != nullptr);
    const auto text = xml->toString();
    const auto parsed = juce::XmlDocument::parse(text);
    REQUIRE(parsed != nullptr);

    Rig target;
    REQUIRE(target.project.restoreFromXml(*parsed) == StateLoadResult::loaded);

    REQUIRE(sameState(target.project.capture(), saved));
    REQUIRE_THAT(target.getParameter(Ids::kOutputGain), WithinAbs(-9.0, 1.0e-3));
    REQUIRE(target.getParameter(Ids::kQuality) == 3.0f);
    REQUIRE(target.amp.get() == source.amp.get());
    REQUIRE(target.amp.get().isSynced());
    REQUIRE(target.pitch.get() == source.pitch.get());
    for (const auto rack : {RackId::busA, RackId::busB, RackId::main})
        REQUIRE(target.fx.ids(rack) == source.fx.ids(rack));
    REQUIRE(target.wavetable1.userName() == "Osc One");
    REQUIRE(target.wavetable2.userName() == "Osc Two");
    REQUIRE(target.wavetable1.displayFrames() == source.wavetable1.displayFrames());
    REQUIRE(target.tock.userName() == "my tock");
    REQUIRE(target.packedMain.load() == source.packedMain.load());
    REQUIRE(target.packedBusA.load() == source.packedBusA.load());

    // A second round trip changes nothing.
    Rig again;
    REQUIRE(again.project.restore(parseAndWrap(target.project.toXml()->toString())) ==
            StateLoadResult::loaded);
    REQUIRE(sameState(again.project.capture(), saved));
}

TEST_CASE("A state loaded into a used instance replaces everything that was there", "[core][project]")
{
    Rig source;
    personalise(source);
    const auto saved = source.project.capture();

    // A target that has its own, different personalisation.
    Rig target;
    target.setParameter(Ids::kOutputGain, 6.0f);
    target.setParameter("stereo.enabled", 1.0f);
    target.fx.move(RackId::main, 0, 2);
    target.fx.add(RackId::busB, 1);
    REQUIRE(target.wavetable2.loadUser(sineFrames(2), "Other"));

    REQUIRE(target.project.restore(saved) == StateLoadResult::loaded);
    REQUIRE(sameState(target.project.capture(), saved));
    REQUIRE(target.getParameter("stereo.enabled") == target.defaultOf("stereo.enabled"));
    REQUIRE(target.wavetable2.userName() == "Osc Two");
}

TEST_CASE("Controls that a saved state does not mention take their defaults", "[core][project][missing]")
{
    Rig rig;
    personalise(rig);

    // A state that only mentions two parameters (as an older version or a small preset would).
    juce::ValueTree partial(kRoot);
    partial.setProperty("stateVersion", fcm::core::kStateVersion, nullptr);
    const auto add = [&](const char* id, double value)
    {
        juce::ValueTree entry("PARAM");
        entry.setProperty("id", id, nullptr);
        entry.setProperty("value", value, nullptr);
        partial.appendChild(entry, nullptr);
    };
    add("global.outputGain", -20.0);
    add("distortion.drive", 40.0);

    REQUIRE(rig.project.restore(partial) == StateLoadResult::loaded);
    REQUIRE_THAT(rig.getParameter("global.outputGain"), WithinAbs(-20.0, 1.0e-3));
    REQUIRE_THAT(rig.getParameter("distortion.drive"), WithinAbs(40.0, 1.0e-3));

    // Everything else is its default, whatever it was before: parameters...
    for (auto* parameter : rig.processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (withId->paramID == "global.outputGain" || withId->paramID == "distortion.drive")
            continue;
        INFO(withId->paramID);
        REQUIRE_THAT(
            rig.getParameter(withId->paramID.toStdString()),
            WithinAbs(rig.defaultOf(withId->paramID.toStdString()),
                      std::max(1.0e-4, std::abs(rig.defaultOf(withId->paramID.toStdString())) * 1.0e-6)));
    }

    // ...and the other pieces: default envelopes, registration order, no user data.
    REQUIRE(rig.amp.get() == BreakpointEnvelope(EnvelopeKind::amp));
    REQUIRE(rig.pitch.get() == BreakpointEnvelope(EnvelopeKind::pitch));
    REQUIRE(rig.fx.ids(RackId::main) == std::vector<std::string>{"distortion", "eq", "stereo"});
    REQUIRE(rig.fx.ids(RackId::busA).empty());
    REQUIRE_FALSE(rig.wavetable1.hasUser());
    REQUIRE_FALSE(rig.wavetable2.hasUser());
    REQUIRE_FALSE(rig.tock.hasUser());
}

TEST_CASE("A project from before the Tock loads with the Tock at its defaults", "[core][project][missing]")
{
    Rig source;
    personalise(source);

    // Remove everything about the Tock from the saved tree.
    auto old = source.project.capture();
    for (int i = old.getNumChildren(); --i >= 0;)
    {
        const auto child = old.getChild(i);
        if (child.getType().toString() == "UserTockSample" ||
            (child.getType().toString() == "PARAM" && child["id"].toString().startsWith("tock.")))
            old.removeChild(i, nullptr);
    }

    Rig target;
    target.setParameter(Ids::kTockOn, 1.0f);
    REQUIRE(target.project.restore(old) == StateLoadResult::loaded);
    REQUIRE(target.getParameter(Ids::kTockOn) == 0.0f);
    REQUIRE_THAT(target.getParameter(Ids::kTockLevel), WithinAbs(-12.0, 1.0e-3));
    REQUIRE_FALSE(target.tock.hasUser());
    // The rest of the project is as saved.
    REQUIRE_THAT(target.getParameter(Ids::kOutputGain), WithinAbs(-9.0, 1.0e-3));
}

TEST_CASE("A damaged node never fails the load of the rest", "[core][project][damaged]")
{
    Rig source;
    personalise(source);
    auto tree = source.project.capture();

    // An envelope with a NaN, an order of unknown IDs and a wavetable that is not base64.
    tree.getChildWithName("AmpEnvelope").getChild(0).setProperty("time", "nan", nullptr);
    tree.getChildWithName("FxRacks").getChild(2).setProperty("effects", "nothing,unknown,more", nullptr);
    tree.getChildWithName("UserWavetable1").setProperty("frames", "!!!not base64!!!", nullptr);
    tree.getChildWithName("UserTockSample").setProperty("sampleRate", 3.0, nullptr);

    Rig target;
    REQUIRE(target.project.restore(tree) == StateLoadResult::loaded);
    REQUIRE_THAT(target.getParameter(Ids::kOutputGain), WithinAbs(-9.0, 1.0e-3)); // the rest loaded
    REQUIRE(target.amp.get() == BreakpointEnvelope(EnvelopeKind::amp));           // damaged: default
    REQUIRE(target.fx.ids(RackId::main).empty()); // only unknown IDs: nothing is left of the rack
    REQUIRE_FALSE(target.wavetable1.hasUser());
    REQUIRE(target.wavetable2.hasUser()); // the intact one
    REQUIRE_FALSE(target.tock.hasUser());
    REQUIRE(target.pitch.get() == source.pitch.get());
}

TEST_CASE("Embedded audio with a false size prefix loads as no user audio",
          "[core][project][untrusted][security]")
{
    Rig source;
    personalise(source);
    const auto saved = source.project.capture();

    const std::vector<juce::String> falsePrefixes{"-1.A", "0.", "2000000000.A", "99999999999999999999.A",
                                                  "12.AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"};
    for (const auto& text : falsePrefixes)
    {
        INFO(text);
        auto tree = saved.createCopy();
        tree.getChildWithName("UserWavetable1").setProperty("frames", text, nullptr);
        tree.getChildWithName("UserTockSample").setProperty("samples", text, nullptr);

        Rig target;
        REQUIRE(target.project.restore(tree) == StateLoadResult::loaded);
        REQUIRE_FALSE(target.wavetable1.hasUser());
        REQUIRE_FALSE(target.tock.hasUser());
        REQUIRE(target.wavetable2.hasUser());                                         // the intact one
        REQUIRE_THAT(target.getParameter(Ids::kOutputGain), WithinAbs(-9.0, 1.0e-3)); // the rest loaded
    }
}

TEST_CASE("A state that is not ours changes nothing at all", "[core][project][rejected]")
{
    Rig rig;
    personalise(rig);
    const auto before = rig.project.capture();

    REQUIRE(rig.project.restore(juce::ValueTree()) == StateLoadResult::rejected);
    REQUIRE(rig.project.restore(juce::ValueTree("Something")) == StateLoadResult::rejected);
    REQUIRE(rig.project.restoreFromXml(*juce::XmlDocument::parse("<Other/>")) == StateLoadResult::rejected);
    REQUIRE(sameState(rig.project.capture(), before));
}

TEST_CASE("A newer version loads what it knows", "[core][project]")
{
    Rig source;
    personalise(source);
    auto tree = source.project.capture();
    tree.setProperty("stateVersion", fcm::core::kStateVersion + 5, nullptr);
    juce::ValueTree unknown("PARAM");
    unknown.setProperty("id", "future.control", nullptr);
    unknown.setProperty("value", 1.0, nullptr);
    tree.appendChild(unknown, nullptr);
    tree.appendChild(juce::ValueTree("FutureNode"), nullptr);

    Rig target;
    REQUIRE(target.project.restore(tree) == StateLoadResult::loadedFromNewerVersion);
    REQUIRE_THAT(target.getParameter(Ids::kOutputGain), WithinAbs(-9.0, 1.0e-3));
    // Saved again it is a state of this version.
    REQUIRE(static_cast<int>(target.project.capture()["stateVersion"]) == fcm::core::kStateVersion);
}

TEST_CASE("A version 1 project opens with its effects in the Main rack and its settings",
          "[core][project][migration]")
{
    Rig source;
    source.setParameter("distortion.enabled", 1.0f);
    source.setParameter("distortion.drive", 61.0f);
    source.setParameter("eq.enabled", 1.0f);

    // Make it look like a saved 0.1 project: version 1, the effect order in a FxChain node, no racks.
    auto old = source.project.capture();
    old.setProperty("stateVersion", 1, nullptr);
    old.removeChild(old.getChildWithName("FxRacks"), nullptr);
    juce::ValueTree chain("FxChain");
    chain.setProperty("order", "stereo,eq,distortion", nullptr);
    old.appendChild(chain, nullptr);

    Rig target;
    target.fx.add(RackId::busA, 1); // something that must not survive the load
    REQUIRE(target.project.restore(old) == StateLoadResult::loaded);

    REQUIRE(target.fx.ids(RackId::main) == std::vector<std::string>{"stereo", "eq", "distortion"});
    REQUIRE(target.fx.ids(RackId::busA).empty());
    REQUIRE(target.fx.ids(RackId::busB).empty());
    REQUIRE_THAT(target.getParameter("distortion.drive"), WithinAbs(61.0, 1.0e-3));
    REQUIRE(target.getParameter("eq.enabled") == 1.0f);
    REQUIRE(target.getParameter("eq2.enabled") == 0.0f); // the new copies start at their defaults

    // Saving it again writes version 2 with the racks and no FxChain.
    const auto saved = target.project.capture();
    REQUIRE(static_cast<int>(saved["stateVersion"]) == 2);
    REQUIRE(saved.getChildWithName("FxRacks").isValid());
    REQUIRE_FALSE(saved.getChildWithName("FxChain").isValid());
}

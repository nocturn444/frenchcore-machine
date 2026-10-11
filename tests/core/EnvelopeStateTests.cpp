// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EnvelopeState.h"
#include "dsp/envelope/EnvelopePresets.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_events/juce_events.h>

using Catch::Matchers::WithinAbs;
using fcm::core::EnvelopeState;
using fcm::dsp::AmpKnob;
using fcm::dsp::BreakpointEnvelope;
using fcm::dsp::EnvelopeKind;
using fcm::dsp::EnvelopePresets;

TEST_CASE("EnvelopeState starts on the default shape and publishes it", "[core][envelope]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    for (auto kind : {EnvelopeKind::amp, EnvelopeKind::pitch})
    {
        EnvelopeState state(kind);
        REQUIRE(state.get() == BreakpointEnvelope(kind));

        const auto* shape = state.source().acquire();
        REQUIRE(shape != nullptr);
        REQUIRE(shape->count == state.get().size());
    }
}

TEST_CASE("Editing publishes the new shape to the audio side", "[core][envelope]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EnvelopeState state(EnvelopeKind::amp);

    state.edit([](BreakpointEnvelope& envelope) { envelope.setKnob(AmpKnob::sustain, 0.25f); });
    REQUIRE_THAT(state.get().knob(AmpKnob::sustain), WithinAbs(0.25, 1.0e-6));
    REQUIRE_THAT(state.source().acquire()->lastValue(), WithinAbs(0.25, 1.0e-6));

    state.set(EnvelopePresets::all(EnvelopeKind::amp)[1].envelope);
    REQUIRE(state.get() == EnvelopePresets::all(EnvelopeKind::amp)[1].envelope);
    REQUIRE(state.source().acquire()->count == state.get().size());
}

TEST_CASE("The release time is available to any thread as seconds", "[core][envelope]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EnvelopeState state(EnvelopeKind::amp);
    REQUIRE_THAT(state.getReleaseSeconds(), WithinAbs(0.03, 1.0e-6));

    state.edit([](BreakpointEnvelope& envelope) { envelope.setReleaseMs(250.0f); });
    REQUIRE_THAT(state.getReleaseSeconds(), WithinAbs(0.25, 1.0e-6));
}

TEST_CASE("Listeners are told about changes", "[core][envelope]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    struct Counter : juce::ChangeListener
    {
        void changeListenerCallback(juce::ChangeBroadcaster*) override { ++calls; }
        int calls = 0;
    } counter;

    EnvelopeState state(EnvelopeKind::pitch);
    state.addChangeListener(&counter);
    state.edit([](BreakpointEnvelope& envelope) { envelope.setKnob(fcm::dsp::PitchKnob::end, 0.5f); });

    // Notifications are asynchronous; deliver the pending one now.
    state.dispatchPendingMessages();
    REQUIRE(counter.calls >= 1);
    state.removeChangeListener(&counter);
}

TEST_CASE("Every preset survives a round trip through the saved form and through XML",
          "[core][envelope][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (auto kind : {EnvelopeKind::amp, EnvelopeKind::pitch})
        for (const auto& preset : EnvelopePresets::all(kind))
        {
            INFO(preset.name);
            EnvelopeState source(kind);
            source.set(preset.envelope);

            // Through the ValueTree...
            EnvelopeState direct(kind);
            REQUIRE(direct.restoreFrom(source.toValueTree()));
            REQUIRE(direct.get() == preset.envelope);

            // ...and through the text a host would store.
            const auto xml = source.toValueTree().createXml();
            REQUIRE(xml != nullptr);
            EnvelopeState viaXml(kind);
            REQUIRE(viaXml.restoreFrom(juce::ValueTree::fromXml(xml->toString())));
            REQUIRE(viaXml.get() == preset.envelope);
        }
}

TEST_CASE("Edited shapes round trip exactly", "[core][envelope][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EnvelopeState source(EnvelopeKind::amp);
    source.edit(
        [](BreakpointEnvelope& envelope)
        {
            envelope.addPoint(10.0f, 0.93f);
            envelope.addPoint(60.0f, 0.123456f);
            envelope.setCurve(2, -0.37f);
            envelope.setKnob(AmpKnob::decay, 217.5f);
            envelope.setReleaseMs(77.7f);
        });

    EnvelopeState restored(EnvelopeKind::amp);
    REQUIRE(restored.restoreFrom(juce::ValueTree::fromXml(source.toValueTree().createXml()->toString())));
    REQUIRE(restored.get() == source.get());
}

TEST_CASE("A missing or damaged node gives the default shape", "[core][envelope][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const BreakpointEnvelope defaultAmp(EnvelopeKind::amp);

    auto stateWith = [&](auto damage)
    {
        EnvelopeState state(EnvelopeKind::amp);
        state.edit([](BreakpointEnvelope& envelope)
                   { envelope.setKnob(AmpKnob::sustain, 0.1f); }); // not the default
        auto node = EnvelopeState(EnvelopeKind::amp).toValueTree();
        damage(node);
        const bool accepted = state.restoreFrom(node);
        return std::pair{accepted, state.get() == defaultAmp};
    };

    // Not damaged: accepted.
    REQUIRE(stateWith([](juce::ValueTree&) {}).first);

    for (auto damage :
         std::vector<std::function<void(juce::ValueTree&)>>{
             [](juce::ValueTree& n) { n.removeChild(0, nullptr); }, // too few points / no start
             [](juce::ValueTree& n) { n.setProperty("releaseMs", 0.0, nullptr); }, // bad release
             [](juce::ValueTree& n) { n.removeProperty("releaseMs", nullptr); },
             [](juce::ValueTree& n) { n.getChild(1).setProperty("time", -5.0, nullptr); }, // out of order
             [](juce::ValueTree& n) { n.getChild(2).setProperty("value", 4.0, nullptr); }, // out of range
             [](juce::ValueTree& n) { n.getChild(2).removeProperty("curve", nullptr); },
             [](juce::ValueTree& n) { n.getChild(1).setProperty("time", std::nan(""), nullptr); },
             [](juce::ValueTree& n)
             { n.getChild(3).setProperty("main", true, nullptr); }, // wrong number of main points
             [](juce::ValueTree& n) { n.appendChild(juce::ValueTree("Other"), nullptr); },
             [](juce::ValueTree& n)
             {
                 for (int i = 0; i < 40; ++i)
                     n.appendChild(n.getChild(1).createCopy(), nullptr);
             }})
    {
        const auto [accepted, isDefault] = stateWith(damage);
        REQUIRE_FALSE(accepted);
        REQUIRE(isDefault);
    }

    // No node at all, or one of the wrong kind.
    EnvelopeState state(EnvelopeKind::amp);
    REQUIRE_FALSE(state.restoreFrom(juce::ValueTree()));
    REQUIRE_FALSE(state.restoreFrom(EnvelopeState(EnvelopeKind::pitch).toValueTree()));
    REQUIRE(state.get() == defaultAmp);
}

TEST_CASE("The saved node has the documented shape", "[core][envelope][state]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto amp = EnvelopeState(EnvelopeKind::amp).toValueTree();
    const auto pitch = EnvelopeState(EnvelopeKind::pitch).toValueTree();

    REQUIRE(amp.getType() == juce::Identifier("AmpEnvelope"));
    REQUIRE(pitch.getType() == juce::Identifier("PitchEnvelope"));
    REQUIRE(amp.getNumChildren() == 5);
    REQUIRE(amp.getChild(0).getType() == juce::Identifier("Point"));
    REQUIRE(amp.hasProperty("releaseMs"));
    for (const char* property : {"time", "value", "curve", "main"})
        REQUIRE(amp.getChild(0).hasProperty(property));
}

TEST_CASE("The sync tempo is saved with the shape and restored", "[core][envelope][state][sync]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EnvelopeState source(EnvelopeKind::amp);
    source.edit(
        [](BreakpointEnvelope& envelope)
        {
            envelope.enableSync(173.0f);
            envelope.setKnob(AmpKnob::hold, 40.0f);
        });

    const auto node = source.toValueTree();
    REQUIRE(node.hasProperty("syncBpm"));

    EnvelopeState restored(EnvelopeKind::amp);
    REQUIRE(restored.restoreFrom(juce::ValueTree::fromXml(node.createXml()->toString())));
    REQUIRE(restored.get() == source.get());
    REQUIRE(restored.get().isSynced());
    REQUIRE_THAT(restored.get().syncRefBpm(), WithinAbs(173.0, 1.0e-4));
    REQUIRE_THAT(restored.source().acquire()->syncRefBpm, WithinAbs(173.0, 1.0e-4)); // and published
}

TEST_CASE("An unsynced shape has no sync property, and an old node without it loads unsynced",
          "[core][envelope][state][sync]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto node = EnvelopeState(EnvelopeKind::amp).toValueTree();
    REQUIRE_FALSE(node.hasProperty("syncBpm"));

    EnvelopeState state(EnvelopeKind::amp);
    state.edit([](BreakpointEnvelope& envelope) { envelope.enableSync(150.0f); });
    REQUIRE(state.restoreFrom(node)); // a node saved before sync existed
    REQUIRE_FALSE(state.get().isSynced());
}

TEST_CASE("A damaged sync tempo gives the default shape", "[core][envelope][state][sync]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    for (double bad : {-5.0, 3.0, 1000.0, std::nan("")})
    {
        auto node = EnvelopeState(EnvelopeKind::amp).toValueTree();
        node.setProperty("syncBpm", bad, nullptr);

        EnvelopeState state(EnvelopeKind::amp);
        state.edit([](BreakpointEnvelope& envelope) { envelope.setKnob(AmpKnob::sustain, 0.1f); });
        REQUIRE_FALSE(state.restoreFrom(node));
        REQUIRE(state.get() == BreakpointEnvelope(EnvelopeKind::amp));
    }
}

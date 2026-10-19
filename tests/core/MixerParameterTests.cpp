// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/EngineParameterReader.h"
#include "core/ParameterIds.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>

using Catch::Matchers::WithinAbs;
namespace Ids = fcm::core::ParameterIds;

namespace
{
void setValue(juce::AudioProcessorValueTreeState& state, const std::string& id, float value)
{
    auto* parameter = state.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
} // namespace

TEST_CASE("The mixer and bus parameters have the documented ranges and defaults", "[core][mixer][layout]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;

    for (const auto& source : Ids::kMixerSources)
    {
        auto* destination = dynamic_cast<juce::AudioParameterChoice*>(
            processor.state.getParameter(Ids::mixerDestination(source)));
        REQUIRE(destination != nullptr);
        REQUIRE(destination->choices == juce::StringArray{"Main", "Bus A", "Bus B"});
        REQUIRE(destination->getIndex() == 0);
        REQUIRE_FALSE(destination->isAutomatable()); // the routing must not jump with automation

        for (const auto& id : {Ids::mixerMute(source), Ids::mixerSolo(source)})
        {
            auto* toggle = dynamic_cast<juce::AudioParameterBool*>(processor.state.getParameter(id));
            REQUIRE(toggle != nullptr);
            REQUIRE_FALSE(toggle->get());
            REQUIRE(toggle->isAutomatable());
        }

        // The mixer's Level and the dimming switch are the source's own parameters.
        REQUIRE(processor.state.getParameter(source.level) != nullptr);
        REQUIRE(processor.state.getParameter(source.on) != nullptr);
    }

    for (int bus = 0; bus < 2; ++bus)
    {
        auto* level =
            dynamic_cast<juce::AudioParameterFloat*>(processor.state.getParameter(Ids::busLevel(bus)));
        REQUIRE(level != nullptr);
        REQUIRE_THAT(level->getNormalisableRange().start, WithinAbs(-48.0, 1.0e-6));
        REQUIRE_THAT(level->getNormalisableRange().end, WithinAbs(12.0, 1.0e-6));
        REQUIRE_THAT(level->get(), WithinAbs(0.0, 1.0e-6));

        auto* region =
            dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter(Ids::busRegion(bus)));
        REQUIRE(region != nullptr);
        REQUIRE(region->choices == juce::StringArray{"Whole", "Punch", "Tail"});
        REQUIRE(region->getIndex() == 0);
    }
}

TEST_CASE("The reader turns the mixer parameters into engine parameters", "[core][mixer][reader]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    fcm::core::EngineParameterReader reader(processor.state);

    // Defaults: everything to Main, nothing muted, buses at unity and Whole.
    auto read = reader.read();
    for (const auto& source : read.mixer)
    {
        REQUIRE(source.destination == fcm::dsp::Destination::main);
        REQUIRE_FALSE(source.mute);
        REQUIRE_FALSE(source.solo);
    }
    for (const auto& bus : read.buses)
    {
        REQUIRE_THAT(bus.level, WithinAbs(1.0, 1.0e-6));
        REQUIRE(bus.region == fcm::dsp::BusRegion::whole);
    }

    setValue(processor.state, "mixer.osc2.dest", 1.0f);
    setValue(processor.state, "mixer.tock.dest", 2.0f);
    setValue(processor.state, "mixer.osc1.mute", 1.0f);
    setValue(processor.state, "mixer.tock.solo", 1.0f);
    setValue(processor.state, "bus.a.level", -6.0f);
    setValue(processor.state, "bus.a.region", 2.0f);
    setValue(processor.state, "bus.b.level", -48.0f);
    setValue(processor.state, "bus.b.region", 1.0f);
    read = reader.read();

    REQUIRE(read.mixer[fcm::dsp::stemOsc2].destination == fcm::dsp::Destination::busA);
    REQUIRE(read.mixer[fcm::dsp::stemTock].destination == fcm::dsp::Destination::busB);
    REQUIRE(read.mixer[fcm::dsp::stemOsc1].mute);
    REQUIRE(read.mixer[fcm::dsp::stemTock].solo);
    REQUIRE_THAT(read.buses[0].level, WithinAbs(0.5012, 1.0e-3));
    REQUIRE(read.buses[0].region == fcm::dsp::BusRegion::tail);
    REQUIRE(read.buses[1].level == 0.0f); // -48 dB is silence
    REQUIRE(read.buses[1].region == fcm::dsp::BusRegion::punch);
}

TEST_CASE("Mixer values that are not usable numbers fall back to the defaults", "[core][mixer][reader]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    fcm::core::EngineParameterReader reader(processor.state);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const char* id : {"mixer.osc1.dest", "mixer.osc1.mute", "mixer.osc1.solo", "bus.a.level",
                           "bus.a.region", "bus.b.level"})
        for (const float bad : {nan, inf, -inf, 99.0f, -7.0f})
        {
            processor.state.getRawParameterValue(id)->store(bad);
            const auto read = reader.read();
            INFO(id << " " << bad);
            const int destination = static_cast<int>(read.mixer[fcm::dsp::stemOsc1].destination);
            REQUIRE(destination >= 0);
            REQUIRE(destination < fcm::dsp::kNumDestinations);
            for (const auto& bus : read.buses)
            {
                REQUIRE(std::isfinite(bus.level));
                REQUIRE(bus.level >= 0.0f);
                REQUIRE(static_cast<int>(bus.region) >= 0);
                REQUIRE(static_cast<int>(bus.region) <= 2);
            }
            processor.state.getRawParameterValue(id)->store(processor.state.getParameter(id)->convertFrom0to1(
                processor.state.getParameter(id)->getDefaultValue()));
        }
}

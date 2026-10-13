// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/ParameterIds.h"
#include "core/TockSelector.h"
#include "dsp/tock/FactoryTocks.h"

#include <catch2/catch_test_macros.hpp>
#include <limits>

using fcm::core::TockSelector;
using fcm::dsp::FactoryTocks;

namespace
{
void setEntry(juce::AudioProcessorValueTreeState& state, int entry)
{
    auto* parameter = state.getParameter(fcm::core::ParameterIds::kTockSample);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(entry)));
}
} // namespace

TEST_CASE("The Tock parameters are choices that list the factory samples in order", "[core][tock]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;

    auto* sample = dynamic_cast<juce::AudioParameterChoice*>(
        processor.state.getParameter(fcm::core::ParameterIds::kTockSample));
    REQUIRE(sample != nullptr);
    // The factory samples in order, then the user sample entry.
    REQUIRE(sample->choices.size() == FactoryTocks::count() + 1);
    for (int i = 0; i < FactoryTocks::count(); ++i)
        REQUIRE(sample->choices[i].toStdString() == FactoryTocks::name(i));
    REQUIRE(sample->choices[FactoryTocks::count()].toStdString() == "User");

    auto* mode = dynamic_cast<juce::AudioParameterChoice*>(
        processor.state.getParameter(fcm::core::ParameterIds::kTockMode));
    REQUIRE(mode != nullptr);
    REQUIRE(mode->choices.size() == 2);
    REQUIRE(mode->choices[0].toStdString() == "Synth");
    REQUIRE(mode->choices[1].toStdString() == "Sample");
}

TEST_CASE("The Tock selector publishes the current sample and follows the parameter", "[core][tock]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    TockSelector selector(processor.state);

    REQUIRE(selector.currentEntry() == 0);
    const auto* first = selector.source().acquire();
    REQUIRE(first != nullptr);
    REQUIRE(first->samples == FactoryTocks::build(0)->samples);

    setEntry(processor.state, 2);
    selector.update();
    REQUIRE(selector.currentEntry() == 2);
    REQUIRE(selector.source().acquire()->samples == FactoryTocks::build(2)->samples);
}

TEST_CASE("An unusable Tock sample value selects a valid sample", "[core][tock]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    TockSelector selector(processor.state);

    auto* raw = processor.state.getRawParameterValue(fcm::core::ParameterIds::kTockSample);
    for (const float bad : {-5.0f, 99.0f, std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
    {
        raw->store(bad);
        selector.update();
        REQUIRE(selector.currentEntry() >= 0);
        REQUIRE(selector.currentEntry() <= TockSelector::userEntry());
        REQUIRE(selector.source().acquire() != nullptr);
    }
}

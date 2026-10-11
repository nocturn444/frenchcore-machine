// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/ParameterIds.h"
#include "core/WavetableSelector.h"
#include "dsp/oscillator/FactoryWavetables.h"

#include <catch2/catch_test_macros.hpp>

using fcm::core::WavetableSelector;
using fcm::dsp::FactoryWavetables;

namespace
{
void setEntry(juce::AudioProcessorValueTreeState& state, int oscillator, int entry)
{
    auto* parameter = state.getParameter(fcm::core::ParameterIds::oscillator(oscillator, "wavetable"));
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(entry)));
}
} // namespace

TEST_CASE("The wavetable parameters are choices that list the factory tables in order", "[core][wavetable]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;

    for (int oscillator = 0; oscillator < 2; ++oscillator)
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.state.getParameter(fcm::core::ParameterIds::oscillator(oscillator, "wavetable")));
        REQUIRE(choice != nullptr);
        // The factory tables in order, then the user wavetable entry.
        REQUIRE(choice->choices.size() == FactoryWavetables::count() + 1);
        REQUIRE(WavetableSelector::userEntry() == FactoryWavetables::count());
        REQUIRE(choice->choices[FactoryWavetables::count()].toStdString() == "User");
        REQUIRE(choice->getIndex() == 0);
        for (int i = 0; i < FactoryWavetables::count(); ++i)
            REQUIRE(choice->choices[i].toStdString() == FactoryWavetables::name(i));
    }
}

TEST_CASE("The selector publishes the current table at construction", "[core][wavetable]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;

    WavetableSelector selector(processor.state, 0);
    REQUIRE(selector.currentEntry() == 0);
    const auto* table = selector.source().acquire();
    REQUIRE(table != nullptr);
    REQUIRE(table->numFrames() == 1);
}

TEST_CASE("Changing the parameter publishes the table of the new entry", "[core][wavetable]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    WavetableSelector selector(processor.state, 1);

    setEntry(processor.state, 1, 8);
    selector.update();
    REQUIRE(selector.currentEntry() == 8);
    REQUIRE(selector.source().acquire()->numFrames() == 16);

    // The other oscillator's parameter does not matter to this selector.
    setEntry(processor.state, 0, 3);
    selector.update();
    REQUIRE(selector.currentEntry() == 8);
}

TEST_CASE("An unusable parameter value selects a valid table", "[core][wavetable]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    WavetableSelector selector(processor.state, 0);

    // The raw value is written directly, as a damaged project would leave it.
    auto* raw = processor.state.getRawParameterValue(fcm::core::ParameterIds::oscillator(0, "wavetable"));
    for (const float bad :
         {-5.0f, 99.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        raw->store(bad);
        selector.update();
        REQUIRE(selector.currentEntry() >= 0);
        REQUIRE(selector.currentEntry() <= WavetableSelector::userEntry());
        REQUIRE(selector.source().acquire() != nullptr);
    }
}

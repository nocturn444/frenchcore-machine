// SPDX-License-Identifier: AGPL-3.0-only
#include "TestProcessor.h"
#include "core/EqResponse.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>

using Catch::Matchers::WithinAbs;
using namespace fcm::core;

TEST_CASE("The display axes map the ends and the middle", "[core][eq][display]")
{
    REQUIRE_THAT(eqFrequencyToUnit(20.0), WithinAbs(0.0, 1.0e-12));
    REQUIRE_THAT(eqFrequencyToUnit(20000.0), WithinAbs(1.0, 1.0e-12));
    REQUIRE_THAT(eqFrequencyToUnit(632.455532), WithinAbs(0.5, 1.0e-6)); // the geometric middle
    REQUIRE_THAT(eqGainToUnit(-18.0), WithinAbs(0.0, 1.0e-12));
    REQUIRE_THAT(eqGainToUnit(0.0), WithinAbs(0.5, 1.0e-12));
    REQUIRE_THAT(eqGainToUnit(18.0), WithinAbs(1.0, 1.0e-12));

    // Out of range is clamped, and the two directions are inverses inside the range.
    REQUIRE(eqFrequencyToUnit(5.0) == 0.0);
    REQUIRE(eqFrequencyToUnit(90000.0) == 1.0);
    REQUIRE(eqGainToUnit(99.0) == 1.0);
    for (const double hz : {20.0, 55.0, 440.0, 3200.0, 20000.0})
        REQUIRE_THAT(eqUnitToFrequency(eqFrequencyToUnit(hz)), WithinAbs(hz, hz * 1.0e-9));
    for (const double db : {-18.0, -4.5, 0.0, 7.0, 18.0})
        REQUIRE_THAT(eqUnitToGain(eqGainToUnit(db)), WithinAbs(db, 1.0e-9));
}

TEST_CASE("The display axes never produce a non-finite number", "[core][eq][display]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double bad : {nan, inf, -inf})
    {
        REQUIRE(std::isfinite(eqFrequencyToUnit(bad)));
        REQUIRE(std::isfinite(eqUnitToFrequency(bad)));
        REQUIRE(std::isfinite(eqGainToUnit(bad)));
        REQUIRE(std::isfinite(eqUnitToGain(bad)));
    }
}

TEST_CASE("The bands are read from the parameters", "[core][eq][display]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;

    // Defaults: a flat EQ.
    auto bands = readEqBands(processor.state);
    REQUIRE(bands[0].type == 1); // low shelf
    REQUIRE(bands[1].type == 0);
    REQUIRE(bands[3].type == 2); // high shelf
    REQUIRE_THAT(bands[1].freq, WithinAbs(400.0, 1.0e-3));
    for (const auto& band : bands)
    {
        REQUIRE(band.on);
        REQUIRE_FALSE(band.isActive()); // no gain, no cut
        REQUIRE_THAT(eqBandResponseDb(band, 1000.0), WithinAbs(0.0, 1.0e-12));
    }
    REQUIRE_THAT(eqResponseDb(bands, 100.0), WithinAbs(0.0, 1.0e-12));

    const auto set = [&](const char* id, float value)
    {
        auto* parameter = processor.state.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    set("eq.b2.gain", 12.0f);
    set("eq.b2.freq", 1000.0f);
    set("eq.b2.q", 1.0f);
    set("eq.b3.type", 3.0f); // low cut
    set("eq.b3.freq", 100.0f);

    bands = readEqBands(processor.state);
    REQUIRE(bands[1].isActive());
    REQUIRE(bands[2].isActive());
    REQUIRE(bands[2].isCut());
    REQUIRE_THAT(eqBandResponseDb(bands[1], 1000.0), WithinAbs(12.0, 0.2)); // the full gain at the centre
    REQUIRE(eqBandResponseDb(bands[2], 25.0) < -20.0);
    REQUIRE_THAT(eqResponseDb(bands, 1000.0), WithinAbs(12.0, 0.4));

    // A band that is off does not count, even a cut.
    set("eq.b3.on", 0.0f);
    bands = readEqBands(processor.state);
    REQUIRE_FALSE(bands[2].isActive());
    REQUIRE_THAT(eqBandResponseDb(bands[2], 25.0), WithinAbs(0.0, 1.0e-12));
}

TEST_CASE("Extreme bands are read with their own gain and Q and widen the display",
          "[core][eq][display][extreme]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    const auto set = [&](const char* id, float value)
    {
        auto* parameter = processor.state.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    auto bands = readEqBands(processor.state);
    REQUIRE(eqDisplayRangeDb(bands) == kEqRangeDb);

    set("eq.b2.gain", 6.0f);
    set("eq.b2.q", 2.0f);
    set("eq.b2.xgain", 30.0f);
    set("eq.b2.xq", 40.0f);
    bands = readEqBands(processor.state);
    REQUIRE_FALSE(bands[1].extreme);
    REQUIRE_THAT(bands[1].gain, WithinAbs(6.0, 1.0e-3));
    REQUIRE_THAT(bands[1].q, WithinAbs(2.0, 1.0e-3));

    set("eq.b2.extreme", 1.0f);
    bands = readEqBands(processor.state);
    REQUIRE(bands[1].extreme);
    REQUIRE_THAT(bands[1].gain, WithinAbs(30.0, 1.0e-3));
    REQUIRE_THAT(bands[1].q, WithinAbs(40.0, 0.05));
    REQUIRE(eqDisplayRangeDb(bands) == kEqExtremeRangeDb);

    // The gain axis follows the range: 30 dB is near the top of the 36 dB display.
    REQUIRE_THAT(eqGainToUnit(36.0, kEqExtremeRangeDb), WithinAbs(1.0, 1.0e-12));
    REQUIRE_THAT(eqUnitToGain(eqGainToUnit(30.0, kEqExtremeRangeDb), kEqExtremeRangeDb),
                 WithinAbs(30.0, 1.0e-9));
}

TEST_CASE("Key Track moves the displayed band with the note", "[core][eq][display][keytrack]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fcm::test::TestProcessor processor;
    const auto set = [&](const char* id, float value)
    {
        auto* parameter = processor.state.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    set("eq.b2.gain", 12.0f);
    set("eq.b2.freq", 500.0f);
    set("eq.b2.q", 4.0f);
    set("eq.b2.keytrack", 100.0f);

    const auto bands = readEqBands(processor.state);
    REQUIRE_THAT(bands[1].frequencyAt(0.0), WithinAbs(500.0, 1.0e-9));
    REQUIRE_THAT(bands[1].frequencyAt(12.0), WithinAbs(1000.0, 1.0e-6));

    // The curve has its peak where the band is for that note.
    REQUIRE_THAT(eqResponseDb(bands, 1000.0, 12.0), WithinAbs(12.0, 0.4));
    REQUIRE(eqResponseDb(bands, 500.0, 12.0) < 4.0);
    REQUIRE_THAT(eqResponseDb(bands, 500.0, 0.0), WithinAbs(12.0, 0.4));
}

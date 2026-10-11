// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/MusicalTime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>

using namespace fcm::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("The figure table is sorted and has the expected lengths", "[dsp][sync]")
{
    for (size_t i = 1; i < kMusicalFigures.size(); ++i)
        REQUIRE(kMusicalFigures[i].beats > kMusicalFigures[i - 1].beats);

    auto beatsOf = [](const std::string& name)
    {
        for (const auto& figure : kMusicalFigures)
            if (name == figure.name)
                return figure.beats;
        return -1.0;
    };

    REQUIRE_THAT(beatsOf("1/4"), WithinAbs(1.0, 1.0e-12));
    REQUIRE_THAT(beatsOf("1/16"), WithinAbs(0.25, 1.0e-12));
    REQUIRE_THAT(beatsOf("1 bar"), WithinAbs(4.0, 1.0e-12));
    // Triplets are 2/3 of the straight value, dotted ones 3/2.
    REQUIRE_THAT(beatsOf("1/8T"), WithinAbs(beatsOf("1/8") * 2.0 / 3.0, 1.0e-12));
    REQUIRE_THAT(beatsOf("1/16T"), WithinAbs(beatsOf("1/16") * 2.0 / 3.0, 1.0e-12));
    REQUIRE_THAT(beatsOf("1/8D"), WithinAbs(beatsOf("1/8") * 1.5, 1.0e-12));
    REQUIRE_THAT(beatsOf("1/4D"), WithinAbs(beatsOf("1/4") * 1.5, 1.0e-12));
    REQUIRE_THAT(beatsOf("1/2T"), WithinAbs(beatsOf("1/2") * 2.0 / 3.0, 1.0e-12));
}

TEST_CASE("Beats and milliseconds convert both ways", "[dsp][sync]")
{
    // 120 BPM: a beat is 500 ms. 200 BPM: 300 ms.
    REQUIRE_THAT(beatsToMs(1.0, 120.0), WithinAbs(500.0, 1.0e-9));
    REQUIRE_THAT(beatsToMs(0.25, 200.0), WithinAbs(75.0, 1.0e-9));
    for (double bpm : {40.0, 120.0, 200.0, 300.0})
        REQUIRE_THAT(msToBeats(beatsToMs(0.375, bpm), bpm), WithinAbs(0.375, 1.0e-9));
}

TEST_CASE("timeChoices lists the figures that fit the range", "[dsp][sync]")
{
    // A knob that can be zero starts with "0".
    const auto hold = timeChoices(0.0f, 1000.0f, 200.0); // beat = 300 ms
    REQUIRE(hold.front().label == "0");
    REQUIRE(juce::exactlyEqual(hold.front().ms, 0.0f));

    // A knob that cannot be zero starts with its shortest time, called Min.
    const auto release = timeChoices(1.0f, 2000.0f, 200.0);
    REQUIRE(release.front().label == "Min");
    REQUIRE_THAT(release.front().ms, WithinAbs(1.0, 1.0e-6));

    // Sorted, within the range, with the right milliseconds at 200 BPM.
    for (const auto* choices : {&hold, &release})
        for (size_t i = 1; i < choices->size(); ++i)
        {
            REQUIRE((*choices)[i].ms > (*choices)[i - 1].ms);
            REQUIRE((*choices)[i].ms <= 2000.0f);
        }
    for (const auto& choice : hold)
        if (choice.label == "1/16")
            REQUIRE_THAT(choice.ms, WithinAbs(75.0, 1.0e-3));
    for (const auto& choice : hold)
        if (choice.label == "1/4")
            REQUIRE_THAT(choice.ms, WithinAbs(300.0, 1.0e-3));

    // Figures longer than the knob's range are left out: at 200 BPM a bar is 1200 ms.
    const auto attack = timeChoices(0.0f, 500.0f, 200.0);
    for (const auto& choice : attack)
        REQUIRE(choice.ms <= 500.0f);
    REQUIRE(attack.size() < hold.size());

    // At a slow tempo a bar does not fit in 2000 ms.
    const auto slow = timeChoices(1.0f, 2000.0f, 60.0); // beat = 1000 ms, bar = 4000 ms
    REQUIRE(slow.back().label != "1 bar");

    // Very short figures are dropped when they are below the minimum.
    const auto decay = timeChoices(1.0f, 1000.0f, 400.0); // 1/64 = 9.4 ms, still above 1 ms
    REQUIRE(decay.size() > 2);
}

TEST_CASE("nearestChoice finds the closest step", "[dsp][sync]")
{
    const auto choices = timeChoices(0.0f, 1000.0f, 200.0);
    REQUIRE(choices[static_cast<size_t>(nearestChoice(choices, 0.0f))].label == "0");
    REQUIRE(choices[static_cast<size_t>(nearestChoice(choices, 1.0f))].label ==
            "0"); // 1 ms is nearer to 0 than to 1/64
    REQUIRE(choices[static_cast<size_t>(nearestChoice(choices, 74.0f))].label == "1/16");
    REQUIRE(choices[static_cast<size_t>(nearestChoice(choices, 300.0f))].label == "1/4");
    REQUIRE(juce::exactlyEqual(choices[static_cast<size_t>(nearestChoice(choices, 5000.0f))].ms,
                               choices.back().ms));
}

TEST_CASE("formatBarFraction writes grid positions as fractions of a bar", "[dsp][sync]")
{
    REQUIRE(formatBarFraction(0.0) == "0");
    REQUIRE(formatBarFraction(0.0625) == "1/64");
    REQUIRE(formatBarFraction(0.125) == "1/32");
    REQUIRE(formatBarFraction(0.25) == "1/16");
    REQUIRE(formatBarFraction(0.5) == "1/8");
    REQUIRE(formatBarFraction(0.75) == "3/16");
    REQUIRE(formatBarFraction(1.0) == "1/4");
    REQUIRE(formatBarFraction(2.0) == "1/2");
    REQUIRE(formatBarFraction(3.0) == "3/4");
    REQUIRE(formatBarFraction(4.0) == "1");
    REQUIRE(formatBarFraction(8.0) == "2");
    REQUIRE(formatBarFraction(-1.0) == "0");
}

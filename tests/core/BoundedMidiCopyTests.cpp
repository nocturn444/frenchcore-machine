// SPDX-License-Identifier: AGPL-3.0-only
#include "core/BoundedMidiCopy.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace fcm::core;

namespace
{
juce::MidiBuffer makeBurst(int numEvents, int numSamples)
{
    juce::MidiBuffer burst;
    for (int i = 0; i < numEvents; ++i)
        burst.addEvent(juce::MidiMessage::noteOn(1, 36 + (i % 48), static_cast<juce::uint8>(1 + (i % 127))),
                       i % numSamples);
    return burst;
}

int countEvents(const juce::MidiBuffer& buffer)
{
    int n = 0;
    for (const auto metadata : buffer)
    {
        juce::ignoreUnused(metadata);
        ++n;
    }
    return n;
}
} // namespace

TEST_CASE("A small MIDI block is copied completely", "[core][midi]")
{
    const auto source = makeBurst(20, 512);
    juce::MidiBuffer destination;
    destination.ensureSize(kAuditionMidiBytes);

    const auto result = copyMidiWithinBudget(source, destination, 512, kHostMidiBudgetBytes);

    REQUIRE(result.copied == 20);
    REQUIRE(result.dropped == 0);
    REQUIRE(countEvents(destination) == 20);
}

TEST_CASE("Events outside the block are not copied", "[core][midi]")
{
    juce::MidiBuffer source;
    source.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8)100), 10);
    source.addEvent(juce::MidiMessage::noteOn(1, 41, (juce::uint8)100), 512);
    juce::MidiBuffer destination;
    destination.ensureSize(kAuditionMidiBytes);

    const auto result = copyMidiWithinBudget(source, destination, 512, kHostMidiBudgetBytes);

    REQUIRE(result.copied == 1);
    REQUIRE(countEvents(destination) == 1);
}

TEST_CASE("An oversized MIDI burst is capped to the budget", "[core][midi][realtime]")
{
    constexpr int kBlock = 512;
    const auto burst = makeBurst(100000, kBlock);

    juce::MidiBuffer destination;
    destination.ensureSize(kAuditionMidiBytes);

    const auto result = copyMidiWithinBudget(burst, destination, kBlock, kHostMidiBudgetBytes);

    REQUIRE(result.copied > 0);
    REQUIRE(result.dropped > 0);
    REQUIRE(result.copied + result.dropped == 100000);
    // Three-byte events: the stored size stays under the budget, which is under the reserved room,
    // so the buffer never has to grow (JUCE stores 4 + 2 bytes of header per event, see
    // kMidiEventOverheadBytes).
    REQUIRE(result.copied * (kMidiEventOverheadBytes + 3) <= kHostMidiBudgetBytes);
    static_assert(kHostMidiBudgetBytes < kAuditionMidiBytes);
    REQUIRE(countEvents(destination) == result.copied);
}

TEST_CASE("Room is left for the audition events after a full-budget host burst", "[core][midi][realtime]")
{
    constexpr int kBlock = 512;
    const auto burst = makeBurst(100000, kBlock);

    juce::MidiBuffer destination;
    destination.ensureSize(kAuditionMidiBytes);

    const auto result = copyMidiWithinBudget(burst, destination, kBlock, kHostMidiBudgetBytes);
    // The audition queue holds 64 events, plus a few from the auto-play sequencer.
    constexpr int kAuditionEvents = 80;
    for (int i = 0; i < kAuditionEvents; ++i)
        destination.addEvent(juce::MidiMessage::noteOff(1, 36 + (i % 48)), 0);

    const int worstCaseBytes =
        result.copied * (kMidiEventOverheadBytes + 3) + kAuditionEvents * (kMidiEventOverheadBytes + 3);
    REQUIRE(worstCaseBytes <= kAuditionMidiBytes);
    REQUIRE(countEvents(destination) == result.copied + kAuditionEvents);
}

TEST_CASE("Oversized system-exclusive messages are dropped", "[core][midi][realtime]")
{
    juce::MidiBuffer source;
    std::vector<juce::uint8> payload(20000, 0x11);
    source.addEvent(juce::MidiMessage::createSysExMessage(payload.data(), static_cast<int>(payload.size())),
                    5);
    REQUIRE(countEvents(source) == 1);

    juce::MidiBuffer destination;
    destination.ensureSize(kAuditionMidiBytes);

    const auto result = copyMidiWithinBudget(source, destination, 512, kHostMidiBudgetBytes);

    REQUIRE(result.copied == 0);
    REQUIRE(result.dropped == 1);
}

TEST_CASE("Events are kept in order, and a dropped event is never overtaken", "[core][midi]")
{
    constexpr int kBlock = 512;
    juce::MidiBuffer source;
    // Fill the budget with small events, then a long one that does not fit, then more small ones.
    const int fits = kHostMidiBudgetBytes / (kMidiEventOverheadBytes + 3) - 2;
    for (int i = 0; i < fits; ++i)
        source.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8)100), 0);
    std::vector<juce::uint8> longPayload(200, 0x22);
    source.addEvent(juce::MidiMessage::createSysExMessage(longPayload.data(), 200), 500);
    source.addEvent(juce::MidiMessage::noteOff(1, 40), 511);

    juce::MidiBuffer destination;
    destination.ensureSize(kAuditionMidiBytes);
    const auto result = copyMidiWithinBudget(source, destination, kBlock, kHostMidiBudgetBytes);

    REQUIRE(result.copied == fits);
    REQUIRE(result.dropped == 2);

    int lastPosition = -1;
    for (const auto metadata : destination)
    {
        REQUIRE(metadata.samplePosition >= lastPosition);
        lastPosition = metadata.samplePosition;
    }
}

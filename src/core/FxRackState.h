// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectRack.h"
#include "dsp/fx/EffectRegistry.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <string>
#include <vector>

namespace fcm::core
{

// What is in each rack, as the plugin keeps it (docs/specs/mixer-and-buses.md, docs/decisions/0010):
// per rack an ordered list of effect instance IDs for the UI and the saved state, and the packed
// value the audio thread reads. Adding, removing and duplicating an effect also sets the instance's
// parameters (through the parameter tree, so the host sees it). Every change is published to the
// audio thread at once and announced to listeners (the rack panels) through the ChangeBroadcaster.
// Message thread only.
class FxRackState : public juce::ChangeBroadcaster
{
  public:
    // `targets` are where the packed values are stored for the audio thread (the engine's racks).
    FxRackState(juce::AudioProcessorValueTreeState& parameters, const dsp::EffectRegistry& registry,
                std::array<std::atomic<std::uint64_t>*, dsp::kNumRacks> targets);

    // Instance IDs ("distortion", "distortion2", ...), first to last.
    const std::vector<std::string>& ids(dsp::RackId rack) const noexcept;

    // How many copies of this effect type are in no rack.
    int freeCopies(int effectIndex) const noexcept;
    bool canAdd(dsp::RackId rack, int effectIndex) const noexcept;
    bool canDuplicate(dsp::RackId rack, const std::string& instanceId) const noexcept;

    // Puts the lowest free copy of the effect at the end of the rack with its parameters at their
    // defaults and switched on. Returns its instance ID, or an empty string when the rack is full
    // or all copies are in use.
    std::string add(dsp::RackId rack, int effectIndex);

    // A new copy right after `instanceId` in the same rack with the same settings.
    std::string duplicate(dsp::RackId rack, const std::string& instanceId);

    // Takes the instance out of its rack and switches it off. False if it was not in the rack.
    bool remove(dsp::RackId rack, const std::string& instanceId);

    // Moves the effect at position `from` to position `to` (both clamped to the rack).
    void move(dsp::RackId rack, int from, int to);

    // The racks of a new project: Main holds the effects marked inDefaultMain, the buses are empty.
    void setDefault();

    // Saved form: a node "FxRacks" with one "Rack" child per rack (property "id": busA, busB or
    // main; property "effects": the instance IDs separated by commas).
    juce::ValueTree toValueTree() const;

    // Restores from a saved node, which is untrusted input: unknown IDs are dropped, an instance
    // listed twice (in one rack or two) keeps its first place, a rack is cut at its capacity. A
    // missing or wrongly named node gives the default racks and returns false.
    bool restoreFrom(const juce::ValueTree& node);

    static const juce::Identifier& nodeType();
    static const char* rackName(dsp::RackId rack) noexcept;

    // The position (0-based) in a row of `count` equal slots `slotWidth` wide that the horizontal
    // position `x` falls in. Clamped to the row; anything unusable gives 0.
    static int positionAt(double x, double slotWidth, int count) noexcept;

    // The same for slots of different sizes laid out one after the other (the vertical rack): the
    // position whose span holds `offset`, clamped to the first and last. Sizes that are not finite
    // positive numbers count as 0; anything unusable gives 0.
    static int positionIn(const std::vector<double>& sizes, double offset) noexcept;

  private:
    void publish();
    bool isUsed(const std::string& instanceId) const noexcept;
    void setInstanceToDefaults(int instance);
    void copyInstance(int from, int to);
    int lowestFreeInstance(int effectIndex) const noexcept;

    juce::AudioProcessorValueTreeState& parameters;
    const dsp::EffectRegistry& registry;
    std::array<std::atomic<std::uint64_t>*, dsp::kNumRacks> targets;
    std::array<std::vector<std::string>, dsp::kNumRacks> racks;
};

} // namespace fcm::core

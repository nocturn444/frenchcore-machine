// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectRegistry.h"

#include <atomic>
#include <cstdint>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <string>
#include <vector>

namespace fcm::core
{

// The order of the effects in the chain, as the plugin keeps it (docs/specs/fx-chain.md §4): a list
// of effect IDs for the UI and the saved state, and the packed value the audio thread reads. Every
// change is published to the audio thread at once and announced to listeners (the rack) through the
// ChangeBroadcaster. Message thread only.
class FxOrderState : public juce::ChangeBroadcaster
{
  public:
    // `target` is where the packed order is stored for the audio thread (the engine's chain).
    FxOrderState(const dsp::EffectRegistry& registry, std::atomic<std::uint64_t>& target);

    // Effect IDs, first to last. Always contains every registered effect exactly once.
    const std::vector<std::string>& ids() const noexcept { return order; }

    // Replaces the order. IDs that are unknown or repeated are dropped; registered effects that are
    // missing are appended in registration order.
    void set(const std::vector<std::string>& wanted);

    // Moves the effect at position `from` to position `to` (both clamped to the list).
    void move(int from, int to);

    // Saved form: a node "FxChain" with the property "order", the IDs separated by commas.
    juce::ValueTree toValueTree() const;

    // Restores from a saved node, which is untrusted input. A missing, wrongly named or unusable
    // node gives the registration order and returns false.
    bool restoreFrom(const juce::ValueTree& node);

    static const juce::Identifier& nodeType();

    // The position (0-based) in a row of `count` equal slots `slotWidth` wide that the horizontal
    // position `x` falls in. Clamped to the row; anything unusable gives 0 (used by the rack when an
    // effect is dragged).
    static int positionAt(double x, double slotWidth, int count) noexcept;

  private:
    void publish();

    const dsp::EffectRegistry& registry;
    std::atomic<std::uint64_t>& target;
    std::vector<std::string> order;
};

} // namespace fcm::core

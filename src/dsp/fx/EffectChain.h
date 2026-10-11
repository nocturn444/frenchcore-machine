// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/EffectRegistry.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

namespace fcm::dsp
{

// The row of effects (docs/specs/fx-chain.md). One instance of every registered effect, in the order
// of a packed value that another thread may change at any time. Every slot keeps a constant delay of
// its latency whether it is on or not, so toggling, mixing or reordering never changes timing
// (docs/decisions/0009). Nothing allocates after prepare().
class EffectChain
{
  public:
    explicit EffectChain(const EffectRegistry& registry) : registry(&registry) {}

    // The registry is only read at prepare(); the chain keeps the effects it built.
    void prepare(const EffectSpec& spec);
    void reset() noexcept;

    // Once per block, before process(): the state of every effect and the oversampling factor
    // (1, 2, 4 or 8). Entries beyond the registered effects are ignored.
    void setParameters(const std::array<EffectSlotParameters, kMaxEffects>& slots,
                       int oversamplingFactor) noexcept;

    // Processes the block in place (up to the channel count given to prepare()).
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    // The sum of the latencies of every registered effect at this factor.
    int latencySamples(int oversamplingFactor) const noexcept;

    // The order, written by any thread and read once per block: 4 bits per position, the effect
    // index at each position. Anything that is not a permutation of the registered effects plays in
    // registration order.
    std::atomic<std::uint64_t>& order() noexcept { return packedOrder; }
    static std::uint64_t packOrder(std::span<const int> effectIndices) noexcept;
    static std::uint64_t registrationOrder(int count) noexcept;

    int effectCount() const noexcept { return static_cast<int>(slots.size()); }

  private:
    struct Slot
    {
        std::unique_ptr<Effect> effect;
        int parameterCount = 0;
        juce::SmoothedValue<float> mix{0.0f};
        bool asleep = true;
        std::vector<float> delayLine; // numChannels * delayCapacity, a ring per channel
        int delayCapacity = 1;
        int delayLength = 0; // the current latency in samples
        int writeIndex = 0;
        std::array<float, kMaxEffectParameters> values{};
    };

    void decodeOrder(std::array<int, kMaxEffects>& positions) const noexcept;
    void setDelayLength(Slot& slot, int length) noexcept;

    const EffectRegistry* registry;
    std::vector<Slot> slots;
    EffectSpec spec;
    int factor = 1;
    bool firstBlock = true;
    juce::AudioBuffer<float> wet;
    std::atomic<std::uint64_t> packedOrder{0};
};

} // namespace fcm::dsp

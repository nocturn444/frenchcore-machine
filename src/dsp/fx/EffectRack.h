// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectPool.h"
#include "dsp/fx/NoteTriggers.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <span>
#include <vector>

namespace fcm::dsp
{

enum class RackId
{
    busA = 0,
    busB = 1,
    main = 2
};
inline constexpr int kNumRacks = 3;

// One row of effects (docs/specs/fx-chain.md, docs/specs/mixer-and-buses.md): up to
// kMaxRackEffects instances of an EffectPool, in the order of a packed value that another thread may
// change at any time. Every instance keeps a constant delay of its latency whether it is on or not,
// and the rack pads the rest up to the latency of the whole pool, so timing never changes when
// something is toggled, mixed, reordered, added or removed (docs/decisions/0009, 0010). Nothing
// allocates after prepare().
class EffectRack
{
  public:
    // The packed form: a 4-bit count, then 6 bits per pool index, first effect first.
    static std::uint64_t pack(std::span<const int> instances) noexcept;
    // Reads a packed value into `out` and returns how many entries it has. A value with a count
    // that is too big, an index that is not in the pool or a repeated index is not valid and gives 0
    // (an empty rack), never anything partial.
    static int unpack(std::uint64_t packed, int instanceCount,
                      std::array<int, kMaxRackEffects>& out) noexcept;
    // A bit per pool index for the entries of a packed value (0 when it is not valid).
    static std::uint64_t memberMask(std::uint64_t packed, int instanceCount) noexcept;

    // Sizes the padding delay for the pool; call after the pool is prepared.
    void prepare(const EffectPool& pool, const EffectSpec& spec);
    void reset() noexcept;

    // The contents, written by any thread and read once per block.
    std::atomic<std::uint64_t>& value() noexcept { return packedValue; }

    // Processes the block in place with the pool's current state (EffectPool::setParameters must
    // have run for this block) and the contents `packed` that the caller read once for the block.
    // `triggers` (from 0.3, docs/decisions/0012 item 1) lets the rack tell each running effect about
    // the note-ons whose sound reaches it in this block: the note's position in the log plus the
    // rack's input delay plus the latencies of the effects in front of it.
    void process(EffectPool& pool, std::uint64_t packed, int oversamplingFactor,
                 juce::AudioBuffer<float>& buffer, const RackTriggers& triggers = {}) noexcept;

  private:
    void processChunk(EffectPool& pool, const std::array<int, kMaxRackEffects>& members, int count,
                      int padSamples, int oversamplingFactor, std::int64_t chunkClock,
                      const RackTriggers& triggers, juce::AudioBuffer<float>& buffer) noexcept;

    std::atomic<std::uint64_t> packedValue{0};
    std::vector<float> padLine; // numChannels * padCapacity, a ring per channel
    int padCapacity = 1;
    int padWrite = 0;
    int instanceCount = 0;
};

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/EffectRegistry.h"

#include <array>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

namespace fcm::dsp
{

// Every effect instance of the plugin: kInstancesPerEffect copies of each registered effect, built
// and prepared once (docs/decisions/0010). It keeps what each instance needs between blocks: the
// smoothed mix, the delay line that keeps its dry signal in time with its wet signal, and whether it
// is asleep. The racks (EffectRack) decide which instances run and in which order; the pool never
// allocates after prepare().
class EffectPool
{
  public:
    explicit EffectPool(const EffectRegistry& source) : registry(&source) {}

    // The registry is only read here; the pool keeps the effects it built.
    void prepare(const EffectSpec& spec);
    void reset() noexcept;

    // Once per block, before the racks process. `slots` has one entry per pool index (entries
    // beyond the registry are ignored). `inRack` has a bit per pool index: an instance that is in no
    // rack is not touched, and is reset when it joins one.
    void setParameters(const std::array<EffectSlotParameters, kMaxInstances>& slots, int oversamplingFactor,
                       const EffectContext& context, std::uint64_t inRack) noexcept;

    // The sum of the latencies of every instance at this factor (docs/decisions/0010 item 6).
    int latencySamples(int oversamplingFactor) const noexcept;
    // The largest that sum gets over the four factors, for sizing delay lines.
    int largestLatencySamples() const noexcept;

    int instanceCount() const noexcept { return static_cast<int>(slots.size()); }
    int instanceLatency(int instance, int oversamplingFactor) const noexcept;

  private:
    friend class EffectRack;

    struct Slot
    {
        std::unique_ptr<Effect> effect;
        int parameterCount = 0;
        juce::SmoothedValue<float> mix{0.0f};
        bool asleep = true;
        bool inRack = false;
        std::vector<float> delayLine; // numChannels * delayCapacity, a ring per channel
        int delayCapacity = 1;
        int delayLength = 0; // the current latency in samples
        int writeIndex = 0;
        std::array<float, kMaxEffectParameters> values{};
    };

    void setDelayLength(Slot& slot, int length) noexcept;

    const EffectRegistry* registry;
    std::vector<Slot> slots;
    EffectSpec spec;
    int factor = 1;
    bool firstBlock = true;
    juce::AudioBuffer<float> wet; // scratch for the racks (they run one after the other)
};

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectPool.h"
#include "dsp/fx/EffectRack.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace fcm::test
{

// Pool index of copy `copy` (1 to 4) of the effect at `effect` in the registry.
constexpr int instance(int effect, int copy = 1)
{
    return fcm::dsp::EffectRegistry::instanceIndex(effect, copy);
}

// One pool and one rack wired the way the engine wires them, for testing effects and the rack
// without the voice. By default the rack holds the first copy of every effect in registration order.
class RackRig
{
  public:
    explicit RackRig(const fcm::dsp::EffectRegistry& source) : pool(source), registry(source) {}

    void prepare(const fcm::dsp::EffectSpec& spec)
    {
        pool.prepare(spec);
        rack.prepare(pool, spec);
        std::vector<int> firstCopies;
        for (int effect = 0; effect < registry.count(); ++effect)
            firstCopies.push_back(instance(effect));
        setContents(firstCopies);
    }

    void setContents(std::span<const int> instances)
    {
        rack.value().store(fcm::dsp::EffectRack::pack(instances));
    }

    void setParameters(const std::array<fcm::dsp::EffectSlotParameters, fcm::dsp::kMaxInstances>& slots,
                       int factor)
    {
        oversampling = factor;
        pool.setParameters(slots, factor, context,
                           fcm::dsp::EffectRack::memberMask(rack.value().load(), pool.instanceCount()));
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        rack.process(pool, rack.value().load(), oversampling, buffer);
    }

    int latencySamples(int factor) const noexcept { return pool.latencySamples(factor); }

    fcm::dsp::EffectPool pool;
    fcm::dsp::EffectRack rack;
    fcm::dsp::EffectContext context;

  private:
    const fcm::dsp::EffectRegistry& registry;
    int oversampling = 2;
};

} // namespace fcm::test

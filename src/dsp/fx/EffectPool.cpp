// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/EffectPool.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kMixSmoothSeconds = 0.010;
constexpr int kFactors[] = {1, 2, 4, 8};

bool isSupportedFactor(int factor) noexcept
{
    return factor == 1 || factor == 2 || factor == 4 || factor == 8;
}
} // namespace

void EffectPool::prepare(const EffectSpec& newSpec)
{
    spec = newSpec;
    slots.clear();
    slots.resize(static_cast<size_t>(registry->instanceCount()));

    const int channels = std::max(1, spec.numChannels);
    wet.setSize(channels, std::max(1, spec.maxBlockSize));

    for (int i = 0; i < instanceCount(); ++i)
    {
        Slot& slot = slots[static_cast<size_t>(i)];
        const int effectIndex = EffectRegistry::effectOf(i);
        slot.effect = registry->create(effectIndex);
        slot.effect->prepare(spec);
        slot.parameterCount = static_cast<int>(slot.effect->parameters().size());

        int most = 0;
        for (int f : kFactors)
            most = std::max(most, slot.effect->latencySamples(f));
        slot.delayCapacity = most + 1;
        slot.delayLine.assign(static_cast<size_t>(channels) * static_cast<size_t>(slot.delayCapacity), 0.0f);
        slot.mix.reset(spec.sampleRate, kMixSmoothSeconds);
        slot.mix.setCurrentAndTargetValue(0.0f);
        slot.asleep = true;
        slot.inRack = false;
        slot.values.fill(0.0f);
        for (int p = 0; p < slot.parameterCount; ++p)
            slot.values[static_cast<size_t>(p)] =
                slot.effect->parameters()[static_cast<size_t>(p)].defaultValue;
        slot.effect->setParameters(
            std::span<const float>(slot.values.data(), static_cast<size_t>(slot.parameterCount)));
        slot.effect->setOversampling(factor);
        setDelayLength(slot, slot.effect->latencySamples(factor));
    }
    firstBlock = true;
}

void EffectPool::reset() noexcept
{
    for (auto& slot : slots)
    {
        slot.effect->reset();
        std::fill(slot.delayLine.begin(), slot.delayLine.end(), 0.0f);
        slot.writeIndex = 0;
        slot.mix.setCurrentAndTargetValue(slot.mix.getTargetValue());
        slot.asleep = true;
    }
    firstBlock = true;
}

void EffectPool::setDelayLength(Slot& slot, int length) noexcept
{
    length = std::clamp(length, 0, slot.delayCapacity - 1);
    if (length == slot.delayLength)
        return;

    // A different latency (the user changed Quality): start the delay line empty.
    slot.delayLength = length;
    std::fill(slot.delayLine.begin(), slot.delayLine.end(), 0.0f);
    slot.writeIndex = 0;
}

void EffectPool::setParameters(const std::array<EffectSlotParameters, kMaxInstances>& parameters,
                               int oversamplingFactor, const EffectContext& context,
                               std::uint64_t inRack) noexcept
{
    const int newFactor = isSupportedFactor(oversamplingFactor) ? oversamplingFactor : 1;
    const bool factorChanged = newFactor != factor || firstBlock;
    factor = newFactor;

    for (size_t i = 0; i < slots.size() && i < parameters.size(); ++i)
    {
        Slot& slot = slots[i];
        const auto& wanted = parameters[i];
        const bool used = ((inRack >> i) & 1u) != 0;

        const float mix = std::isfinite(wanted.mix) ? std::clamp(wanted.mix, 0.0f, 1.0f) : 1.0f;
        const float target = wanted.enabled ? mix : 0.0f;
        if (firstBlock)
            slot.mix.setCurrentAndTargetValue(target);
        else
            slot.mix.setTargetValue(target);

        if (factorChanged)
            slot.effect->setOversampling(factor);
        setDelayLength(slot, slot.effect->latencySamples(factor));

        if (!used)
        {
            slot.inRack = false;
            continue;
        }

        if (!slot.inRack)
        {
            // Joining a rack: nothing of its past is valid. It starts from silence and, once it is
            // on, from a reset effect.
            slot.inRack = true;
            slot.asleep = true;
            std::fill(slot.delayLine.begin(), slot.delayLine.end(), 0.0f);
            slot.writeIndex = 0;
        }

        // Anything that is not a usable number keeps the last good value.
        for (int p = 0; p < slot.parameterCount; ++p)
            if (std::isfinite(wanted.values[static_cast<size_t>(p)]))
                slot.values[static_cast<size_t>(p)] = wanted.values[static_cast<size_t>(p)];
        slot.effect->setParameters(
            std::span<const float>(slot.values.data(), static_cast<size_t>(slot.parameterCount)));
        slot.effect->setContext(context);
        if (curveSources[i] != nullptr)
            slot.effect->setCurve(curveSources[i]->acquire());
    }
    firstBlock = false;
}

int EffectPool::latencySamples(int oversamplingFactor) const noexcept
{
    const int f = isSupportedFactor(oversamplingFactor) ? oversamplingFactor : 1;
    int total = 0;
    for (const auto& slot : slots)
        total += slot.effect != nullptr ? slot.effect->latencySamples(f) : 0;
    return total;
}

int EffectPool::largestLatencySamples() const noexcept
{
    int most = 0;
    for (int f : kFactors)
        most = std::max(most, latencySamples(f));
    return most;
}

int EffectPool::instanceLatency(int instance, int oversamplingFactor) const noexcept
{
    if (instance < 0 || instance >= instanceCount() || slots[static_cast<size_t>(instance)].effect == nullptr)
        return 0;
    return slots[static_cast<size_t>(instance)].effect->latencySamples(
        isSupportedFactor(oversamplingFactor) ? oversamplingFactor : 1);
}

} // namespace fcm::dsp

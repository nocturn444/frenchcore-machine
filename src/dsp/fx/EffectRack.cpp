// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/EffectRack.h"

#include <algorithm>

namespace fcm::dsp
{

namespace
{
constexpr int kCountBits = 4;
constexpr int kIndexBits = 6;
constexpr std::uint64_t kIndexMask = (1u << kIndexBits) - 1u;
} // namespace

std::uint64_t EffectRack::pack(std::span<const int> instances) noexcept
{
    const size_t count = std::min<size_t>(instances.size(), kMaxRackEffects);
    std::uint64_t packed = count;
    for (size_t i = 0; i < count; ++i)
        packed |= (static_cast<std::uint64_t>(instances[i]) & kIndexMask) << (kCountBits + kIndexBits * i);
    return packed;
}

int EffectRack::unpack(std::uint64_t packed, int instanceCount,
                       std::array<int, kMaxRackEffects>& out) noexcept
{
    const int count = static_cast<int>(packed & ((1u << kCountBits) - 1u));
    if (count > kMaxRackEffects)
        return 0;

    std::uint64_t seen = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto index = (packed >> (kCountBits + kIndexBits * i)) & kIndexMask;
        if (index >= static_cast<std::uint64_t>(instanceCount) || ((seen >> index) & 1u) != 0)
            return 0;
        seen |= std::uint64_t{1} << index;
        out[static_cast<size_t>(i)] = static_cast<int>(index);
    }
    return count;
}

std::uint64_t EffectRack::memberMask(std::uint64_t packed, int instanceCount) noexcept
{
    std::array<int, kMaxRackEffects> members{};
    const int count = unpack(packed, instanceCount, members);
    std::uint64_t mask = 0;
    for (int i = 0; i < count; ++i)
        mask |= std::uint64_t{1} << members[static_cast<size_t>(i)];
    return mask;
}

void EffectRack::prepare(const EffectPool& pool, const EffectSpec& spec)
{
    instanceCount = pool.instanceCount();
    padCapacity = pool.largestLatencySamples() + 1;
    padLine.assign(static_cast<size_t>(std::max(1, spec.numChannels)) * static_cast<size_t>(padCapacity),
                   0.0f);
    padWrite = 0;
}

void EffectRack::reset() noexcept
{
    std::fill(padLine.begin(), padLine.end(), 0.0f);
    padWrite = 0;
}

void EffectRack::process(EffectPool& pool, std::uint64_t packed, int oversamplingFactor,
                         juce::AudioBuffer<float>& buffer) noexcept
{
    const int total = buffer.getNumSamples();
    const int channels = std::min(buffer.getNumChannels(), pool.wet.getNumChannels());
    const int chunkSize = pool.wet.getNumSamples();
    if (total <= 0 || channels <= 0 || chunkSize <= 0 || pool.slots.empty())
        return;

    std::array<int, kMaxRackEffects> members{};
    const int count = unpack(packed, instanceCount, members);

    // Together with the padding, the rack always delays by the latency of the whole pool.
    int memberLatency = 0;
    for (int i = 0; i < count; ++i)
        memberLatency += pool.instanceLatency(members[static_cast<size_t>(i)], oversamplingFactor);
    const int padSamples =
        std::clamp(pool.latencySamples(oversamplingFactor) - memberLatency, 0, padCapacity - 1);

    // A host may hand over more samples than it announced; they are processed in pieces.
    for (int start = 0; start < total; start += chunkSize)
    {
        std::array<float*, 2> pointers{};
        const int used = std::min<int>(channels, static_cast<int>(pointers.size()));
        for (int c = 0; c < used; ++c)
            pointers[static_cast<size_t>(c)] = buffer.getWritePointer(c) + start;

        juce::AudioBuffer<float> chunk(pointers.data(), used, std::min(chunkSize, total - start));
        processChunk(pool, members, count, padSamples, chunk);
    }
}

void EffectRack::processChunk(EffectPool& pool, const std::array<int, kMaxRackEffects>& members, int count,
                              int padSamples, juce::AudioBuffer<float>& buffer) noexcept
{
    using Slot = EffectPool::Slot;

    const int numSamples = std::min(buffer.getNumSamples(), pool.wet.getNumSamples());
    const int channels = std::min(buffer.getNumChannels(), pool.wet.getNumChannels());

    for (int position = 0; position < count; ++position)
    {
        Slot& slot = pool.slots[static_cast<size_t>(members[static_cast<size_t>(position)])];
        const bool active = slot.mix.isSmoothing() || slot.mix.getTargetValue() > 0.0f;

        juce::AudioBuffer<float> wetView(pool.wet.getArrayOfWritePointers(), channels, numSamples);
        if (active)
        {
            if (slot.asleep)
            {
                slot.effect->reset();
                slot.asleep = false;
            }
            for (int c = 0; c < channels; ++c)
                wetView.copyFrom(c, 0, buffer, c, 0, numSamples);
            slot.effect->process(wetView);
        }
        else
        {
            slot.asleep = true;
        }

        // Per sample: the dry signal goes through the slot's delay line, then it is mixed with the wet.
        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = active ? slot.mix.getNextValue() : 0.0f;
            const int writeAt = slot.writeIndex;
            const int readAt = (writeAt + slot.delayCapacity - slot.delayLength) % slot.delayCapacity;

            for (int c = 0; c < channels; ++c)
            {
                float* line =
                    slot.delayLine.data() + static_cast<size_t>(c) * static_cast<size_t>(slot.delayCapacity);
                float* io = buffer.getWritePointer(c);
                line[writeAt] = io[i];
                const float dry = line[readAt];
                io[i] = active ? dry * (1.0f - mix) + wetView.getSample(c, i) * mix : dry;
            }
            slot.writeIndex = (writeAt + 1) % slot.delayCapacity;
        }
    }

    // The padding: whatever latency the effects of this rack do not add. The ring is always fed, so
    // it holds the right history whenever the padding changes.
    {
        const int padChannels =
            std::min(channels, static_cast<int>(padLine.size() / static_cast<size_t>(padCapacity)));
        for (int i = 0; i < numSamples; ++i)
        {
            const int readAt = (padWrite + padCapacity - padSamples) % padCapacity;
            for (int c = 0; c < padChannels; ++c)
            {
                float* line = padLine.data() + static_cast<size_t>(c) * static_cast<size_t>(padCapacity);
                float* io = buffer.getWritePointer(c);
                line[padWrite] = io[i];
                io[i] = line[readAt];
            }
            padWrite = (padWrite + 1) % padCapacity;
        }
    }
}

} // namespace fcm::dsp

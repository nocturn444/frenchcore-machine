// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/EffectChain.h"

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

std::uint64_t EffectChain::packOrder(std::span<const int> effectIndices) noexcept
{
    std::uint64_t packed = 0;
    for (size_t i = 0; i < effectIndices.size() && i < static_cast<size_t>(kMaxEffects); ++i)
        packed |= (static_cast<std::uint64_t>(effectIndices[i]) & 0xF) << (4 * i);
    return packed;
}

std::uint64_t EffectChain::registrationOrder(int count) noexcept
{
    std::uint64_t packed = 0;
    for (int i = 0; i < std::min(count, kMaxEffects); ++i)
        packed |= static_cast<std::uint64_t>(i) << (4 * i);
    return packed;
}

void EffectChain::prepare(const EffectSpec& newSpec)
{
    spec = newSpec;
    slots.clear();
    slots.resize(static_cast<size_t>(registry->count()));

    const int channels = std::max(1, spec.numChannels);
    wet.setSize(channels, std::max(1, spec.maxBlockSize));

    for (int i = 0; i < registry->count(); ++i)
    {
        Slot& slot = slots[static_cast<size_t>(i)];
        slot.effect = registry->create(i);
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
        slot.values.fill(0.0f);
        for (int p = 0; p < slot.parameterCount; ++p)
            slot.values[static_cast<size_t>(p)] =
                slot.effect->parameters()[static_cast<size_t>(p)].defaultValue;
        slot.effect->setParameters(
            std::span<const float>(slot.values.data(), static_cast<size_t>(slot.parameterCount)));
        slot.effect->setOversampling(factor);
        setDelayLength(slot, slot.effect->latencySamples(factor));
    }

    if (packedOrder.load() == 0)
        packedOrder.store(registrationOrder(registry->count()));
    firstBlock = true;
}

void EffectChain::reset() noexcept
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

void EffectChain::setDelayLength(Slot& slot, int length) noexcept
{
    length = std::clamp(length, 0, slot.delayCapacity - 1);
    if (length == slot.delayLength)
        return;

    // A different latency (the user changed Quality): start the delay line empty.
    slot.delayLength = length;
    std::fill(slot.delayLine.begin(), slot.delayLine.end(), 0.0f);
    slot.writeIndex = 0;
}

void EffectChain::setParameters(const std::array<EffectSlotParameters, kMaxEffects>& parameters,
                                int oversamplingFactor) noexcept
{
    const int newFactor = isSupportedFactor(oversamplingFactor) ? oversamplingFactor : 1;
    const bool factorChanged = newFactor != factor || firstBlock;
    factor = newFactor;

    for (size_t i = 0; i < slots.size() && i < parameters.size(); ++i)
    {
        Slot& slot = slots[i];
        const auto& wanted = parameters[i];

        // Anything that is not a usable number keeps the last good value.
        for (int p = 0; p < slot.parameterCount; ++p)
            if (std::isfinite(wanted.values[static_cast<size_t>(p)]))
                slot.values[static_cast<size_t>(p)] = wanted.values[static_cast<size_t>(p)];
        slot.effect->setParameters(
            std::span<const float>(slot.values.data(), static_cast<size_t>(slot.parameterCount)));

        const float mix = std::isfinite(wanted.mix) ? std::clamp(wanted.mix, 0.0f, 1.0f) : 1.0f;
        const float target = wanted.enabled ? mix : 0.0f;
        if (firstBlock)
            slot.mix.setCurrentAndTargetValue(target);
        else
            slot.mix.setTargetValue(target);

        if (factorChanged)
            slot.effect->setOversampling(factor);
        setDelayLength(slot, slot.effect->latencySamples(factor));
    }
    firstBlock = false;
}

int EffectChain::latencySamples(int oversamplingFactor) const noexcept
{
    const int f = isSupportedFactor(oversamplingFactor) ? oversamplingFactor : 1;
    int total = 0;
    for (const auto& slot : slots)
        total += slot.effect != nullptr ? slot.effect->latencySamples(f) : 0;
    return total;
}

void EffectChain::decodeOrder(std::array<int, kMaxEffects>& positions) const noexcept
{
    const int count = effectCount();
    const std::uint64_t packed = packedOrder.load();

    std::array<bool, kMaxEffects> seen{};
    bool valid = true;
    for (int i = 0; i < count; ++i)
    {
        const int index = static_cast<int>((packed >> (4 * i)) & 0xF);
        positions[static_cast<size_t>(i)] = index;
        if (index >= count || seen[static_cast<size_t>(index)])
            valid = false;
        else
            seen[static_cast<size_t>(index)] = true;
    }

    if (!valid)
        for (int i = 0; i < count; ++i)
            positions[static_cast<size_t>(i)] = i;
}

void EffectChain::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int total = buffer.getNumSamples();
    const int channels = std::min(buffer.getNumChannels(), wet.getNumChannels());
    const int chunkSize = wet.getNumSamples();
    if (total <= 0 || channels <= 0 || chunkSize <= 0 || slots.empty())
        return;

    // A host may hand over more samples than it announced; they are processed in pieces.
    for (int start = 0; start < total; start += chunkSize)
    {
        std::array<float*, 2> pointers{};
        const int used = std::min<int>(channels, static_cast<int>(pointers.size()));
        for (int c = 0; c < used; ++c)
            pointers[static_cast<size_t>(c)] = buffer.getWritePointer(c) + start;

        juce::AudioBuffer<float> chunk(pointers.data(), used, std::min(chunkSize, total - start));
        processChunk(chunk);
    }
}

void EffectChain::processChunk(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = std::min(buffer.getNumSamples(), wet.getNumSamples());
    const int channels = std::min(buffer.getNumChannels(), wet.getNumChannels());
    std::array<int, kMaxEffects> positions{};
    decodeOrder(positions);

    for (int position = 0; position < effectCount(); ++position)
    {
        Slot& slot = slots[static_cast<size_t>(positions[static_cast<size_t>(position)])];
        const bool active = slot.mix.isSmoothing() || slot.mix.getTargetValue() > 0.0f;

        juce::AudioBuffer<float> wetView(wet.getArrayOfWritePointers(), channels, numSamples);
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
}

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/BusStage.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

void DelayRing::prepare(int capacity)
{
    data.assign(static_cast<size_t>(std::max(1, capacity)), 0.0f);
    write = 0;
}

void DelayRing::reset() noexcept
{
    std::fill(data.begin(), data.end(), 0.0f);
    write = 0;
}

void BusStage::prepare(double sampleRate, int maxBlock, int maxLatency)
{
    cleanRing.prepare(maxLatency + 1);
    weightRing.prepare(maxLatency + 1);
    clean.assign(static_cast<size_t>(std::max(1, maxBlock)), 0.0f);
    weight.assign(static_cast<size_t>(std::max(1, maxBlock)), 0.0f);
    for (auto* smoothed : {&level, &whole, &punch, &tail})
        smoothed->reset(sampleRate, kSmoothSeconds);
    reset();
}

void BusStage::reset() noexcept
{
    cleanRing.reset();
    weightRing.reset();
    for (auto* smoothed : {&level, &whole, &punch, &tail})
        smoothed->setCurrentAndTargetValue(smoothed->getTargetValue());
}

void BusStage::setParameters(const BusParameters& parameters, bool snap) noexcept
{
    const float levelTarget = std::isfinite(parameters.level) ? std::max(parameters.level, 0.0f) : 1.0f;
    const auto region = parameters.region;
    const std::array<float, 3> targets{
        region == BusRegion::whole || (region != BusRegion::punch && region != BusRegion::tail) ? 1.0f : 0.0f,
        region == BusRegion::punch ? 1.0f : 0.0f, region == BusRegion::tail ? 1.0f : 0.0f};
    if (snap)
    {
        level.setCurrentAndTargetValue(levelTarget);
        whole.setCurrentAndTargetValue(targets[0]);
        punch.setCurrentAndTargetValue(targets[1]);
        tail.setCurrentAndTargetValue(targets[2]);
    }
    else
    {
        level.setTargetValue(levelTarget);
        whole.setTargetValue(targets[0]);
        punch.setTargetValue(targets[1]);
        tail.setTargetValue(targets[2]);
    }
}

void BusStage::begin(const float* input, const float* punchWeight, int count, int latency,
                     juce::AudioBuffer<float>& rackInput) noexcept
{
    count = std::min(count, static_cast<int>(clean.size()));
    float* left = rackInput.getWritePointer(0);
    float* right = rackInput.getNumChannels() > 1 ? rackInput.getWritePointer(1) : nullptr;

    for (int i = 0; i < count; ++i)
    {
        const float wp = punchWeight[i];
        const float w = whole.getNextValue() + punch.getNextValue() * wp + tail.getNextValue() * (1.0f - wp);
        clean[static_cast<size_t>(i)] = cleanRing.process(input[i], latency);
        weight[static_cast<size_t>(i)] = weightRing.process(w, latency);
        left[i] = input[i];
        if (right != nullptr)
            right[i] = input[i];
    }
}

void BusStage::finish(const juce::AudioBuffer<float>& processed, int count,
                      juce::AudioBuffer<float>& destination) noexcept
{
    count = std::min(count, static_cast<int>(clean.size()));
    const int channels = std::min(processed.getNumChannels(), destination.getNumChannels());
    for (int i = 0; i < count; ++i)
    {
        const float gain = level.getNextValue();
        const float w = weight[static_cast<size_t>(i)];
        const float dry = clean[static_cast<size_t>(i)] * (1.0f - w);
        for (int c = 0; c < channels; ++c)
            destination.getWritePointer(c)[i] += gain * (dry + processed.getSample(c, i) * w);
    }
}

} // namespace fcm::dsp

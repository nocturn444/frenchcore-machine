// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/Mixer.h"

#include <algorithm>

namespace fcm::dsp
{

void Mixer::prepare(double sampleRate) noexcept
{
    for (auto& channel : channels)
    {
        channel.audible.reset(sampleRate, kSmoothSeconds);
        for (auto& route : channel.route)
            route.reset(sampleRate, kSmoothSeconds);
    }
    reset();
}

void Mixer::reset() noexcept
{
    for (auto& channel : channels)
    {
        channel.audible.setCurrentAndTargetValue(channel.audible.getTargetValue());
        for (auto& route : channel.route)
            route.setCurrentAndTargetValue(route.getTargetValue());
    }
}

void Mixer::setParameters(const std::array<MixerSourceParameters, kNumStems>& sources, bool snap) noexcept
{
    // Solo on any channel silences every channel that is not soloed; Mute silences its own.
    const bool anySolo = std::any_of(sources.begin(), sources.end(), [](const auto& s) { return s.solo; });

    for (size_t i = 0; i < channels.size(); ++i)
    {
        auto& channel = channels[i];
        const auto& source = sources[i];
        const bool audible = !source.mute && (!anySolo || source.solo);

        // A destination that is not one of the three (a damaged value) goes to Main.
        const int wanted = static_cast<int>(source.destination);
        const int destination = wanted >= 0 && wanted < kNumDestinations ? wanted : 0;

        const float audibleTarget = audible ? 1.0f : 0.0f;
        if (snap)
            channel.audible.setCurrentAndTargetValue(audibleTarget);
        else
            channel.audible.setTargetValue(audibleTarget);

        for (int d = 0; d < kNumDestinations; ++d)
        {
            const float routeTarget = d == destination ? 1.0f : 0.0f;
            auto& route = channel.route[static_cast<size_t>(d)];
            if (snap)
                route.setCurrentAndTargetValue(routeTarget);
            else
                route.setTargetValue(routeTarget);
        }
    }
}

void Mixer::process(const std::array<float, kNumStems>& stems,
                    std::array<float, kNumDestinations>& sums) noexcept
{
    sums.fill(0.0f);
    for (size_t i = 0; i < channels.size(); ++i)
    {
        auto& channel = channels[i];
        const float level = stems[i] * channel.audible.getNextValue();
        for (size_t d = 0; d < sums.size(); ++d)
        {
            const float weight = channel.route[d].getNextValue();
            // Exact zero for a destination the stem has fully left, so an unused bus stays silent.
            if (weight != 0.0f)
                sums[d] += level * weight;
        }
    }
}

} // namespace fcm::dsp

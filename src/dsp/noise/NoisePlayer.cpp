// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/noise/NoisePlayer.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kMaxSemitones = 96.0;
constexpr std::uint32_t kSeed = 0x2545f491u;
} // namespace

void NoisePlayer::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void NoisePlayer::reset() noexcept
{
    running = false;
    position = 0.0;
    randomState = kSeed;
}

void NoisePlayer::start(const NoiseSettings& settings) noexcept
{
    oneShot = settings.oneShot;
    const double phase =
        std::isfinite(settings.phase) ? std::clamp(static_cast<double>(settings.phase), 0.0, 1.0) : 0.0;
    const double random =
        std::isfinite(settings.random) ? std::clamp(static_cast<double>(settings.random), 0.0, 1.0) : 0.0;

    // xorshift32 (Marsaglia 2003): deterministic after reset(), a different start for every kick.
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    const double unit = static_cast<double>(randomState) / 4294967296.0;

    running = sample != nullptr && sample->samples.size() >= 2;
    if (running)
    {
        const double start = std::fmod(phase + random * unit, 1.0);
        position = std::min(start * static_cast<double>(sample->samples.size()),
                            static_cast<double>(sample->samples.size()) - 1.0);
    }
}

float NoisePlayer::value(int index) const noexcept
{
    const auto length = static_cast<int>(sample->samples.size());
    const auto& data = sample->samples;
    if (oneShot)
        return index >= 0 && index < length ? data[static_cast<size_t>(index)] : 0.0f;

    // Loop: the last 5 ms are blended with the beginning, so that after the last sample comes the
    // first one without a step.
    const int fade = std::clamp(static_cast<int>(kLoopCrossfadeSeconds * sample->sampleRate), 1, length / 4);
    const int loopEnd = length - fade;
    if (index >= length)
        return data[static_cast<size_t>(index - loopEnd)];
    if (index < loopEnd)
        return data[static_cast<size_t>(std::max(index, 0))];

    const float w = static_cast<float>(index - loopEnd) / static_cast<float>(fade);
    return data[static_cast<size_t>(index)] * (1.0f - w) + data[static_cast<size_t>(index - loopEnd)] * w;
}

float NoisePlayer::process(double semitones) noexcept
{
    if (!running || sample == nullptr)
        return 0.0f;

    const auto length = static_cast<int>(sample->samples.size());
    const int whole = static_cast<int>(position);
    const auto fraction = static_cast<float>(position - whole);
    const float a = value(whole);
    const float b = value(whole + 1);
    const float out = a + fraction * (b - a);

    const double pitch =
        std::isfinite(semitones) ? std::clamp(semitones, -kMaxSemitones, kMaxSemitones) : 0.0;
    position += std::exp2(pitch / 12.0) * sample->sampleRate / sampleRate;

    if (oneShot)
    {
        if (position >= static_cast<double>(length - 1))
            running = false;
    }
    else
    {
        const int fade =
            std::clamp(static_cast<int>(kLoopCrossfadeSeconds * sample->sampleRate), 1, length / 4);
        const double loopEnd = static_cast<double>(length - fade);
        while (position >= static_cast<double>(length))
            position -= loopEnd;
    }
    return std::isfinite(out) ? out : 0.0f;
}

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/oscillator/WavetableOscillator.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
// Linear interpolation inside one frame; the guard sample makes index + 1 always valid.
inline float readFrame(const float* frame, double phase) noexcept
{
    const double scaled = phase * WavetableData::kFrameSize;
    const auto index = static_cast<int>(scaled);
    const auto fraction = static_cast<float>(scaled - index);
    return frame[index] + fraction * (frame[index + 1] - frame[index]);
}
} // namespace

void WavetableOscillator::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    reset();
}

int WavetableOscillator::mipLevelFor(double frequencyHz, double sampleRate) noexcept
{
    // Level m is safe when 1024 / 2^m <= nyquist / f, i.e. m >= log2(2048 f / sampleRate).
    const double x = 2048.0 * frequencyHz / sampleRate;
    if (x <= 1.0)
        return 0;

    int exponent = 0;
    const double mantissa = std::frexp(x, &exponent); // x = mantissa * 2^exponent, mantissa in [0.5, 1)
    const int level = mantissa > 0.5 ? exponent : exponent - 1; // ceil(log2(x))
    return std::min(level, WavetableData::kNumMipLevels - 1);
}

float WavetableOscillator::process(double frequencyHz) noexcept
{
    if (table == nullptr)
        return 0.0f;

    const double frequency = std::clamp(frequencyHz, 0.0, 0.45 * sampleRate);
    const int mip = mipLevelFor(frequency, sampleRate);

    float output;
    const int numFrames = table->numFrames();
    if (numFrames == 1)
    {
        output = readFrame(table->frame(mip, 0), phase);
    }
    else
    {
        const double framePosition = static_cast<double>(std::clamp(position, 0.0f, 1.0f)) * (numFrames - 1);
        const int lower = std::min(static_cast<int>(framePosition), numFrames - 2);
        const auto fraction = static_cast<float>(framePosition - lower);
        const float a = readFrame(table->frame(mip, lower), phase);
        const float b = readFrame(table->frame(mip, lower + 1), phase);
        output = a + fraction * (b - a);
    }

    phase += frequency / sampleRate;
    phase -= std::floor(phase);
    return output;
}

} // namespace fcm::dsp

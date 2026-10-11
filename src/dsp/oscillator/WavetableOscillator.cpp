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
    // The phase is kept in [0, 1), so the index is below kFrameSize; the clamp makes that certain.
    const auto index = std::clamp(static_cast<int>(scaled), 0, WavetableData::kFrameSize - 1);
    const auto fraction = static_cast<float>(scaled - index);
    return frame[index] + fraction * (frame[index + 1] - frame[index]);
}
} // namespace

void WavetableOscillator::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    reset();
}

double WavetableOscillator::wrapPhase(double phase) noexcept
{
    if (!std::isfinite(phase))
        return 0.0;

    const double wrapped = phase - std::floor(phase);
    // A tiny negative phase rounds up to exactly 1 here, which would index one past the frame.
    return wrapped >= 0.0 && wrapped < 1.0 ? wrapped : 0.0;
}

int WavetableOscillator::mipLevelFor(double frequencyHz, double sampleRate) noexcept
{
    // Level m is safe when 1024 / 2^m <= nyquist / f, i.e. m >= log2(2048 f / sampleRate).
    const double x = 2048.0 * frequencyHz / sampleRate;
    if (!(x > 1.0)) // also catches NaN
        return 0;
    if (!std::isfinite(x))
        return WavetableData::kNumMipLevels - 1;

    int exponent = 0;
    const double mantissa = std::frexp(x, &exponent); // x = mantissa * 2^exponent, mantissa in [0.5, 1)
    const int level = mantissa > 0.5 ? exponent : exponent - 1; // ceil(log2(x))
    return std::clamp(level, 0, WavetableData::kNumMipLevels - 1);
}

float WavetableOscillator::process(double frequencyHz) noexcept
{
    if (table == nullptr)
        return 0.0f;

    // A frequency that is not a positive finite number plays as 0 Hz (a constant), never as NaN.
    const double frequency = frequencyHz > 0.0 ? std::min(frequencyHz, 0.45 * sampleRate) : 0.0;
    const int mip = mipLevelFor(frequency, sampleRate);

    float output;
    const int numFrames = table->numFrames();
    if (numFrames == 1)
    {
        output = readFrame(table->frame(mip, 0), phase);
    }
    else
    {
        const double framePosition = static_cast<double>(position) * (numFrames - 1); // position is in [0, 1]
        const int lower = std::min(static_cast<int>(framePosition), numFrames - 2);
        const auto fraction = static_cast<float>(framePosition - lower);
        const float a = readFrame(table->frame(mip, lower), phase);
        const float b = readFrame(table->frame(mip, lower + 1), phase);
        output = a + fraction * (b - a);
    }

    phase = wrapPhase(phase + frequency / sampleRate);
    return output;
}

} // namespace fcm::dsp

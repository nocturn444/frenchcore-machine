// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/oscillator/FactoryWavetables.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr int kFrameSize = WavetableData::kFrameSize;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

// Shape of one cycle at phase x (radians) for morph position t in 0..1 (0 for single-frame tables).
using Shape = std::function<double(double x, double t)>;

struct Table
{
    const char* name;
    int numFrames;
    Shape shape;
};

// Sum of sin(n x) / n^power over the harmonics n = first, first + step, ... up to `last`.
double harmonics(double x, int first, int last, int step, double power, bool alternate)
{
    double sum = 0.0;
    int sign = 1;
    for (int n = first; n <= last; n += step)
    {
        sum += sign * std::sin(n * x) / std::pow(n, power);
        if (alternate)
            sign = -sign;
    }
    return sum;
}

const std::array<Table, 9>& tables()
{
    // Built once; the shapes are pure functions, so this is thread safe after initialisation.
    static const std::array<Table, 9> list{{
        {"Sine", 1, [](double x, double) { return std::sin(x); }},
        // Triangle from its odd harmonics (1/n^2, alternating sign): rounder than a naive triangle.
        {"Soft Triangle", 1, [](double x, double) { return harmonics(x, 1, 41, 2, 2.0, true); }},
        // Sine pushed into tanh, drive 1 to 6.
        {"Warm Drive", 8, [](double x, double t) { return std::tanh((1.0 + 5.0 * t) * std::sin(x)); }},
        // Sine clipped at +-1 after a gain of 1 to 8.
        {"Hard Clip", 8,
         [](double x, double t) { return std::clamp((1.0 + 7.0 * t) * std::sin(x), -1.0, 1.0); }},
        // Wavefolder: sin(k sin x), k from 0.5 to 4 radians.
        {"Wave Fold", 8, [](double x, double t) { return std::sin((0.5 + 3.5 * t) * std::sin(x)); }},
        // Odd harmonics added one by one (1/n): sine towards a square.
        {"Odd Stack", 8,
         [](double x, double t)
         {
             const int last = 1 + 2 * static_cast<int>(std::lround(t * 7.0));
             return harmonics(x, 1, std::min(last, 31), 2, 1.0, false);
         }},
        // All harmonics added (1/n): sine towards a saw.
        {"Saw Rise", 8,
         [](double x, double t)
         {
             const int last = 1 + static_cast<int>(std::lround(t * 7.0)) * 4;
             return harmonics(x, 1, std::max(1, last), 1, 1.0, false);
         }},
        // A second harmonic makes the wave lopsided, then tanh drives it hard.
        {"Hardcore Punch", 12, [](double x, double t)
         { return std::tanh((2.0 + 6.0 * t) * (std::sin(x) + 0.5 * t * std::sin(2.0 * x + 0.6))); }},
        // A DC bias before tanh gives strong even harmonics; the bias is removed afterwards.
        {"Rumble Core", 16,
         [](double x, double t)
         {
             const double drive = 1.0 + 9.0 * t;
             const double bias = 0.6 * t;
             return std::tanh(drive * (std::sin(x) + bias));
         }},
    }};
    return list;
}

// Removes the mean and scales the frame to a peak of exactly 1 (a flat frame stays zero).
void normalise(float* frame)
{
    double mean = 0.0;
    for (int i = 0; i < kFrameSize; ++i)
        mean += frame[i];
    mean /= kFrameSize;

    double peak = 0.0;
    for (int i = 0; i < kFrameSize; ++i)
        peak = std::max(peak, std::abs(frame[i] - mean));

    const double scale = peak > 1.0e-12 ? 1.0 / peak : 0.0;
    for (int i = 0; i < kFrameSize; ++i)
        frame[i] = static_cast<float>((frame[i] - mean) * scale);
}
} // namespace

int FactoryWavetables::count() noexcept
{
    return static_cast<int>(tables().size());
}

const char* FactoryWavetables::name(int index) noexcept
{
    return index >= 0 && index < count() ? tables()[static_cast<size_t>(index)].name : "";
}

std::vector<float> FactoryWavetables::makeFrames(int index)
{
    const Table& table = tables()[static_cast<size_t>(index >= 0 && index < count() ? index : 0)];
    std::vector<float> samples(static_cast<size_t>(table.numFrames) * kFrameSize);

    for (int frame = 0; frame < table.numFrames; ++frame)
    {
        const double t = table.numFrames > 1 ? static_cast<double>(frame) / (table.numFrames - 1) : 0.0;
        float* out = samples.data() + static_cast<size_t>(frame) * kFrameSize;
        for (int i = 0; i < kFrameSize; ++i)
            out[i] = static_cast<float>(table.shape(kTwoPi * i / kFrameSize, t));
        normalise(out);
    }
    return samples;
}

std::unique_ptr<const WavetableData> FactoryWavetables::build(int index)
{
    const auto frames = makeFrames(index);
    return WavetableData::fromFrames(frames);
}

} // namespace fcm::dsp

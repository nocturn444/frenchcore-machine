// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/tock/FactoryTocks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kTwoPi = 2.0 * std::numbers::pi;

struct Noise
{
    std::uint32_t state = 0x2545f491u;
    double next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<double>(state) * (2.0 / 4294967296.0) - 1.0;
    }
};

using Maker = std::vector<float> (*)();

// A tiny dry click: the derivative of a Gaussian pulse (one short up-and-down wiggle with no DC),
// about 4 ms long.
std::vector<float> makeClick()
{
    constexpr double kSigma = 0.00018;
    constexpr double kCentre = 0.0015;
    std::vector<float> out(static_cast<size_t>(0.004 * FactoryTocks::kSampleRate));
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / FactoryTocks::kSampleRate - kCentre;
        out[i] = static_cast<float>(-t / kSigma * std::exp(-t * t / (2.0 * kSigma * kSigma)));
    }
    return out;
}

// A short low thud: a 160 Hz sine that falls a little in pitch, with a puff of noise at the start.
std::vector<float> makeKnock()
{
    std::vector<float> out(static_cast<size_t>(0.080 * FactoryTocks::kSampleRate));
    Noise noise;
    double phase = 0.0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / FactoryTocks::kSampleRate;
        const double frequency = 120.0 + 60.0 * std::exp(-t / 0.012);
        phase += frequency / FactoryTocks::kSampleRate;
        const double body = std::sin(kTwoPi * phase) * std::exp(-t / 0.022);
        const double puff = noise.next() * std::exp(-t / 0.002) * 0.4;
        out[i] = static_cast<float>(body + puff);
    }
    return out;
}

// A quick snap: 12 ms of noise, high-passed by subtracting a one-pole low-pass.
std::vector<float> makeSnap()
{
    std::vector<float> out(static_cast<size_t>(0.012 * FactoryTocks::kSampleRate));
    Noise noise;
    double low = 0.0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / FactoryTocks::kSampleRate;
        const double x = noise.next();
        low += 0.12 * (x - low);
        out[i] = static_cast<float>((x - low) * std::exp(-t / 0.003));
    }
    return out;
}

// A short bright tick: a 3.2 kHz sine, 10 ms, decaying fast.
std::vector<float> makeTick()
{
    std::vector<float> out(static_cast<size_t>(0.010 * FactoryTocks::kSampleRate));
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / FactoryTocks::kSampleRate;
        out[i] = static_cast<float>(std::sin(kTwoPi * 3200.0 * t) * std::exp(-t / 0.002));
    }
    return out;
}

struct Entry
{
    const char* name;
    Maker make;
};

const std::array<Entry, 4>& entries()
{
    static const std::array<Entry, 4> list{{
        {"Click", makeClick},
        {"Knock", makeKnock},
        {"Snap", makeSnap},
        {"Tick", makeTick},
    }};
    return list;
}
} // namespace

int FactoryTocks::count() noexcept
{
    return static_cast<int>(entries().size());
}

const char* FactoryTocks::name(int index) noexcept
{
    return index >= 0 && index < count() ? entries()[static_cast<size_t>(index)].name : "";
}

std::unique_ptr<const TockSample> FactoryTocks::build(int index)
{
    auto sample = std::make_unique<TockSample>();
    sample->sampleRate = kSampleRate;
    sample->samples = entries()[static_cast<size_t>(index >= 0 && index < count() ? index : 0)].make();

    float peak = 0.0f;
    for (float s : sample->samples)
        peak = std::max(peak, std::abs(s));
    if (peak > 1.0e-9f)
        for (float& s : sample->samples)
            s /= peak;
    return sample;
}

} // namespace fcm::dsp

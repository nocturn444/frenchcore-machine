// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/noise/FactoryNoises.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kRate = FactoryNoises::kSampleRate;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr size_t kLoopSamples = static_cast<size_t>(2.0 * kRate);
constexpr size_t kShotSamples = static_cast<size_t>(0.150 * kRate);

// xorshift32 (Marsaglia 2003), mapped to -1..1, a fixed seed so every build is the same.
struct Random
{
    std::uint32_t state = 0x9e3779b9u;
    double next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<double>(state) * (2.0 / 4294967296.0) - 1.0;
    }
    double nextUnit() noexcept { return 0.5 * (next() + 1.0); }
};

using Maker = std::vector<float> (*)();

std::vector<float> makeWhite()
{
    std::vector<float> out(kLoopSamples);
    Random random;
    for (auto& s : out)
        s = static_cast<float>(random.next());
    return out;
}

// Pink noise from white through Paul Kellet's "economy" filter (about -3 dB per octave to within
// 0.05 dB from 10 Hz to nearly the Nyquist frequency).
std::vector<float> makePink()
{
    std::vector<float> out(kLoopSamples);
    Random random;
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& s : out)
    {
        const double white = random.next();
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        s = static_cast<float>(b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362);
        b6 = white * 0.115926;
    }
    return out;
}

// Brown noise: white noise integrated with a very slow leak (about 4 Hz), -6 dB per octave above it.
std::vector<float> makeBrown()
{
    std::vector<float> out(kLoopSamples);
    Random random;
    double y = 0.0;
    for (auto& s : out)
    {
        y = 0.9995 * y + random.next();
        s = static_cast<float>(y);
    }
    return out;
}

// Vinyl-like crackle: sparse clicks (about 40 a second) of random size, each a short decay.
std::vector<float> makeCrackle()
{
    std::vector<float> out(kLoopSamples, 0.0f);
    Random random;
    const double probability = 40.0 / kRate;
    double level = 0.0;
    for (auto& s : out)
    {
        if (random.nextUnit() < probability)
            level = random.next() * (0.3 + 0.7 * random.nextUnit());
        s = static_cast<float>(level);
        level *= std::exp(-1.0 / (0.0008 * kRate));
    }
    return out;
}

// A ringing, inharmonic noise: white noise through eight resonators at unrelated frequencies.
std::vector<float> makeMetal()
{
    static constexpr std::array<double, 8> kFrequencies{517.0,  811.0,  1283.0, 1957.0,
                                                        2711.0, 3643.0, 4987.0, 6733.0};
    std::vector<float> out(kLoopSamples);
    Random random;
    std::array<double, 8> y1{}, y2{};
    std::array<double, 8> a1{}, a2{}, gain{};
    for (size_t k = 0; k < 8; ++k)
    {
        const double radius = 0.9985;
        a1[k] = 2.0 * radius * std::cos(kTwoPi * kFrequencies[k] / kRate);
        a2[k] = -radius * radius;
        gain[k] = 1.0 - radius;
    }
    for (auto& s : out)
    {
        const double white = random.next();
        double sum = 0.0;
        for (size_t k = 0; k < 8; ++k)
        {
            const double y = a1[k] * y1[k] + a2[k] * y2[k] + gain[k] * white;
            y2[k] = y1[k];
            y1[k] = y;
            sum += y;
        }
        s = static_cast<float>(sum);
    }
    return out;
}

// A stepped, bit-crushed noise: white noise held for four samples and rounded to 4 bits.
std::vector<float> makeDigital()
{
    std::vector<float> out(kLoopSamples);
    Random random;
    float held = 0.0f;
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (i % 4 == 0)
            held = static_cast<float>(std::round(random.next() * 7.5) / 7.5);
        out[i] = held;
    }
    return out;
}

// A short bright burst: noise with its lows taken off (minus a one-pole low-pass), decaying fast.
std::vector<float> makeAirBurst()
{
    std::vector<float> out(kShotSamples);
    Random random;
    double low = 0.0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / kRate;
        const double x = random.next();
        low += 0.25 * (x - low);
        out[i] = static_cast<float>((x - low) * std::exp(-t / 0.022));
    }
    return out;
}

// A short low noise hit: noise through a one-pole low-pass at about 150 Hz, decaying fast.
std::vector<float> makeThump()
{
    std::vector<float> out(kShotSamples);
    Random random;
    double low = 0.0;
    const double coefficient = 1.0 - std::exp(-kTwoPi * 150.0 / kRate);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / kRate;
        low += coefficient * (random.next() - low);
        out[i] = static_cast<float>(low * std::exp(-t / 0.030));
    }
    return out;
}

struct Entry
{
    const char* name;
    Maker make;
    bool playOnce = false; // a short hit: it never loops
};

const std::array<Entry, 8>& entries()
{
    static const std::array<Entry, 8> list{{
        {"White", makeWhite},
        {"Pink", makePink},
        {"Brown", makeBrown},
        {"Crackle", makeCrackle},
        {"Metal", makeMetal},
        {"Digital", makeDigital},
        {"Air Burst", makeAirBurst, true},
        {"Thump", makeThump, true},
    }};
    return list;
}
} // namespace

int FactoryNoises::count() noexcept
{
    return static_cast<int>(entries().size());
}

const char* FactoryNoises::name(int index) noexcept
{
    return index >= 0 && index < count() ? entries()[static_cast<size_t>(index)].name : "";
}

std::unique_ptr<const NoiseSample> FactoryNoises::build(int index)
{
    auto sample = std::make_unique<NoiseSample>();
    sample->sampleRate = kSampleRate;
    const auto& entry = entries()[static_cast<size_t>(index >= 0 && index < count() ? index : 0)];
    sample->samples = entry.make();
    sample->playOnce = entry.playOnce;

    // Remove any offset, then scale to the same peak for all.
    double mean = 0.0;
    for (float s : sample->samples)
        mean += s;
    mean /= static_cast<double>(sample->samples.size());

    float peak = 0.0f;
    for (float& s : sample->samples)
    {
        s = static_cast<float>(s - mean);
        peak = std::max(peak, std::abs(s));
    }
    if (peak > 1.0e-9f)
        for (float& s : sample->samples)
            s /= peak;
    return sample;
}

} // namespace fcm::dsp

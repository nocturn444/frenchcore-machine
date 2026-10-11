// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/eq/EqEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kInactiveGainDb = 0.01; // below this a band is skipped: flat is bit-exact

float finiteOrZero(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}
} // namespace

const EffectDescriptor& EqEffect::descriptor() const
{
    static const EffectDescriptor d{"eq", "EQ", 1};
    return d;
}

std::span<const ParameterSpec> EqEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"low.freq", "Low Freq", ParameterKind::number, 20.0f, 500.0f, 1.0f, 100.0f, 100.0f, "Hz", {}},
        {"low.gain", "Low Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"low.q", "Low Q", ParameterKind::number, 0.3f, 2.0f, 0.01f, 0.71f, 0.0f, "", {}},
        {"mid.freq", "Mid Freq", ParameterKind::number, 60.0f, 8000.0f, 1.0f, 500.0f, 500.0f, "Hz", {}},
        {"mid.gain", "Mid Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"mid.q", "Mid Q", ParameterKind::number, 0.2f, 10.0f, 0.01f, 1.0f, 1.0f, "", {}},
        {"high.freq",
         "High Freq",
         ParameterKind::number,
         1000.0f,
         20000.0f,
         10.0f,
         8000.0f,
         6000.0f,
         "Hz",
         {}},
        {"high.gain", "High Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"high.q", "High Q", ParameterKind::number, 0.3f, 2.0f, 0.01f, 0.71f, 0.0f, "", {}},
    };
    return specs;
}

void EqEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);
    reset();
}

void EqEffect::reset() noexcept
{
    for (auto& band : bands)
        for (auto& channel : band.state)
            channel.fill(0.0);
}

EqEffect::Coefficients EqEffect::design(Shape shape, double rate, double frequencyHz, double q,
                                        double gainDb) noexcept
{
    // Stable ranges: below Nyquist, a positive Q, and a gain the cookbook can handle.
    const double f = std::clamp(std::isfinite(frequencyHz) ? frequencyHz : 1000.0, 10.0, 0.45 * rate);
    const double quality = std::clamp(std::isfinite(q) ? q : 0.71, 0.1, 20.0);
    const double db = std::clamp(std::isfinite(gainDb) ? gainDb : 0.0, -30.0, 30.0);

    const double A = std::pow(10.0, db / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f / rate;
    const double cosW = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * quality);
    const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

    double b0, b1, b2, a0, a1, a2;
    switch (shape)
    {
    case Shape::lowShelf:
        b0 = A * ((A + 1.0) - (A - 1.0) * cosW + twoSqrtAAlpha);
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW);
        b2 = A * ((A + 1.0) - (A - 1.0) * cosW - twoSqrtAAlpha);
        a0 = (A + 1.0) + (A - 1.0) * cosW + twoSqrtAAlpha;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW);
        a2 = (A + 1.0) + (A - 1.0) * cosW - twoSqrtAAlpha;
        break;
    case Shape::highShelf:
        b0 = A * ((A + 1.0) + (A - 1.0) * cosW + twoSqrtAAlpha);
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW);
        b2 = A * ((A + 1.0) + (A - 1.0) * cosW - twoSqrtAAlpha);
        a0 = (A + 1.0) - (A - 1.0) * cosW + twoSqrtAAlpha;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW);
        a2 = (A + 1.0) - (A - 1.0) * cosW - twoSqrtAAlpha;
        break;
    case Shape::peak:
    default:
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cosW;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = -2.0 * cosW;
        a2 = 1.0 - alpha / A;
        break;
    }

    Coefficients c;
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

void EqEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 9)
        return;

    constexpr Shape shapes[kBands] = {Shape::lowShelf, Shape::peak, Shape::highShelf};
    for (int i = 0; i < kBands; ++i)
    {
        const auto base = static_cast<size_t>(i) * 3;
        const double gain = finiteOrZero(values[base + 1]);
        auto& band = bands[static_cast<size_t>(i)];
        band.active = std::abs(gain) >= kInactiveGainDb;
        if (band.active)
            band.coefficients = design(shapes[i], sampleRate, finiteOrZero(values[base]),
                                       finiteOrZero(values[base + 2]), gain);
    }
}

void EqEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();

    for (int c = 0; c < numChannels; ++c)
    {
        float* data = buffer.getWritePointer(c);
        for (int i = 0; i < numSamples; ++i)
            data[i] = finiteOrZero(data[i]);

        for (auto& band : bands)
        {
            if (!band.active)
                continue;

            const auto& k = band.coefficients;
            auto& state = band.state[static_cast<size_t>(c)];
            for (int i = 0; i < numSamples; ++i)
            {
                const double x = data[i];
                const double y = k.b0 * x + state[0];
                state[0] = k.b1 * x - k.a1 * y + state[1];
                state[1] = k.b2 * x - k.a2 * y;
                data[i] = static_cast<float>(y);
            }
        }

        for (int i = 0; i < numSamples; ++i)
            data[i] = finiteOrZero(data[i]);
    }
}

} // namespace fcm::dsp

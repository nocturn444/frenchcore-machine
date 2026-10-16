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
    static const EffectDescriptor d{"eq", "EQ", 1, 1.0f, true};
    return d;
}

std::span<const ParameterSpec> EqEffect::parameters() const
{
    static constexpr std::string_view kTypeNames[] = {"Bell", "Low Shelf", "High Shelf", "Low Cut",
                                                      "High Cut"};
    // The first number of each band is the entry of its default Type.
    static const ParameterSpec specs[] = {
        {"b1.on", "Band 1 On", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
        {"b1.type", "Band 1 Type", ParameterKind::choice, 0.0f, 4.0f, 1.0f, 1.0f, 0.0f, "", kTypeNames},
        {"b1.freq", "Band 1 Freq", ParameterKind::number, 20.0f, 20000.0f, 1.0f, 80.0f, 1000.0f, "Hz", {}},
        {"b1.gain", "Band 1 Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"b1.q", "Band 1 Q", ParameterKind::number, 0.1f, 10.0f, 0.01f, 0.71f, 1.0f, "", {}},

        {"b2.on", "Band 2 On", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
        {"b2.type", "Band 2 Type", ParameterKind::choice, 0.0f, 4.0f, 1.0f, 0.0f, 0.0f, "", kTypeNames},
        {"b2.freq", "Band 2 Freq", ParameterKind::number, 20.0f, 20000.0f, 1.0f, 400.0f, 1000.0f, "Hz", {}},
        {"b2.gain", "Band 2 Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"b2.q", "Band 2 Q", ParameterKind::number, 0.1f, 10.0f, 0.01f, 1.0f, 1.0f, "", {}},

        {"b3.on", "Band 3 On", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
        {"b3.type", "Band 3 Type", ParameterKind::choice, 0.0f, 4.0f, 1.0f, 0.0f, 0.0f, "", kTypeNames},
        {"b3.freq", "Band 3 Freq", ParameterKind::number, 20.0f, 20000.0f, 1.0f, 2500.0f, 1000.0f, "Hz", {}},
        {"b3.gain", "Band 3 Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"b3.q", "Band 3 Q", ParameterKind::number, 0.1f, 10.0f, 0.01f, 1.0f, 1.0f, "", {}},

        {"b4.on", "Band 4 On", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
        {"b4.type", "Band 4 Type", ParameterKind::choice, 0.0f, 4.0f, 1.0f, 2.0f, 0.0f, "", kTypeNames},
        {"b4.freq", "Band 4 Freq", ParameterKind::number, 20.0f, 20000.0f, 1.0f, 8000.0f, 1000.0f, "Hz", {}},
        {"b4.gain", "Band 4 Gain", ParameterKind::number, -18.0f, 18.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"b4.q", "Band 4 Q", ParameterKind::number, 0.1f, 10.0f, 0.01f, 0.71f, 1.0f, "", {}},
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
    case Shape::lowCut: // 2nd-order high-pass
        b0 = (1.0 + cosW) / 2.0;
        b1 = -(1.0 + cosW);
        b2 = (1.0 + cosW) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosW;
        a2 = 1.0 - alpha;
        break;
    case Shape::highCut: // 2nd-order low-pass
        b0 = (1.0 - cosW) / 2.0;
        b1 = 1.0 - cosW;
        b2 = (1.0 - cosW) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosW;
        a2 = 1.0 - alpha;
        break;
    case Shape::bell:
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

bool EqEffect::isActive(bool on, Shape shape, double gainDb) noexcept
{
    const bool isCut = shape == Shape::lowCut || shape == Shape::highCut;
    return on && (isCut || (std::isfinite(gainDb) && std::abs(gainDb) >= kInactiveGainDb));
}

double EqEffect::magnitudeDb(const Coefficients& k, double rate, double hz) noexcept
{
    // |H(e^jw)| from the coefficients, evaluated with z^-1 = e^-jw.
    const double w = 2.0 * std::numbers::pi * std::clamp(hz, 1.0, 0.5 * rate) / rate;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    const double numRe = k.b0 + k.b1 * c1 + k.b2 * c2;
    const double numIm = -(k.b1 * s1 + k.b2 * s2);
    const double denRe = 1.0 + k.a1 * c1 + k.a2 * c2;
    const double denIm = -(k.a1 * s1 + k.a2 * s2);
    const double num = numRe * numRe + numIm * numIm;
    const double den = std::max(denRe * denRe + denIm * denIm, 1.0e-30);
    return 10.0 * std::log10(std::max(num / den, 1.0e-30));
}

void EqEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < static_cast<size_t>(kBands * kValuesPerBand))
        return;

    for (int i = 0; i < kBands; ++i)
    {
        const auto base = static_cast<size_t>(i * kValuesPerBand);
        const bool on = finiteOrZero(values[base]) >= 0.5f;
        const int type = std::clamp(static_cast<int>(std::lround(finiteOrZero(values[base + 1]))), 0, 4);
        const auto shape = static_cast<Shape>(type);
        const double gain = finiteOrZero(values[base + 3]);

        auto& band = bands[static_cast<size_t>(i)];
        // A bell or shelf with no gain does nothing, so it is skipped: flat is bit-exact.
        band.active = isActive(on, shape, gain);
        if (band.active)
            band.coefficients = design(shape, sampleRate, finiteOrZero(values[base + 2]),
                                       finiteOrZero(values[base + 4]), gain);
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

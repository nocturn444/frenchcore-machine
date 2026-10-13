// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/stereo/StereoEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
// Q of a second-order Butterworth section; two in series make a Linkwitz-Riley 4th-order filter.
constexpr double kButterworthQ = 1.0 / std::numbers::sqrt2_v<double>;

float finiteOr(float x, float fallback) noexcept
{
    return std::isfinite(x) ? x : fallback;
}

// Coefficients of the first-order all-pass stages of the decorrelator: distinct, alternating in sign,
// so the phase shift differs from one frequency to the next.
constexpr std::array<double, 6> kAllpassCoefficients{0.62, -0.47, 0.33, -0.71, 0.54, -0.26};
} // namespace

const EffectDescriptor& StereoEffect::descriptor() const
{
    static const EffectDescriptor d{"stereo", "Stereo", 1};
    return d;
}

std::span<const ParameterSpec> StereoEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"xover1", "Crossover 1", ParameterKind::number, 40.0f, 400.0f, 1.0f, 120.0f, 120.0f, "Hz", {}},
        {"xover2", "Crossover 2", ParameterKind::number, 400.0f, 8000.0f, 1.0f, 2500.0f, 2500.0f, "Hz", {}},
        {"lowwidth", "Low Width", ParameterKind::number, 0.0f, 200.0f, 1.0f, 100.0f, 0.0f, "%", {}},
        {"midwidth", "Mid Width", ParameterKind::number, 0.0f, 200.0f, 1.0f, 100.0f, 0.0f, "%", {}},
        {"highwidth", "High Width", ParameterKind::number, 0.0f, 200.0f, 1.0f, 100.0f, 0.0f, "%", {}},
        {"monobelow", "Mono Below", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
    };
    return specs;
}

StereoEffect::Biquad StereoEffect::lowPass(double rate, double frequencyHz) noexcept
{
    const double f = std::clamp(frequencyHz, 10.0, 0.45 * rate);
    const double w0 = 2.0 * std::numbers::pi * f / rate;
    const double cosW = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * kButterworthQ);
    const double a0 = 1.0 + alpha;

    Biquad k;
    k.b0 = (1.0 - cosW) * 0.5 / a0;
    k.b1 = (1.0 - cosW) / a0;
    k.b2 = k.b0;
    k.a1 = -2.0 * cosW / a0;
    k.a2 = (1.0 - alpha) / a0;
    return k;
}

StereoEffect::Biquad StereoEffect::highPass(double rate, double frequencyHz) noexcept
{
    const double f = std::clamp(frequencyHz, 10.0, 0.45 * rate);
    const double w0 = 2.0 * std::numbers::pi * f / rate;
    const double cosW = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * kButterworthQ);
    const double a0 = 1.0 + alpha;

    Biquad k;
    k.b0 = (1.0 + cosW) * 0.5 / a0;
    k.b1 = -(1.0 + cosW) / a0;
    k.b2 = k.b0;
    k.a1 = -2.0 * cosW / a0;
    k.a2 = (1.0 - alpha) / a0;
    return k;
}

double StereoEffect::Decorrelator::run(double input) noexcept
{
    double x = input;
    for (size_t i = 0; i < x1.size(); ++i)
    {
        // First-order all-pass: y = -a x + x[n-1] + a y[n-1]; unit magnitude, frequency-dependent phase.
        const double a = kAllpassCoefficients[i];
        const double y = -a * x + x1[i] + a * y1[i];
        x1[i] = x;
        y1[i] = y;
        x = y;
    }
    return x;
}

void StereoEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, 2);
    reset();
}

void StereoEffect::reset() noexcept
{
    splits = {};
    decorrelators = {};
}

void StereoEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 6)
        return;

    // Crossover 2 stays above Crossover 1, and both below Nyquist.
    const double x1 = std::clamp(static_cast<double>(finiteOr(values[0], 120.0f)), 20.0, 0.30 * sampleRate);
    const double x2 =
        std::clamp(static_cast<double>(finiteOr(values[1], 2500.0f)), x1 * 1.5, 0.45 * sampleRate);
    lowPass1 = lowPass(sampleRate, x1);
    highPass1 = highPass(sampleRate, x1);
    lowPass2 = lowPass(sampleRate, x2);
    highPass2 = highPass(sampleRate, x2);

    const auto width = [&](size_t index)
    { return std::clamp(static_cast<double>(finiteOr(values[index], 100.0f)), 0.0, 200.0) * 0.01; };
    widths = {width(2), width(3), width(4)};
    if (finiteOr(values[5], 1.0f) >= 0.5f)
        widths[0] = 0.0; // Mono Below
}

void StereoEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2 || channels < 2)
    {
        // Nothing to widen in one channel; only a sample that is not a number is replaced.
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            float* data = buffer.getWritePointer(c);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (!std::isfinite(data[i]))
                    data[i] = 0.0f;
        }
        return;
    }

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        double bands[2][3];
        for (int c = 0; c < 2; ++c)
        {
            const double input =
                std::isfinite((c == 0 ? left : right)[i]) ? (c == 0 ? left : right)[i] : 0.0f;
            auto& split = splits[static_cast<size_t>(c)];

            const double low = split.low1.run(lowPass1, input);
            const double rest = split.high1.run(highPass1, input);
            const double mid = split.low2.run(lowPass2, rest);
            const double high = split.high2.run(highPass2, rest);

            // The low band gets the Crossover 2 all-pass (its low-pass plus its high-pass), so that
            // the three bands stay in phase with each other.
            bands[c][0] = split.alignLow.run(lowPass2, low) + split.alignHigh.run(highPass2, low);
            bands[c][1] = mid;
            bands[c][2] = high;
        }

        double outLeft = 0.0;
        double outRight = 0.0;
        for (size_t band = 0; band < 3; ++band)
        {
            const double l = bands[0][band];
            const double r = bands[1][band];
            const double w = widths[band];
            const double mid = 0.5 * (l + r);
            const double side = 0.5 * (l - r);

            // Above 100 % a decorrelated copy of the mid is added to the sides, with the loudness kept.
            const double extra = std::max(0.0, w - 1.0);
            const double decorrelated = extra > 0.0 ? decorrelators[band].run(mid) : 0.0;
            const double compensation = 1.0 / std::sqrt(1.0 + extra * extra);
            const double sideOut = (w * side + extra * decorrelated) * compensation;
            const double midOut = mid * compensation;

            outLeft += midOut + sideOut;
            outRight += midOut - sideOut;
        }

        left[i] = std::isfinite(outLeft) ? static_cast<float>(outLeft) : 0.0f;
        right[i] = std::isfinite(outRight) ? static_cast<float>(outRight) : 0.0f;
    }
}

} // namespace fcm::dsp

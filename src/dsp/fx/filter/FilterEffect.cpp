// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/filter/FilterEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.010;
constexpr double kMaxCutoffOfRate = 0.45; // the cutoff stays below this share of the sample rate
constexpr double kMinDamping = 0.05;      // Q of 20 at the most
constexpr double kButterworth2 = 1.4142135623730951;
constexpr double kButterworth4First = 1.8477590650225735;  // 2 cos(3 pi / 8)
constexpr double kButterworth4Second = 0.7653668647301796; // 2 cos(pi / 8)
constexpr double kDriveShape = 15.0;

float finiteOrZero(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}

enum Param
{
    pType,
    pSlope,
    pCutoff,
    pResonance,
    pDrive
};
} // namespace

const EffectDescriptor& FilterEffect::descriptor() const
{
    static const EffectDescriptor d{"filter", "Filter", 1};
    return d;
}

std::span<const ParameterSpec> FilterEffect::parameters() const
{
    static constexpr std::string_view kTypeNames[] = {"Low Pass", "High Pass", "Band Pass", "Notch"};
    static constexpr std::string_view kSlopeNames[] = {"12 dB", "24 dB"};
    static const ParameterSpec specs[] = {
        {"type", "Type", ParameterKind::choice, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f, "", kTypeNames},
        {"slope", "Slope", ParameterKind::choice, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "", kSlopeNames},
        {"cutoff", "Cutoff", ParameterKind::number, 20.0f, 20000.0f, 1.0f, 20000.0f, 1000.0f, "Hz", {}},
        {"resonance", "Resonance", ParameterKind::number, 0.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
        {"drive", "Drive", ParameterKind::number, 0.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
    };
    return specs;
}

double FilterEffect::damping12(double r) noexcept
{
    r = std::clamp(std::isfinite(r) ? r : 0.0, 0.0, 1.0);
    return kButterworth2 - (kButterworth2 - kMinDamping) * r;
}

double FilterEffect::damping24First() noexcept
{
    return kButterworth4First;
}

double FilterEffect::damping24Second(double r) noexcept
{
    r = std::clamp(std::isfinite(r) ? r : 0.0, 0.0, 1.0);
    return kButterworth4Second - (kButterworth4Second - kMinDamping) * r;
}

double FilterEffect::saturate(double x, double d) noexcept
{
    d = std::clamp(std::isfinite(d) ? d : 0.0, 0.0, 1.0);
    const double a = kDriveShape * d * d;
    if (a < 1.0e-6)
        return x; // the limit of tanh(a x) / tanh(a) as a goes to 0
    return std::tanh(a * x) / std::tanh(a);
}

void FilterEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);
    cutoff.reset(sampleRate, kSmoothSeconds);
    resonance.reset(sampleRate, kSmoothSeconds);
    drive.reset(sampleRate, kSmoothSeconds);
    reset();
}

void FilterEffect::reset() noexcept
{
    for (auto& channel : stages)
        for (auto& stage : channel)
            stage = {};
    snapSmoothers = true;
}

void FilterEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 5)
        return;

    type = static_cast<Type>(std::clamp(static_cast<int>(std::lround(finiteOrZero(values[pType]))), 0, 3));
    steep = finiteOrZero(values[pSlope]) >= 0.5f;

    const float maxCutoff = static_cast<float>(kMaxCutoffOfRate * sampleRate);
    const float cutoffTarget =
        std::clamp(std::isfinite(values[pCutoff]) ? values[pCutoff] : 20000.0f, 20.0f, maxCutoff);
    const float resonanceTarget = std::clamp(finiteOrZero(values[pResonance]) * 0.01f, 0.0f, 1.0f);
    const float driveTarget = std::clamp(finiteOrZero(values[pDrive]) * 0.01f, 0.0f, 1.0f);

    if (snapSmoothers)
    {
        cutoff.setCurrentAndTargetValue(cutoffTarget);
        resonance.setCurrentAndTargetValue(resonanceTarget);
        drive.setCurrentAndTargetValue(driveTarget);
        snapSmoothers = false;
    }
    else
    {
        cutoff.setTargetValue(cutoffTarget);
        resonance.setTargetValue(resonanceTarget);
        drive.setTargetValue(driveTarget);
    }
}

double FilterEffect::tick(Stage& stage, double input, double g, double k, double a1, double a2, double a3,
                          Type kind) noexcept
{
    // Zavalishin's TPT state variable filter: v1 is the band, v2 the low pass.
    const double v3 = input - stage.ic2;
    const double v1 = a1 * stage.ic1 + a2 * v3;
    const double v2 = stage.ic2 + a2 * stage.ic1 + a3 * v3;
    stage.ic1 = 2.0 * v1 - stage.ic1;
    stage.ic2 = 2.0 * v2 - stage.ic2;
    (void)g;

    switch (kind)
    {
    case Type::highPass:
        return input - k * v1 - v2;
    case Type::bandPass:
        return k * v1; // constant peak gain of 1
    case Type::notch:
        return input - k * v1;
    case Type::lowPass:
    default:
        return v2;
    }
}

void FilterEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    std::array<float*, kMaxChannels> data{};
    for (int c = 0; c < numChannels; ++c)
        data[static_cast<size_t>(c)] = buffer.getWritePointer(c);

    for (int i = 0; i < numSamples; ++i)
    {
        const double fc = static_cast<double>(cutoff.getNextValue());
        const double r = static_cast<double>(resonance.getNextValue());
        const double d = static_cast<double>(drive.getNextValue());

        // One tan per sample, shared by the channels and by both stages of the 24 dB filter.
        const double g = std::tan(std::numbers::pi * fc / sampleRate);
        const double kFirst = steep ? damping24First() : damping12(r);
        const double kSecond = damping24Second(r);

        const double a1First = 1.0 / (1.0 + g * (g + kFirst));
        const double a2First = g * a1First;
        const double a3First = g * a2First;
        const double a1Second = 1.0 / (1.0 + g * (g + kSecond));
        const double a2Second = g * a1Second;
        const double a3Second = g * a2Second;

        for (int c = 0; c < numChannels; ++c)
        {
            float& sample = data[static_cast<size_t>(c)][i];
            auto& channel = stages[static_cast<size_t>(c)];

            double x = std::isfinite(sample) ? static_cast<double>(sample) : 0.0;
            x = saturate(x, d);

            double y = tick(channel[0], x, g, kFirst, a1First, a2First, a3First, type);
            if (steep)
                y = tick(channel[1], y, g, kSecond, a1Second, a2Second, a3Second, type);

            sample = std::isfinite(y) ? static_cast<float>(y) : 0.0f;
        }
    }
}

} // namespace fcm::dsp

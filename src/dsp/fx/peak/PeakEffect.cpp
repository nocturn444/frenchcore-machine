// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/peak/PeakEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.010;
constexpr float kBoostDb = 18.0f; // at Amount 100 %, at the chosen frequency
constexpr float kDriveDb = 24.0f; // at Amount 100 %, before the clipper
// A band-pass made of a one-pole high-pass and a one-pole low-pass at the same frequency has a gain of
// 1/2 (and no phase shift) at that frequency, so the band-pass is added with twice what the boost needs.
constexpr float kBandPassPeak = 0.5f;

enum Param
{
    pFreq,
    pAmount,
    pHardness
};

float finiteOr(float x, float fallback) noexcept
{
    return std::isfinite(x) ? x : fallback;
}
} // namespace

const EffectDescriptor& PeakEffect::descriptor() const
{
    static const EffectDescriptor d{"peak", "Peak", 1, 1.0f, false};
    return d;
}

std::span<const ParameterSpec> PeakEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"freq", "Freq", ParameterKind::number, 20.0f, 5000.0f, 1.0f, 150.0f, 300.0f, "Hz", {}},
        {"amount", "Amount", ParameterKind::number, 0.0f, 100.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"hardness", "Hardness", ParameterKind::number, 0.0f, 100.0f, 0.1f, 30.0f, 0.0f, "%", {}},
    };
    return specs;
}

void PeakEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);

    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        // The same polyphase IIR half-band set-up as the Distortion (integer latency).
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(channels), i + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, true);
        oversamplers[i]->initProcessing(static_cast<size_t>(std::max(1, spec.maxBlockSize)));
    }

    hardnessPerSample.assign(static_cast<size_t>(std::max(1, spec.maxBlockSize)), 0.0f);
    freqHz.reset(sampleRate, kSmoothSeconds);
    amountUnit.reset(sampleRate, kSmoothSeconds);
    hardnessUnit.reset(sampleRate, kSmoothSeconds);
    snapSmoothers = true;
    reset();
}

void PeakEffect::reset() noexcept
{
    highPassState.fill(0.0f);
    lowPassState.fill(0.0f);
    for (auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            oversampler->reset();
    snapSmoothers = true;
}

void PeakEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 3)
        return;

    const float freq = std::clamp(finiteOr(values[pFreq], 150.0f), 20.0f, 5000.0f);
    const float amount = std::clamp(finiteOr(values[pAmount], 50.0f), 0.0f, 100.0f) * 0.01f;
    const float hardness = std::clamp(finiteOr(values[pHardness], 30.0f), 0.0f, 100.0f) * 0.01f;

    if (snapSmoothers)
    {
        freqHz.setCurrentAndTargetValue(freq);
        amountUnit.setCurrentAndTargetValue(amount);
        hardnessUnit.setCurrentAndTargetValue(hardness);
        snapSmoothers = false;
        updateCoefficients(freq, amount);
    }
    else
    {
        freqHz.setTargetValue(freq);
        amountUnit.setTargetValue(amount);
        hardnessUnit.setTargetValue(hardness);
    }
}

void PeakEffect::updateCoefficients(float freq, float amount) noexcept
{
    // The corner is limited to 0.45 of the base sample rate.
    const double corner = std::min(static_cast<double>(freq), 0.45 * sampleRate);
    const double g = std::tan(std::numbers::pi * corner / sampleRate);
    filterG = static_cast<float>(g / (1.0 + g));

    const float boost = std::pow(10.0f, kBoostDb * amount / 20.0f) - 1.0f;
    boostGain = boost / kBandPassPeak;
    driveGain = std::pow(10.0f, kDriveDb * amount / 20.0f);
}

juce::dsp::Oversampling<float>* PeakEffect::activeOversampler() noexcept
{
    switch (factor)
    {
    case 2:
        return oversamplers[0].get();
    case 4:
        return oversamplers[1].get();
    case 8:
        return oversamplers[2].get();
    default:
        return nullptr;
    }
}

void PeakEffect::setOversampling(int newFactor) noexcept
{
    if (newFactor != 1 && newFactor != 2 && newFactor != 4 && newFactor != 8)
        newFactor = 1;
    if (newFactor == factor)
        return;

    factor = newFactor;
    if (auto* oversampler = activeOversampler())
        oversampler->reset();
}

int PeakEffect::latencySamples(int oversamplingFactor) const noexcept
{
    size_t index = 0;
    switch (oversamplingFactor)
    {
    case 2:
        index = 0;
        break;
    case 4:
        index = 1;
        break;
    case 8:
        index = 2;
        break;
    default:
        return 0;
    }
    return oversamplers[index] != nullptr
               ? static_cast<int>(std::lround(oversamplers[index]->getLatencyInSamples()))
               : 0;
}

float PeakEffect::clip(float u, float hardness) noexcept
{
    return (1.0f - hardness) * std::tanh(u) + hardness * std::clamp(u, -1.0f, 1.0f);
}

void PeakEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int total = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int capacity = static_cast<int>(hardnessPerSample.size());
    if (total <= 0 || numChannels <= 0 || capacity <= 0)
        return;

    // A buffer longer than the one announced in prepare() is processed in pieces.
    for (int start = 0; start < total; start += capacity)
    {
        std::array<float*, kMaxChannels> pointers{};
        for (int c = 0; c < numChannels; ++c)
            pointers[static_cast<size_t>(c)] = buffer.getWritePointer(c) + start;
        juce::AudioBuffer<float> piece(pointers.data(), numChannels, std::min(capacity, total - start));
        processPiece(piece);
    }
}

void PeakEffect::processPiece(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // 1. The boost and the drive, at the base rate, with the settings smoothed per sample. The hardness
    // of each base-rate sample is kept for the clipper, which may run faster.
    for (int i = 0; i < numSamples; ++i)
    {
        const bool moving = freqHz.isSmoothing() || amountUnit.isSmoothing();
        const float freq = freqHz.getNextValue();
        const float amount = amountUnit.getNextValue();
        hardnessPerSample[static_cast<size_t>(i)] = hardnessUnit.getNextValue();
        if (moving)
            updateCoefficients(freq, amount);

        for (int c = 0; c < numChannels; ++c)
        {
            const auto ci = static_cast<size_t>(c);
            float* data = buffer.getWritePointer(c);
            const float x = std::isfinite(data[i]) ? data[i] : 0.0f;

            // High-pass, then low-pass, both TPT one-poles at the same corner.
            const float v1 = (x - highPassState[ci]) * filterG;
            const float lowOfX = v1 + highPassState[ci];
            highPassState[ci] = lowOfX + v1;
            const float highOfX = x - lowOfX;

            const float v2 = (highOfX - lowPassState[ci]) * filterG;
            const float bandPass = v2 + lowPassState[ci];
            lowPassState[ci] = bandPass + v2;

            data[i] = (x + boostGain * bandPass) * driveGain;
        }
    }

    // 2. The clipper, oversampled when the factor is above 1.
    if (auto* oversampler = activeOversampler())
    {
        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(), static_cast<size_t>(numChannels),
                                           static_cast<size_t>(numSamples));
        auto up = oversampler->processSamplesUp(block);
        const size_t ratio = std::max<size_t>(up.getNumSamples() / static_cast<size_t>(numSamples), 1);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* data = up.getChannelPointer(c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                data[i] = clip(data[i],
                               hardnessPerSample[std::min(i / ratio, static_cast<size_t>(numSamples - 1))]);
        }
        oversampler->processSamplesDown(block);
    }
    else
    {
        for (int c = 0; c < numChannels; ++c)
        {
            float* data = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i)
                data[i] = clip(data[i], hardnessPerSample[static_cast<size_t>(i)]);
        }
    }

    for (int c = 0; c < numChannels; ++c)
    {
        float* data = buffer.getWritePointer(c);
        for (int i = 0; i < numSamples; ++i)
            if (!std::isfinite(data[i]))
                data[i] = 0.0f;
    }
}

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/distortion/DistortionEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.020;
constexpr float kDriveDbPerPercent = 0.48f; // 100 % = 48 dB
constexpr double kDcBlockerHz = 5.0;

constexpr std::string_view kTypeNames[] = {"Soft Clip", "Hard Clip", "Overdrive", "Foldback", "Bitcrush"};

// Indices into the values given to setParameters(), in the order of parameters().
enum Param
{
    pType,
    pDrive,
    pPreGain,
    pPostGain,
    pTone,
    pBits,
    pRate
};

float finiteOrZero(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}
} // namespace

const EffectDescriptor& DistortionEffect::descriptor() const
{
    static const EffectDescriptor d{"distortion", "Distortion", 1};
    return d;
}

std::span<const ParameterSpec> DistortionEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"type", "Type", ParameterKind::choice, 0.0f, static_cast<float>(kNumTypes - 1), 1.0f, 0.0f, 0.0f, "",
         kTypeNames},
        {"drive", "Drive", ParameterKind::number, 0.0f, 100.0f, 1.0f, 25.0f, 0.0f, "%", {}},
        {"pregain", "Pre-gain", ParameterKind::number, -24.0f, 24.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
        {"postgain", "Post-gain", ParameterKind::number, -48.0f, 12.0f, 0.1f, -6.0f, 0.0f, "dB", {}},
        {"tone", "Tone", ParameterKind::number, 0.0f, 100.0f, 1.0f, 100.0f, 0.0f, "%", {}},
        {"bits", "Bits", ParameterKind::integer, 2.0f, 16.0f, 1.0f, 8.0f, 0.0f, "bits", {}},
        {"rate", "Rate", ParameterKind::number, 1.0f, 32.0f, 0.1f, 4.0f, 4.0f, "x", {}},
    };
    return specs;
}

void DistortionEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);

    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        // Polyphase IIR half-band filters: low latency; integer latency so the chain can compensate
        // it exactly (docs/specs/fx-distortion.md §4).
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(channels), i + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, true);
        oversamplers[i]->initProcessing(static_cast<size_t>(std::max(1, spec.maxBlockSize)));
    }

    preGainDb.reset(sampleRate, kSmoothSeconds);
    postGainDb.reset(sampleRate, kSmoothSeconds);
    dcCoefficient = static_cast<float>(1.0 - 2.0 * std::numbers::pi * kDcBlockerHz / sampleRate);
    snapSmoothers = true;
    reset();
}

void DistortionEffect::reset() noexcept
{
    dcInput.fill(0.0f);
    dcOutput.fill(0.0f);
    toneState.fill(0.0f);
    crushHeld.fill(0.0f);
    crushPhase.fill(1.0f); // the first sample is taken at once
    for (auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            oversampler->reset();
    snapSmoothers = true;
}

void DistortionEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 7)
        return;

    type = static_cast<Type>(
        std::clamp(static_cast<int>(std::lround(finiteOrZero(values[pType]))), 0, kNumTypes - 1));

    const float drive = std::clamp(finiteOrZero(values[pDrive]), 0.0f, 100.0f);
    const float preDb =
        std::clamp(finiteOrZero(values[pPreGain]), -24.0f, 24.0f) + kDriveDbPerPercent * drive;
    const float postDb = std::clamp(finiteOrZero(values[pPostGain]), -48.0f, 12.0f);
    if (snapSmoothers)
    {
        preGainDb.setCurrentAndTargetValue(preDb);
        postGainDb.setCurrentAndTargetValue(postDb);
        snapSmoothers = false;
    }
    else
    {
        preGainDb.setTargetValue(preDb);
        postGainDb.setTargetValue(postDb);
    }

    // Tone: 100 % leaves the signal alone; below that a one-pole low-pass from 1 kHz to 20 kHz.
    const float tone = std::clamp(finiteOrZero(values[pTone]), 0.0f, 100.0f);
    toneActive = tone < 100.0f;
    const double cutoff =
        std::min(1000.0 * std::pow(20.0, static_cast<double>(tone) / 100.0), 0.45 * sampleRate);
    toneCoefficient = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * cutoff / sampleRate));

    bits = std::clamp(static_cast<int>(std::lround(finiteOrZero(values[pBits]))), 2, 16);
    rate = std::clamp(finiteOrZero(values[pRate]), 1.0f, 32.0f);
}

juce::dsp::Oversampling<float>* DistortionEffect::activeOversampler() noexcept
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

void DistortionEffect::setOversampling(int newFactor) noexcept
{
    if (newFactor != 1 && newFactor != 2 && newFactor != 4 && newFactor != 8)
        newFactor = 1;
    if (newFactor == factor)
        return;

    factor = newFactor;
    // The filters of the one now in use hold old audio from when it last ran.
    if (auto* oversampler = activeOversampler())
        oversampler->reset();
}

int DistortionEffect::latencySamples(int oversamplingFactor) const noexcept
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

float DistortionEffect::shape(Type shapeType, float x) noexcept
{
    switch (shapeType)
    {
    case Type::softClip:
        return std::tanh(x);
    case Type::hardClip:
        return std::clamp(x, -1.0f, 1.0f);
    case Type::overdrive:
        // The negative half bends earlier than the positive one: even harmonics.
        return x >= 0.0f ? std::tanh(x) : 0.6f * std::tanh(x / 0.6f);
    case Type::foldback:
    {
        // A triangle wave of period 4: equal to x up to 1, then folding back, for any size of x.
        const float u = x + 1.0f;
        const float m = u - 4.0f * std::floor(u * 0.25f);
        return 1.0f - std::abs(m - 2.0f);
    }
    case Type::bitcrush:
        return std::clamp(x, -1.0f, 1.0f); // the quantiser runs in process()
    }
    return 0.0f;
}

void DistortionEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    // 1. Gain (pre-gain and drive together), at the base rate.
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = juce::Decibels::decibelsToGain(preGainDb.getNextValue(), -200.0f);
        for (int c = 0; c < numChannels; ++c)
        {
            float* data = buffer.getWritePointer(c);
            data[i] = finiteOrZero(data[i]) * gain;
        }
    }

    // 2. The shaper: bitcrush at the base rate; the others oversampled when the factor is above 1.
    if (type == Type::bitcrush)
    {
        const float levels = static_cast<float>(1 << (bits - 1));
        const float step = 1.0f / rate;
        for (int c = 0; c < numChannels; ++c)
        {
            float* data = buffer.getWritePointer(c);
            auto& held = crushHeld[static_cast<size_t>(c)];
            auto& phase = crushPhase[static_cast<size_t>(c)];
            for (int i = 0; i < numSamples; ++i)
            {
                phase += step;
                if (phase >= 1.0f)
                {
                    phase -= 1.0f;
                    held = std::round(shape(Type::bitcrush, data[i]) * levels) / levels;
                }
                data[i] = held;
            }
        }
    }
    else if (auto* oversampler = activeOversampler())
    {
        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(), static_cast<size_t>(numChannels),
                                           static_cast<size_t>(numSamples));
        auto up = oversampler->processSamplesUp(block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* data = up.getChannelPointer(c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                data[i] = shape(type, data[i]);
        }
        oversampler->processSamplesDown(block);
    }
    else
    {
        for (int c = 0; c < numChannels; ++c)
        {
            float* data = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i)
                data[i] = shape(type, data[i]);
        }
    }

    // 3. DC blocker, post-gain and tone, at the base rate.
    for (int i = 0; i < numSamples; ++i)
    {
        const float postGain = juce::Decibels::decibelsToGain(postGainDb.getNextValue(), -200.0f);
        for (int c = 0; c < numChannels; ++c)
        {
            float* data = buffer.getWritePointer(c);
            const auto ci = static_cast<size_t>(c);

            // Only the lopsided Overdrive makes DC; the others stay phase-transparent at low pitches.
            float processed = finiteOrZero(data[i]);
            if (type == Type::overdrive)
            {
                const float blocked = processed - dcInput[ci] + dcCoefficient * dcOutput[ci];
                dcInput[ci] = processed;
                dcOutput[ci] = blocked;
                processed = blocked;
            }

            float out = processed * postGain;
            if (toneActive)
            {
                toneState[ci] += toneCoefficient * (out - toneState[ci]);
                out = toneState[ci];
            }
            data[i] = finiteOrZero(out);
        }
    }
}

} // namespace fcm::dsp

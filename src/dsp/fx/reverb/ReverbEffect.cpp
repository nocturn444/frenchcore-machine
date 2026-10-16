// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/reverb/ReverbEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.050;
constexpr double kMaxPredelayMs = 200.0;
constexpr double kMinScale = 0.3; // Size 0 %
constexpr double kMaxScale = 1.5; // Size 100 %
constexpr double kDampingMax = 0.8;
constexpr float kDiffusion = 0.6f;
constexpr double kReferenceRate = 48000.0;

// Mutually prime delay lengths in samples at 48 kHz, about 10 to 57 ms at scale 1 (docs/specs/fx-reverb.md).
constexpr std::array<int, ReverbEffect::kLines> kBaseLengths{509, 677, 877, 1109, 1409, 1783, 2203, 2711};
// The diffusers, in samples at 48 kHz (3 to 9 ms).
constexpr std::array<int, 4> kDiffuserLengths{149, 211, 337, 419};
// Two lines wobble a little, slowly, so the tail does not ring metallically.
constexpr std::array<int, 2> kModulatedLines{2, 5};
constexpr std::array<double, 2> kModulationHz{0.37, 0.51};
constexpr double kModulationSamples = 6.0; // at 48 kHz

// Which way each line goes into the left and right outputs: orthogonal patterns, so the two outputs differ.
constexpr std::array<float, ReverbEffect::kLines> kLeftTaps{1, -1, 1, -1, 1, -1, 1, -1};
constexpr std::array<float, ReverbEffect::kLines> kRightTaps{1, 1, -1, -1, 1, 1, -1, -1};
constexpr float kOutputGain = 0.35f;
constexpr float kInputGain = 0.5f;

float finiteOrZero(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}

enum Param
{
    pSize,
    pDecay,
    pDamping,
    pPredelay,
    pLowCut
};
} // namespace

const EffectDescriptor& ReverbEffect::descriptor() const
{
    // A reverb that is only wet is rarely wanted: Mix starts at 30 % (docs/decisions/0011).
    static const EffectDescriptor d{"reverb", "Reverb", 1, 0.3f};
    return d;
}

std::span<const ParameterSpec> ReverbEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"size", "Size", ParameterKind::number, 0.0f, 100.0f, 0.1f, 40.0f, 0.0f, "%", {}},
        {"decay", "Decay", ParameterKind::number, 0.1f, 10.0f, 0.01f, 1.2f, 1.5f, "s", {}},
        {"damping", "Damping", ParameterKind::number, 0.0f, 100.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"predelay", "Pre-delay", ParameterKind::number, 0.0f, 200.0f, 0.1f, 0.0f, 0.0f, "ms", {}},
        {"lowcut", "Low Cut", ParameterKind::number, 20.0f, 1000.0f, 1.0f, 200.0f, 200.0f, "Hz", {}},
    };
    return specs;
}

double ReverbEffect::lineGain(double delaySeconds, double decay) noexcept
{
    return std::pow(10.0, -3.0 * delaySeconds / std::max(decay, 0.01));
}

void ReverbEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);

    const double rateRatio = sampleRate / kReferenceRate;
    const double margin = kModulationSamples * rateRatio + 4.0;
    for (size_t i = 0; i < kLines; ++i)
    {
        baseDelays[i] = kBaseLengths[i] * rateRatio;
        lines[i].prepare(static_cast<int>(std::ceil(baseDelays[i] * kMaxScale + margin)) + 2);
        delays[i].reset(sampleRate, kSmoothSeconds);
    }
    for (size_t i = 0; i < kDiffusers; ++i)
        diffusers[i].prepare(std::max(1, static_cast<int>(std::lround(kDiffuserLengths[i] * rateRatio))));
    predelay.prepare(static_cast<int>(std::ceil(kMaxPredelayMs * 0.001 * sampleRate)) + 4);
    predelaySamples.reset(sampleRate, kSmoothSeconds);

    for (size_t k = 0; k < 2; ++k)
        lfoStep[k] = 2.0 * std::numbers::pi * kModulationHz[k] / sampleRate;
    modulationDepth = kModulationSamples * rateRatio;
    reset();
}

void ReverbEffect::reset() noexcept
{
    for (auto& line : lines)
        line.clear();
    for (auto& diffuser : diffusers)
        diffuser.clear();
    predelay.clear();
    dampState.fill(0.0f);
    lowCutState = {};
    lfoPhase[0] = 0.0;
    lfoPhase[1] = 0.25;
    snapSmoothers = true;
}

void ReverbEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 5)
        return;

    const double size = std::clamp(static_cast<double>(finiteOrZero(values[pSize])), 0.0, 100.0) * 0.01;
    sizeScale = kMinScale + (kMaxScale - kMinScale) * size;
    decaySeconds = std::clamp(static_cast<double>(finiteOrZero(values[pDecay])), 0.1, 10.0);
    damping =
        kDampingMax * std::clamp(static_cast<double>(finiteOrZero(values[pDamping])), 0.0, 100.0) * 0.01;
    const double predelayMs =
        std::clamp(static_cast<double>(finiteOrZero(values[pPredelay])), 0.0, kMaxPredelayMs);
    lowCutHz = std::clamp(static_cast<double>(finiteOrZero(values[pLowCut])), 20.0, 1000.0);
    lowCut = StereoEffect::highPass(sampleRate, std::min(lowCutHz, 0.4 * sampleRate));

    for (size_t i = 0; i < kLines; ++i)
    {
        const double target = baseDelays[i] * sizeScale;
        if (snapSmoothers)
            delays[i].setCurrentAndTargetValue(target);
        else
            delays[i].setTargetValue(target);
    }
    const double predelayTarget = predelayMs * 0.001 * sampleRate;
    if (snapSmoothers)
        predelaySamples.setCurrentAndTargetValue(predelayTarget);
    else
        predelaySamples.setTargetValue(predelayTarget);
    snapSmoothers = false;
}

void ReverbEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    if (numChannels < 1)
        return;

    float* left = buffer.getWritePointer(0);
    float* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float inLeft = finiteOrZero(left[n]);
        const float inRight = right != nullptr ? finiteOrZero(right[n]) : inLeft;

        // The mono input through the pre-delay, the low cut and the diffusers.
        predelay.push(0.5f * (inLeft + inRight));
        float x = predelay.read(std::max(1.0, predelaySamples.getNextValue()));
        x = static_cast<float>(lowCutState.run(lowCut, static_cast<double>(x)));
        for (auto& diffuser : diffusers)
            x = diffuser.run(x, kDiffusion);

        // The lines: read, damp, and give each its gain for the decay time.
        std::array<float, kLines> out{};
        float sum = 0.0f;
        for (size_t i = 0; i < kLines; ++i)
        {
            double delay = delays[i].getNextValue();
            for (size_t k = 0; k < kModulatedLines.size(); ++k)
                if (static_cast<size_t>(kModulatedLines[k]) == i)
                    delay += modulationDepth * std::sin(lfoPhase[k]);

            const float read = lines[i].read(delay);
            dampState[i] = static_cast<float>((1.0 - damping) * read + damping * dampState[i]);
            const auto gain = static_cast<float>(lineGain(delay / sampleRate, decaySeconds));
            out[i] = dampState[i] * gain;
            sum += out[i];
        }
        for (size_t k = 0; k < 2; ++k)
        {
            lfoPhase[k] += lfoStep[k];
            if (lfoPhase[k] > 2.0 * std::numbers::pi)
                lfoPhase[k] -= 2.0 * std::numbers::pi;
        }

        // Householder feedback: y = x - (2 / N) * sum(x), which keeps the energy, plus the input.
        const float mean = 2.0f * sum / static_cast<float>(kLines);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (size_t i = 0; i < kLines; ++i)
        {
            lines[i].push(out[i] - mean + kInputGain * x);
            wetLeft += kLeftTaps[i] * out[i];
            wetRight += kRightTaps[i] * out[i];
        }

        const float outLeft = kOutputGain * wetLeft;
        const float outRight = kOutputGain * wetRight;
        left[n] = std::isfinite(outLeft) ? outLeft : 0.0f;
        if (right != nullptr)
            right[n] = std::isfinite(outRight) ? outRight : 0.0f;
    }
}

} // namespace fcm::dsp

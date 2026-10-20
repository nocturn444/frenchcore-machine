// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/flanger/FlangerEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kDelaySmoothSeconds = 0.020;
constexpr double kSmoothSeconds = 0.010;
constexpr double kSweepOctaves = 2.0; // at Depth 100 %

// Bit 0: Sync off, bit 1: on (ParameterSpec::shownWhen).
constexpr unsigned kFreeRunning = 1u << 0;
constexpr unsigned kSynced = 1u << 1;

enum Param
{
    pDelay,
    pSync,
    pRate,
    pDivision,
    pDepth,
    pFeedback,
    pKeyTrack,
    kNumParams
};

float finiteOr(float x, float fallback) noexcept
{
    return std::isfinite(x) ? x : fallback;
}
} // namespace

const EffectDescriptor& FlangerEffect::descriptor() const
{
    static const EffectDescriptor d{"flanger", "Flanger", 1, 1.0f, false};
    return d;
}

std::span<const ParameterSpec> FlangerEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"delay", "Delay", ParameterKind::number, 0.1f, 10.0f, 0.01f, 2.0f, 2.0f, "ms", {}},
        {"sync", "Sync", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "", {}},
        {"rate",
         "Rate",
         ParameterKind::number,
         0.0f,
         20.0f,
         0.01f,
         0.3f,
         2.0f,
         "Hz",
         {},
         "sync",
         kFreeRunning},
        {"division", "Rate", ParameterKind::choice, 0.0f, 13.0f, 1.0f, 9.0f, 0.0f, "", kLfoDivisionNames,
         "sync", kSynced},
        {"depth", "Depth", ParameterKind::number, 0.0f, 100.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"feedback", "Feedback", ParameterKind::number, -95.0f, 95.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"keytrack", "Key Track", ParameterKind::number, 0.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
    };
    return specs;
}

double FlangerEffect::levelFor(double fb) noexcept
{
    fb = std::clamp(std::isfinite(fb) ? std::abs(fb) : 0.0, 0.0, 0.95);
    const double peak = (1.0 + 1.0 / (1.0 - fb)) * 0.5;
    return std::min(1.0, 2.0 / peak);
}

float FlangerEffect::hermite(float ym1, float y0, float y1, float y2, float t) noexcept
{
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

void FlangerEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);
    lfo.prepare(sampleRate);
    delayMs.reset(sampleRate, kDelaySmoothSeconds);
    depth.reset(sampleRate, kSmoothSeconds);
    feedback.reset(sampleRate, kSmoothSeconds);
    lineSize = static_cast<int>(std::ceil(kLineSeconds * sampleRate)) + 8;
    for (auto& line : lines)
        line.assign(static_cast<size_t>(lineSize), 0.0f);
    reset();
}

void FlangerEffect::reset() noexcept
{
    for (auto& line : lines)
        std::fill(line.begin(), line.end(), 0.0f);
    writeIndex = 0;
    lfo.reset();
    pending.clear();
    snapSmoothers = true;
}

void FlangerEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < kNumParams)
        return;

    sync = finiteOr(values[pSync], 0.0f) >= 0.5f;
    division = std::clamp(static_cast<int>(std::lround(finiteOr(values[pDivision], 9.0f))), 0,
                          static_cast<int>(kLfoDivisions.size()) - 1);
    rateHz = std::clamp(finiteOr(values[pRate], 0.3f), 0.0f, 20.0f);
    keyTrack = std::clamp(finiteOr(values[pKeyTrack], 0.0f) * 0.01f, 0.0f, 1.0f);

    const float delayTarget = std::clamp(finiteOr(values[pDelay], 2.0f), 0.1f, 10.0f);
    const float depthTarget = std::clamp(finiteOr(values[pDepth], 50.0f) * 0.01f, 0.0f, 1.0f);
    const float feedbackTarget = std::clamp(finiteOr(values[pFeedback], 50.0f) * 0.01f, -0.95f, 0.95f);
    if (snapSmoothers)
    {
        delayMs.setCurrentAndTargetValue(delayTarget);
        depth.setCurrentAndTargetValue(depthTarget);
        feedback.setCurrentAndTargetValue(feedbackTarget);
        snapSmoothers = false;
    }
    else
    {
        delayMs.setTargetValue(delayTarget);
        depth.setTargetValue(depthTarget);
        feedback.setTargetValue(feedbackTarget);
    }
}

void FlangerEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    if (lineSize <= 0)
        return;
    std::array<float*, kMaxChannels> data{};
    for (int c = 0; c < numChannels; ++c)
        data[static_cast<size_t>(c)] = buffer.getWritePointer(c);

    if (sync)
        lfo.setSynced(division, static_cast<double>(context.tempoBpm));
    else
        lfo.setRateHz(static_cast<double>(rateHz));
    const double keyOctaves =
        static_cast<double>(keyTrack) * static_cast<double>(finiteOr(context.noteSemitones, 0.0f)) / 12.0;
    const double maxDelaySamples = kMaxDelaySeconds * sampleRate;

    const auto wrap = [this](int index) { return ((index % lineSize) + lineSize) % lineSize; };

    // Cut at the note-ons: the sweep starts again at each, so every kick is the same.
    pending.run(numSamples,
                [&](int begin, int end, bool restart)
                {
                    if (restart)
                        lfo.restart();
                    for (int i = begin; i < end; ++i)
                    {
                        const double fb = static_cast<double>(feedback.getNextValue());
                        const double sweepDepth = static_cast<double>(depth.getNextValue());
                        const double baseMs = static_cast<double>(delayMs.getNextValue());
                        const double phase = static_cast<double>(lfo.next());
                        const double sweep = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * phase));

                        // The delay shortens as the sweep goes on (the tone rises) and with the note.
                        const double samples =
                            std::clamp(baseMs * 0.001 * sampleRate *
                                           std::exp2(-kSweepOctaves * sweepDepth * sweep - keyOctaves),
                                       kMinDelaySamples, maxDelaySamples);
                        const int whole = static_cast<int>(samples);
                        const float fraction = static_cast<float>(samples - whole);
                        const double level = levelFor(fb);

                        // The sample `samples` ago lies between `base` and `base + 1`.
                        const int base = writeIndex - whole - 1;
                        const float t = 1.0f - fraction;

                        for (int c = 0; c < numChannels; ++c)
                        {
                            float& sample = data[static_cast<size_t>(c)][i];
                            auto& line = lines[static_cast<size_t>(c)];
                            const float x = std::isfinite(sample) ? sample : 0.0f;

                            const float delayed = hermite(line[static_cast<size_t>(wrap(base - 1))],
                                                          line[static_cast<size_t>(wrap(base))],
                                                          line[static_cast<size_t>(wrap(base + 1))],
                                                          line[static_cast<size_t>(wrap(base + 2))], t);
                            float written = x + static_cast<float>(fb) * delayed;
                            if (!std::isfinite(written))
                            {
                                // Never let a bad value live in the feedback loop.
                                std::fill(line.begin(), line.end(), 0.0f);
                                written = x;
                            }
                            line[static_cast<size_t>(writeIndex)] = written;

                            const float y = 0.5f * (x + delayed) * static_cast<float>(level);
                            sample = std::isfinite(y) ? y : 0.0f;
                        }
                        writeIndex = (writeIndex + 1) % lineSize;
                    }
                });
}

} // namespace fcm::dsp

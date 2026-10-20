// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/phaser/PhaserEffect.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.010;
constexpr double kStagesFadeSeconds = 0.020;
constexpr double kMaxFrequencyOfRate = 0.45;
constexpr double kSweepOctaves = 4.0; // at Depth 100 %

// Bit 0: Sync off, bit 1: on (ParameterSpec::shownWhen).
constexpr unsigned kFreeRunning = 1u << 0;
constexpr unsigned kSynced = 1u << 1;

enum Param
{
    pStages,
    pFrequency,
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

const EffectDescriptor& PhaserEffect::descriptor() const
{
    static const EffectDescriptor d{"phaser", "Phaser", 1, 1.0f, false};
    return d;
}

std::span<const ParameterSpec> PhaserEffect::parameters() const
{
    static constexpr std::string_view kStageNames[] = {"4", "8", "12"};
    static const ParameterSpec specs[] = {
        {"stages", "Stages", ParameterKind::choice, 0.0f, 2.0f, 1.0f, 1.0f, 0.0f, "", kStageNames},
        {"freq", "Frequency", ParameterKind::number, 20.0f, 10000.0f, 1.0f, 300.0f, 500.0f, "Hz", {}},
        {"sync", "Sync", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "", {}},
        {"rate",
         "Rate",
         ParameterKind::number,
         0.0f,
         20.0f,
         0.01f,
         0.5f,
         2.0f,
         "Hz",
         {},
         "sync",
         kFreeRunning},
        {"division", "Rate", ParameterKind::choice, 0.0f, 13.0f, 1.0f, 9.0f, 0.0f, "", kLfoDivisionNames,
         "sync", kSynced},
        {"depth", "Depth", ParameterKind::number, 0.0f, 100.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"feedback", "Feedback", ParameterKind::number, 0.0f, 95.0f, 0.1f, 40.0f, 0.0f, "%", {}},
        {"keytrack", "Key Track", ParameterKind::number, 0.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
    };
    return specs;
}

int PhaserEffect::stagesOf(int choice) noexcept
{
    return choice <= 0 ? 4 : (choice == 1 ? 8 : 12);
}

double PhaserEffect::cornerForFirstNotch(double notchHz, int stageCount) noexcept
{
    return notchHz / std::tan(std::numbers::pi / (2.0 * std::max(1, stageCount)));
}

double PhaserEffect::levelFor(double fb) noexcept
{
    fb = std::clamp(std::isfinite(fb) ? fb : 0.0, 0.0, 0.95);
    const double peak = (1.0 + 1.0 / (1.0 - fb)) * 0.5;
    return std::min(1.0, 2.0 / peak);
}

void PhaserEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);
    lfo.prepare(sampleRate);
    frequency.reset(sampleRate, kSmoothSeconds);
    depth.reset(sampleRate, kSmoothSeconds);
    feedback.reset(sampleRate, kSmoothSeconds);
    fadeTotal = std::max(1, static_cast<int>(kStagesFadeSeconds * sampleRate));
    reset();
}

void PhaserEffect::reset() noexcept
{
    for (auto& chain : chains)
        chain.clear();
    lfo.reset();
    pending.clear();
    fadeLeft = 0;
    untilUpdate = 0;
    snapSmoothers = true;
}

void PhaserEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < kNumParams)
        return;

    const int wantedStages = stagesOf(static_cast<int>(std::lround(finiteOr(values[pStages], 1.0f))));
    if (!snapSmoothers && wantedStages != chainStages[static_cast<size_t>(current)])
    {
        // The chain that played fades out while a fresh one fades in.
        current = 1 - current;
        chains[static_cast<size_t>(current)].clear();
        chainStages[static_cast<size_t>(current)] = wantedStages;
        fadeLeft = fadeTotal;
    }
    else if (snapSmoothers)
    {
        chainStages[static_cast<size_t>(current)] = wantedStages;
    }
    sync = finiteOr(values[pSync], 0.0f) >= 0.5f;
    division = std::clamp(static_cast<int>(std::lround(finiteOr(values[pDivision], 9.0f))), 0,
                          static_cast<int>(kLfoDivisions.size()) - 1);
    rateHz = std::clamp(finiteOr(values[pRate], 0.5f), 0.0f, 20.0f);
    keyTrack = std::clamp(finiteOr(values[pKeyTrack], 0.0f) * 0.01f, 0.0f, 1.0f);

    const float frequencyTarget = std::clamp(finiteOr(values[pFrequency], 300.0f), 20.0f, 10000.0f);
    const float depthTarget = std::clamp(finiteOr(values[pDepth], 50.0f) * 0.01f, 0.0f, 1.0f);
    const float feedbackTarget = std::clamp(finiteOr(values[pFeedback], 40.0f) * 0.01f, 0.0f, 0.95f);
    if (snapSmoothers)
    {
        frequency.setCurrentAndTargetValue(frequencyTarget);
        depth.setCurrentAndTargetValue(depthTarget);
        feedback.setCurrentAndTargetValue(feedbackTarget);
        snapSmoothers = false;
    }
    else
    {
        frequency.setTargetValue(frequencyTarget);
        depth.setTargetValue(depthTarget);
        feedback.setTargetValue(feedbackTarget);
    }
}

void PhaserEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    std::array<float*, kMaxChannels> data{};
    for (int c = 0; c < numChannels; ++c)
        data[static_cast<size_t>(c)] = buffer.getWritePointer(c);

    if (sync)
        lfo.setSynced(division, static_cast<double>(context.tempoBpm));
    else
        lfo.setRateHz(static_cast<double>(rateHz));
    const double keyOctaves =
        static_cast<double>(keyTrack) * static_cast<double>(finiteOr(context.noteSemitones, 0.0f)) / 12.0;

    // Cut at the note-ons: the sweep starts again at each, so every kick is the same.
    pending.run(numSamples,
                [&](int begin, int end, bool restart)
                {
                    if (restart)
                    {
                        lfo.restart();
                        untilUpdate = 0;
                    }
                    for (int i = begin; i < end; ++i)
                    {
                        const double fb = static_cast<double>(feedback.getNextValue());
                        const double sweepDepth = static_cast<double>(depth.getNextValue());
                        const double notchHz = static_cast<double>(frequency.getNextValue());
                        const double phase = static_cast<double>(lfo.next());

                        if (untilUpdate <= 0 || frequency.isSmoothing() || depth.isSmoothing())
                        {
                            const double sweep = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * phase));
                            const double hz =
                                notchHz * std::exp2(kSweepOctaves * sweepDepth * sweep + keyOctaves);
                            for (size_t k = 0; k < chains.size(); ++k)
                            {
                                const double corner = std::min(cornerForFirstNotch(hz, chainStages[k]),
                                                               kMaxFrequencyOfRate * sampleRate);
                                const double g =
                                    std::tan(std::numbers::pi * std::max(corner, 1.0) / sampleRate);
                                allPassG[k] = g / (1.0 + g);
                            }
                            untilUpdate = kControlInterval;
                        }
                        --untilUpdate;

                        const double level = levelFor(fb);
                        const double fade = fadeLeft > 0 ? static_cast<double>(fadeLeft) / fadeTotal : 0.0;
                        const int old = 1 - current;

                        for (int c = 0; c < numChannels; ++c)
                        {
                            float& sample = data[static_cast<size_t>(c)][i];
                            const double x = std::isfinite(sample) ? static_cast<double>(sample) : 0.0;

                            const auto run = [&](Chain& chain, int count, double coefficient)
                            {
                                auto& state = chain.state[static_cast<size_t>(c)];
                                double u = x + fb * chain.last[static_cast<size_t>(c)];
                                for (int s = 0; s < count; ++s)
                                {
                                    const double v = (u - state[static_cast<size_t>(s)]) * coefficient;
                                    const double low = v + state[static_cast<size_t>(s)];
                                    state[static_cast<size_t>(s)] = low + v;
                                    u = 2.0 * low - u;
                                }
                                chain.last[static_cast<size_t>(c)] = u;
                                return 0.5 * (x + u) * level;
                            };

                            const auto now = static_cast<size_t>(current);
                            const auto before = static_cast<size_t>(old);
                            double y = run(chains[now], chainStages[now], allPassG[now]);
                            if (fadeLeft > 0)
                                y = y * (1.0 - fade) +
                                    run(chains[before], chainStages[before], allPassG[before]) * fade;
                            sample = std::isfinite(y) ? static_cast<float>(y) : 0.0f;
                        }
                        if (fadeLeft > 0)
                            --fadeLeft;
                    }
                });
}

} // namespace fcm::dsp

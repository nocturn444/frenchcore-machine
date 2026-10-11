// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace fcm::test
{

// Small effects for testing the chain and the layout code (never part of the plugin).

// Multiplies by a gain. Parameter: gain (number 0..4, default 1).
class GainEffect : public fcm::dsp::Effect
{
  public:
    const fcm::dsp::EffectDescriptor& descriptor() const override
    {
        static const fcm::dsp::EffectDescriptor d{"testgain", "Test Gain", 1};
        return d;
    }
    std::span<const fcm::dsp::ParameterSpec> parameters() const override
    {
        static const fcm::dsp::ParameterSpec specs[] = {
            {"gain", "Gain", fcm::dsp::ParameterKind::number, 0.0f, 4.0f, 0.01f, 1.0f, 1.0f, "x", {}}};
        return specs;
    }
    void prepare(const fcm::dsp::EffectSpec&) override {}
    void reset() noexcept override { ++resets; }
    void setParameters(std::span<const float> values) noexcept override
    {
        lastCount = std::min<size_t>(values.size(), lastValues.size());
        std::copy_n(values.begin(), lastCount, lastValues.begin());
        gain = values.empty() ? 1.0f : values[0];
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept override { buffer.applyGain(gain); }

    float gain = 1.0f;

    // The chain owns the instance, so tests watch these through static state.
    static inline int resets = 0;
    static inline std::array<float, 8> lastValues{};
    static inline size_t lastCount = 0;
};

// Hard-clips at a threshold. Parameters: threshold (number 0.05..1, default 1), mode (choice, 2
// entries), flag (toggle) and steps (integer), to exercise every parameter kind.
class ClipEffect : public fcm::dsp::Effect
{
  public:
    const fcm::dsp::EffectDescriptor& descriptor() const override
    {
        static const fcm::dsp::EffectDescriptor d{"testclip", "Test Clip", 1};
        return d;
    }
    std::span<const fcm::dsp::ParameterSpec> parameters() const override
    {
        static const std::string_view modes[] = {"Hard", "Soft"};
        static const fcm::dsp::ParameterSpec specs[] = {
            {"threshold",
             "Threshold",
             fcm::dsp::ParameterKind::number,
             0.05f,
             1.0f,
             0.0f,
             1.0f,
             0.3f,
             "",
             {}},
            {"mode", "Mode", fcm::dsp::ParameterKind::choice, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", modes},
            {"flag", "Flag", fcm::dsp::ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, "", {}},
            {"steps", "Steps", fcm::dsp::ParameterKind::integer, -3.0f, 3.0f, 1.0f, 2.0f, 0.0f, "st", {}}};
        return specs;
    }
    void prepare(const fcm::dsp::EffectSpec&) override {}
    void reset() noexcept override {}
    void setParameters(std::span<const float> values) noexcept override
    {
        threshold = values.empty() ? 1.0f : values[0];
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept override
    {
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            float* d = buffer.getWritePointer(c);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = std::clamp(d[i], -threshold, threshold);
        }
    }

    float threshold = 1.0f;
};

// A pure delay with a latency of 32 samples per oversampling step (32, 64, 128, 256). No parameters.
class DelayEffect : public fcm::dsp::Effect
{
  public:
    const fcm::dsp::EffectDescriptor& descriptor() const override
    {
        static const fcm::dsp::EffectDescriptor d{"testdelay", "Test Delay", 1};
        return d;
    }
    std::span<const fcm::dsp::ParameterSpec> parameters() const override { return {}; }
    void prepare(const fcm::dsp::EffectSpec& spec) override
    {
        ring.assign(static_cast<size_t>(spec.numChannels) * kRing, 0.0f);
        write = 0;
    }
    void reset() noexcept override
    {
        std::fill(ring.begin(), ring.end(), 0.0f);
        write = 0;
    }
    void setParameters(std::span<const float>) noexcept override {}
    int latencySamples(int factor) const noexcept override { return 32 * factor; }

    void setOversampling(int newFactor) noexcept override { factor = newFactor; }

    void process(juce::AudioBuffer<float>& buffer) noexcept override
    {
        const int delay = latencySamples(factor);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                float* line = ring.data() + static_cast<size_t>(c) * kRing;
                float* d = buffer.getWritePointer(c);
                line[write] = d[i];
                d[i] = line[(write + kRing - delay) % kRing];
            }
            write = (write + 1) % kRing;
        }
    }

    static constexpr int kRing = 1024;
    std::vector<float> ring;
    int write = 0;
    int factor = 2;
};

// An effect that only exists to fill a registry: N different IDs, no parameters.
template <int N> class NumberedEffect : public fcm::dsp::Effect
{
  public:
    const fcm::dsp::EffectDescriptor& descriptor() const override
    {
        static const char* const ids[] = {"n00", "n01", "n02", "n03", "n04", "n05", "n06", "n07", "n08",
                                          "n09", "n10", "n11", "n12", "n13", "n14", "n15", "n16", "n17"};
        static const fcm::dsp::EffectDescriptor d{ids[N], "Numbered", 1};
        return d;
    }
    std::span<const fcm::dsp::ParameterSpec> parameters() const override { return {}; }
    void prepare(const fcm::dsp::EffectSpec&) override {}
    void reset() noexcept override {}
    void setParameters(std::span<const float>) noexcept override {}
    void process(juce::AudioBuffer<float>&) noexcept override {}
};

} // namespace fcm::test

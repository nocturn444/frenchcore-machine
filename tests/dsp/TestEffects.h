// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/NoteTriggers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

// Passes the sound through and records what the 0.3 note triggers do (docs/decisions/0012 item 1): the
// position of every noteOn it is told, and the position where the sound first shows in its input,
// both counted in samples of its own input since the engine started (the effect is on from the
// first block, so that is the engine's clock). Fixed arrays, so it can be used in allocation tests.
// Only one probe instance is on at a time in a test, so the log is static.
class ProbeEffect : public fcm::dsp::Effect
{
  public:
    const fcm::dsp::EffectDescriptor& descriptor() const override
    {
        static const fcm::dsp::EffectDescriptor d{"testprobe", "Test Probe", 1};
        return d;
    }
    std::span<const fcm::dsp::ParameterSpec> parameters() const override { return {}; }
    void prepare(const fcm::dsp::EffectSpec&) override {}
    void reset() noexcept override
    {
        ++resets;
        pending.clear();
    }
    void setParameters(std::span<const float>) noexcept override {}
    void noteOn(int sampleOffset) noexcept override
    {
        pending.add(sampleOffset);
        ++noteOnCalls;
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept override
    {
        const int n = buffer.getNumSamples();
        pending.run(n,
                    [&](int begin, int, bool restart)
                    {
                        if (restart && noteOnCount < static_cast<int>(noteOnAt.size()))
                            noteOnAt[static_cast<size_t>(noteOnCount++)] = processed + begin;
                    });
        if (firstSoundAt < 0 && buffer.getNumChannels() > 0)
        {
            const float* d = buffer.getReadPointer(0);
            for (int i = 0; i < n; ++i)
                if (std::abs(d[i]) > 1.0e-4f)
                {
                    firstSoundAt = processed + i;
                    break;
                }
        }
        processed += n;
    }

    static void clearLog() noexcept
    {
        noteOnAt.fill(0);
        noteOnCount = 0;
        noteOnCalls = 0;
        firstSoundAt = -1;
        processed = 0;
        resets = 0;
    }

    static inline std::array<std::int64_t, 64> noteOnAt{};
    static inline int noteOnCount = 0;
    static inline int noteOnCalls = 0;
    static inline std::int64_t firstSoundAt = -1;
    static inline std::int64_t processed = 0;
    static inline int resets = 0;

  private:
    fcm::dsp::PendingNoteOns pending;
};

// An effect that has a drawn curve and does nothing else (docs/decisions/0012 item 3): its default is
// a straight line from 0 to 1, it offers two ready-made shapes, and it records the curve it was given
// by setCurve() (nullptr means its default).
class CurveEffect : public fcm::dsp::Effect
{
  public:
    const fcm::dsp::EffectDescriptor& descriptor() const override
    {
        static const fcm::dsp::EffectDescriptor d{"testcurve", "Test Curve", 1};
        return d;
    }
    std::span<const fcm::dsp::ParameterSpec> parameters() const override { return {}; }
    void prepare(const fcm::dsp::EffectSpec&) override {}
    void reset() noexcept override {}
    void setParameters(std::span<const float>) noexcept override {}
    void process(juce::AudioBuffer<float>&) noexcept override {}

    static const fcm::dsp::EffectCurve& rampCurve()
    {
        static const fcm::dsp::CurvePoint points[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
        static const fcm::dsp::EffectCurve curve = fcm::dsp::EffectCurve::from(points);
        return curve;
    }
    const fcm::dsp::EffectCurve* defaultCurve() const noexcept override { return &rampCurve(); }
    std::span<const fcm::dsp::NamedCurve> curvePresets() const noexcept override
    {
        static const fcm::dsp::CurvePoint down[] = {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        static const fcm::dsp::CurvePoint gate[] = {
            {0.0f, 1.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {0.5f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        static const fcm::dsp::NamedCurve presets[] = {{"Down", fcm::dsp::EffectCurve::from(down)},
                                                       {"Gate", fcm::dsp::EffectCurve::from(gate)}};
        return presets;
    }
    void setCurve(const fcm::dsp::EffectCurve* curve) noexcept override
    {
        current = curve != nullptr ? curve : &rampCurve();
        ++curveCalls;
    }

    const fcm::dsp::EffectCurve* current = nullptr;
    int curveCalls = 0;
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

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

// Three-band stereo width with Linkwitz-Riley crossovers (docs/specs/fx-stereo.md).
class StereoEffect : public Effect
{
  public:
    // Normalised biquad: y = b0 x + s1; s1 = b1 x - a1 y + s2; s2 = b2 x - a2 y.
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // Second-order Butterworth low-pass / high-pass (Q = 1/sqrt 2), exposed for tests.
    static Biquad lowPass(double sampleRate, double frequencyHz) noexcept;
    static Biquad highPass(double sampleRate, double frequencyHz) noexcept;

  private:
    static constexpr int kDecorrelatorStages = 6;

    struct State
    {
        double s1 = 0.0, s2 = 0.0;

        double run(const Biquad& k, double x) noexcept
        {
            const double y = k.b0 * x + s1;
            s1 = k.b1 * x - k.a1 * y + s2;
            s2 = k.b2 * x - k.a2 * y;
            return y;
        }
    };

    // A fourth-order Linkwitz-Riley filter: two second-order sections in series.
    struct LR4
    {
        State first, second;

        double run(const Biquad& k, double x) noexcept { return second.run(k, first.run(k, x)); }
    };

    // Everything one channel needs to split into three bands.
    struct ChannelSplit
    {
        LR4 low1, high1;         // Crossover 1: low-pass and high-pass of the input
        LR4 low2, high2;         // Crossover 2 on the rest: mid and high
        LR4 alignLow, alignHigh; // Crossover 2 all-pass (low plus high) on the low band
    };

    struct Decorrelator
    {
        std::array<double, kDecorrelatorStages> x1{}, y1{};

        double run(double x) noexcept;
    };

    double sampleRate = 44100.0;
    int channels = 2;

    Biquad lowPass1, highPass1, lowPass2, highPass2;
    std::array<double, 3> widths{1.0, 1.0, 1.0}; // low, mid, high; the low one already 0 when mono-below
    std::array<ChannelSplit, 2> splits;
    std::array<Decorrelator, 3> decorrelators;
};

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

// A state variable filter with Low Pass, High Pass, Band Pass and Notch, 12 or 24 dB per octave,
// Resonance and a soft-saturating Drive (docs/specs/fx-filter.md). The filter is the
// topology-preserving-transform two-pole SVF from Vadim Zavalishin, "The Art of Virtual Analog Filter
// Design" (Native Instruments, 2012), which stays stable and smooth under fast cutoff changes.
class FilterEffect : public Effect
{
  public:
    // The order of the Type choice (saved as the entry number).
    enum class Type
    {
        lowPass,
        highPass,
        bandPass,
        notch
    };

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The damping `k` (1 / Q) of the 12 dB filter, and of the two stages of the 24 dB one, for a
    // resonance of 0 to 1. At 0 the response is maximally flat (-3 dB at the cutoff for both slopes);
    // at 1 the damping stays above zero, so the filter never self-oscillates.
    static double damping12(double resonance) noexcept;
    static double damping24First() noexcept;
    static double damping24Second(double resonance) noexcept;

    // The soft saturation of Drive: tanh(a x) / tanh(a), with a = 15 d^2 for a drive d of 0 to 1.
    // Exactly x at a drive of 0, and a full-scale input still gives a full-scale output.
    static double saturate(double x, double drive) noexcept;

  private:
    static constexpr int kMaxChannels = 2;

    struct Stage
    {
        double ic1 = 0.0, ic2 = 0.0;
    };

    // One pass through one two-pole stage; returns the output chosen by `type`.
    static double tick(Stage& stage, double input, double g, double k, double a1, double a2, double a3,
                       Type type) noexcept;

    double sampleRate = 44100.0;
    int channels = 2;
    Type type = Type::lowPass;
    bool steep = false; // 24 dB
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff{20000.0f};
    juce::SmoothedValue<float> resonance{0.0f};
    juce::SmoothedValue<float> drive{0.0f};
    bool snapSmoothers = true;
    std::array<std::array<Stage, 2>, kMaxChannels> stages{};
};

} // namespace fcm::dsp

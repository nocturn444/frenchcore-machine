// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

namespace fcm::dsp
{

// Attack and sustain control by envelope difference (docs/specs/fx-transient.md), the principle of the
// SPL Transient Designer: a fast and a slow peak follower on the linked stereo level; where the fast
// one is above the slow one a hit is starting (attack), where it is below, the sound is ringing out
// (sustain). The shaping depends on that difference in dB, not on the level, so a quiet kick is
// shaped like a loud one.
class TransientEffect : public Effect
{
  public:
    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The largest gain change either way, in dB.
    static constexpr double kMaxGainDb = 24.0;

  private:
    static constexpr int kMaxChannels = 2;

    // One-pole peak follower with separate attack and release coefficients.
    struct Follower
    {
        double value = 0.0;
        double attack = 1.0, release = 1.0; // 1 - exp(-1 / (time * rate))

        double run(double x) noexcept
        {
            value += (x > value ? attack : release) * (x - value);
            return value;
        }
    };

    double sampleRate = 44100.0;
    int channels = 2;
    Follower fast;
    Follower slow;
    double gainSmoothing = 1.0;                     // the 1 ms one-pole coefficient
    double gainDb = 0.0;                            // the smoothed gain applied
    juce::SmoothedValue<float> attackAmount{0.0f};  // -1..1
    juce::SmoothedValue<float> sustainAmount{0.0f}; // -1..1
    juce::SmoothedValue<float> outputGain{1.0f};    // linear
    bool snapSmoothers = true;
};

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace fcm::dsp
{

// Five kinds of distortion with drive, gains and tone, oversampled by the global Quality setting
// (docs/specs/fx-distortion.md).
class DistortionEffect : public Effect
{
  public:
    enum class Type
    {
        softClip,
        hardClip,
        overdrive,
        foldback,
        bitcrush
    };

    static constexpr int kNumTypes = 5;

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void setOversampling(int factor) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;
    int latencySamples(int oversamplingFactor) const noexcept override;

    // The shapers, exposed for tests. `x` is the sample after the gain.
    static float shape(Type type, float x) noexcept;

  private:
    static constexpr int kMaxChannels = 2;

    juce::dsp::Oversampling<float>* activeOversampler() noexcept;

    double sampleRate = 44100.0;
    int channels = 2;
    int factor = 1;
    Type type = Type::softClip;

    // Smoothed in dB at the base rate (docs/specs/fx-distortion.md §4).
    juce::SmoothedValue<float> preGainDb{0.0f};
    juce::SmoothedValue<float> postGainDb{0.0f};
    bool snapSmoothers = true;
    float toneCoefficient = 1.0f; // 1: no filtering
    bool toneActive = false;
    int bits = 8;
    float rate = 4.0f;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers; // 2x, 4x, 8x

    // Per channel state.
    std::array<float, kMaxChannels> dcInput{};
    std::array<float, kMaxChannels> dcOutput{};
    std::array<float, kMaxChannels> toneState{};
    std::array<float, kMaxChannels> crushHeld{};
    std::array<float, kMaxChannels> crushPhase{};
    float dcCoefficient = 0.999f;
};

} // namespace fcm::dsp

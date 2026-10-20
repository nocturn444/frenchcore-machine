// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace fcm::dsp
{

// A quick kick distortion in the idea of Nimble Kick's Kick FX section: a zone of frequencies is
// boosted by a gentle (6 dB per octave) band-pass and the result goes into a soft-to-hard clipper,
// oversampled by the global Quality setting (docs/specs/fx-peak.md).
class PeakEffect : public Effect
{
  public:
    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void setOversampling(int factor) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;
    int latencySamples(int oversamplingFactor) const noexcept override;

    // The clipper: `(1 - hardness) * tanh(u) + hardness * clamp(u, -1, 1)`, hardness 0 to 1.
    static float clip(float u, float hardness) noexcept;

  private:
    static constexpr int kMaxChannels = 2;

    juce::dsp::Oversampling<float>* activeOversampler() noexcept;
    void processPiece(juce::AudioBuffer<float>& buffer) noexcept;
    // Recomputes the filter coefficient and the two gains from the smoothed values.
    void updateCoefficients(float freq, float amount) noexcept;

    double sampleRate = 44100.0;
    int channels = 2;
    int factor = 1;

    juce::SmoothedValue<float> freqHz{150.0f};
    juce::SmoothedValue<float> amountUnit{0.5f};   // 0..1
    juce::SmoothedValue<float> hardnessUnit{0.3f}; // 0..1
    bool snapSmoothers = true;

    // The band-pass: a one-pole high-pass then a one-pole low-pass, both at the same frequency
    // (TPT one-poles, Zavalishin, "The Art of VA Filter Design", ch. 3).
    float filterG = 0.0f;   // g / (1 + g) of the TPT one-pole
    float boostGain = 0.0f; // the band-pass is added with this gain
    float driveGain = 1.0f; // linear gain before the clipper
    std::array<float, kMaxChannels> highPassState{};
    std::array<float, kMaxChannels> lowPassState{};

    std::vector<float> hardnessPerSample; // one per base-rate sample of a piece (set in prepare)
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers; // 2x, 4x, 8x
};

} // namespace fcm::dsp

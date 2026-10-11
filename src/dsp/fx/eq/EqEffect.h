// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

// Three-band EQ: low shelf, peak, high shelf (docs/specs/fx-eq.md). Biquads from the Audio EQ
// Cookbook by Robert Bristow-Johnson.
class EqEffect : public Effect
{
  public:
    enum class Shape
    {
        lowShelf,
        peak,
        highShelf
    };

    // Normalised biquad coefficients (a0 = 1): y = b0 x + s1; s1 = b1 x - a1 y + s2; s2 = b2 x - a2 y.
    struct Coefficients
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The cookbook design, exposed for tests. The frequency and Q are brought into a stable range.
    static Coefficients design(Shape shape, double sampleRate, double frequencyHz, double q,
                               double gainDb) noexcept;

  private:
    static constexpr int kBands = 3;
    static constexpr int kMaxChannels = 2;

    struct Band
    {
        Coefficients coefficients;
        bool active = false;
        std::array<std::array<double, 2>, kMaxChannels> state{}; // s1, s2 per channel
    };

    double sampleRate = 44100.0;
    int channels = 2;
    std::array<Band, kBands> bands;
};

} // namespace fcm::dsp

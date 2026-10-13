// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

// Four-band parametric EQ (docs/specs/fx-eq.md): every band has its own type, frequency, gain and Q.
// Biquads from the Audio EQ Cookbook by Robert Bristow-Johnson.
class EqEffect : public Effect
{
  public:
    // The order is the order of the Type choice (saved as the entry number).
    enum class Shape
    {
        bell,
        lowShelf,
        highShelf,
        lowCut,
        highCut
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

    // The cookbook design, exposed for tests and for the response display. The frequency and Q are
    // brought into a stable range. The gain is ignored by the two cuts.
    static Coefficients design(Shape shape, double sampleRate, double frequencyHz, double q,
                               double gainDb) noexcept;

    // The magnitude of a design at `hz`, in dB (the response curve of the display).
    static double magnitudeDb(const Coefficients& coefficients, double sampleRate, double hz) noexcept;

    // Whether a band changes the sound: a bell or shelf needs some gain, a cut always acts. A band that
    // is off never does.
    static bool isActive(bool on, Shape shape, double gainDb) noexcept;

    // Number of bands, and how many parameter values one band takes, in the order of parameters():
    // on, type, frequency, gain, Q.
    static constexpr int kBands = 4;
    static constexpr int kValuesPerBand = 5;

  private:
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

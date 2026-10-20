// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

// A vowel filter (docs/specs/fx-formant.md): three band-pass filters in parallel, one per formant,
// set from a table of the average formants of an adult male voice (Peterson and Barney, "Control
// methods used in a study of the vowels", JASA 1952). The band-passes are the topology-preserving
// transform state variable filter of Vadim Zavalishin, "The Art of Virtual Analog Filter Design"
// (Native Instruments, 2012), as in the Filter.
class FormantEffect : public Effect
{
  public:
    static constexpr int kVowelCount = 5;
    static constexpr int kFormants = 3;

    // One vowel of the table.
    struct Vowel
    {
        std::array<double, kFormants> hz;
        std::array<double, kFormants> gainDb; // relative to the first formant
    };

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    static const Vowel& vowel(int index) noexcept;

    // The make-up gain, in dB, that brings the pink-noise level of a vowel (Resonance 50 %) to the
    // same value for all five. Computed once from the table.
    static double makeupDb(int index) noexcept;

    // The soft saturation of Drive, the Filter's: tanh(a x) / tanh(a), a = 15 d^2 for d of 0 to 1.
    static double saturate(double x, double drive) noexcept;

  private:
    static constexpr int kMaxChannels = 2;

    struct Band
    {
        double ic1 = 0.0, ic2 = 0.0;
    };

    double sampleRate = 44100.0;
    int channels = 2;
    // The targets move over 20 ms, so a change of Vowel glides instead of clicking.
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, kFormants> frequency;
    std::array<juce::SmoothedValue<float>, kFormants> gain;
    juce::SmoothedValue<float> resonance{0.5f};
    juce::SmoothedValue<float> drive{0.0f};
    bool snapSmoothers = true;
    std::array<std::array<Band, kFormants>, kMaxChannels> bands{};
};

} // namespace fcm::dsp

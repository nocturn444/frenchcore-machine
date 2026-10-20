// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/NoteTriggers.h"
#include "dsp/fx/TempoLfo.h"

#include <array>
#include <vector>

namespace fcm::dsp
{

// A flanger (docs/specs/fx-flanger.md): the sound mixed with a copy of itself delayed by a few
// milliseconds, with feedback, which makes a comb of notches (Julius O. Smith, "Physical Audio Signal
// Processing", the flanging chapter). The delay is read with 4-point Hermite interpolation so a slow
// sweep does not zipper. The sweep is a TempoLfo that restarts at every note.
class FlangerEffect : public Effect
{
  public:
    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void setContext(const EffectContext& newContext) noexcept override { context = newContext; }
    void noteOn(int sampleOffset) noexcept override { pending.add(sampleOffset); }
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The gain that keeps the peaks of the feedback comb within +6 dB: they reach
    // (1 + 1 / (1 - |feedback|)) / 2 of the dry level, and above 2 the output is scaled back.
    static double levelFor(double feedback) noexcept;

    // Four-point, third-order Hermite interpolation between y0 and y1 at t (0 to 1); ym1 and y2 are
    // the neighbours.
    static float hermite(float ym1, float y0, float y1, float y2, float t) noexcept;

  private:
    static constexpr int kMaxChannels = 2;
    static constexpr double kMaxDelaySeconds = 0.040;
    static constexpr double kLineSeconds = 0.050;
    static constexpr double kMinDelaySamples = 3.0; // the interpolator reads two samples ahead

    double sampleRate = 44100.0;
    int channels = 2;

    bool sync = false;
    int division = 9;
    float rateHz = 0.3f;
    float keyTrack = 0.0f;
    EffectContext context;
    TempoLfo lfo;
    PendingNoteOns pending;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> delayMs{2.0f};
    juce::SmoothedValue<float> depth{0.5f};
    juce::SmoothedValue<float> feedback{0.5f};
    bool snapSmoothers = true;

    std::array<std::vector<float>, kMaxChannels> lines;
    int lineSize = 0;
    int writeIndex = 0;
};

} // namespace fcm::dsp

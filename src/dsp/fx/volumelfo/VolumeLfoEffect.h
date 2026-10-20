// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/NoteTriggers.h"
#include "dsp/fx/TempoLfo.h"

namespace fcm::dsp
{

// A volume shaper in the style of LFOTool (docs/specs/fx-volumelfo.md): the level follows a drawn
// curve, one cycle long, repeated at the Rate or at a note value of the tempo, and started again at
// every note so every kick gets the same shape. Used after the distortion and the reverb it cuts the
// mass back into rhythm (pumping, gating). The curve is an effect curve (docs/decisions/0012).
class VolumeLfoEffect : public Effect
{
  public:
    VolumeLfoEffect();

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void setContext(const EffectContext& newContext) noexcept override { context = newContext; }
    void noteOn(int sampleOffset) noexcept override { pending.add(sampleOffset); }
    void setCurve(const EffectCurve* newCurve) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    const EffectCurve* defaultCurve() const noexcept override;
    std::span<const NamedCurve> curvePresets() const noexcept override;

    // The time constant, in seconds, of the slew that rounds the edges of the shape for a Smooth of
    // 0 to 1: 0.2 ms to 20 ms, the first half of the knob covering 0.2 to 5 ms.
    static double smoothSeconds(double smooth) noexcept;

  private:
    double sampleRate = 44100.0;

    bool sync = true;
    int division = 9;
    float rateHz = 2.0f;
    float smooth = 0.1f;
    EffectContext context;
    TempoLfo lfo;
    PendingNoteOns pending;

    juce::SmoothedValue<float> depth{1.0f};
    bool snapSmoothers = true;

    EffectCurve curve; // a copy: the published one may be released after the next block
    int segment = 0;
    double gain = 1.0;
};

} // namespace fcm::dsp

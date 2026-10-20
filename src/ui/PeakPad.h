// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::ui
{

// The Peak's XY pad (docs/specs/fx-peak.md section 2): a dot whose horizontal position is Freq
// (logarithmic, 20 Hz to 5 kHz) and whose vertical position is Hardness (soft at the bottom, hard at
// the top). Dragging writes both parameters through the parameter tree, as one gesture each, so the
// DAW records automation; double-click puts both back to their defaults; the knobs under it move the
// dot too.
class PeakPad : public juce::Component, public juce::SettableTooltipClient
{
  public:
    PeakPad(juce::AudioProcessorValueTreeState& state, const juce::String& instanceId);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // The mapping between the pad and the values, public for tests: position 0..1 from the left (Freq)
    // and from the bottom (Hardness).
    static float frequencyToPosition(float hz) noexcept;
    static float positionToFrequency(float position) noexcept;
    static float hardnessToPosition(float percent) noexcept;
    static float positionToHardness(float position) noexcept;

    static constexpr float kMinHz = 20.0f;
    static constexpr float kMaxHz = 5000.0f;

  private:
    juce::Rectangle<float> plot() const;
    void moveTo(juce::Point<float> position);

    std::unique_ptr<juce::ParameterAttachment> freqAttachment;
    std::unique_ptr<juce::ParameterAttachment> hardnessAttachment;
    juce::RangedAudioParameter* freqParameter = nullptr;
    juce::RangedAudioParameter* hardnessParameter = nullptr;
    float freq = 150.0f;
    float hardness = 30.0f;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeakPad)
};

} // namespace fcm::ui

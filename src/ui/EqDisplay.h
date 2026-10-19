// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/EqResponse.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::ui
{

// The EQ's response curve with one dot per band (docs/specs/fx-eq.md §2). Drag a dot sideways for the
// frequency and up or down for the gain; the mouse wheel over a dot changes its Q; a double-click
// switches the band on or off. It only talks to the parameters, through attachments.
class EqDisplay : public juce::Component
{
  public:
    // `instanceId` is the copy of the EQ it shows: "eq", "eq2", ...
    explicit EqDisplay(juce::AudioProcessorValueTreeState& state, juce::String instanceId = "eq");

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

  private:
    struct BandParameters
    {
        juce::RangedAudioParameter* on = nullptr;
        juce::RangedAudioParameter* freq = nullptr;
        juce::RangedAudioParameter* gain = nullptr;
        juce::RangedAudioParameter* q = nullptr;
        std::unique_ptr<juce::ParameterAttachment> freqAttachment;
        std::unique_ptr<juce::ParameterAttachment> gainAttachment;
        std::unique_ptr<juce::ParameterAttachment> qAttachment;
        std::unique_ptr<juce::ParameterAttachment> onAttachment;
        std::unique_ptr<juce::ParameterAttachment> typeAttachment;
    };

    juce::Rectangle<float> plot() const;
    juce::Point<float> dotPosition(int band) const;
    int bandAt(juce::Point<float> position) const;
    void setValue(juce::RangedAudioParameter* parameter, juce::ParameterAttachment& attachment, double value);

    juce::AudioProcessorValueTreeState& state;
    juce::String instance;
    std::array<BandParameters, core::kEqBands> bands;
    core::EqBands values;
    int dragged = -1;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqDisplay)
};

} // namespace fcm::ui

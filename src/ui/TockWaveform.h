// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/TockSelector.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::ui
{

// The waveform of the Tock sample with the two Trim handles (docs/specs/tock.md §8.3). Dragging a handle
// changes the Trim Start or Trim End parameter, so it is automatable and undoable in the host;
// double-clicking a handle puts it back. In Synth mode there is nothing to trim and it says so.
class TockWaveform : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
  public:
    TockWaveform(juce::AudioProcessorValueTreeState& state, const core::TockSelector& selector);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // The trim fractions (0 to 1) as the display currently shows them.
    double trimStart() const noexcept { return startFraction; }
    double trimEnd() const noexcept { return endFraction; }

  private:
    enum class Handle
    {
        none,
        start,
        end
    };

    void timerCallback() override;
    bool sampleModeOn() const;
    Handle handleAt(juce::Point<int> position) const;
    double fractionAt(int x) const;
    int xOf(double fraction) const;
    juce::Rectangle<float> plot() const;

    juce::AudioProcessorValueTreeState& parameters;
    const core::TockSelector& selector;
    std::atomic<float>* mode = nullptr;
    std::unique_ptr<juce::ParameterAttachment> startAttachment;
    std::unique_ptr<juce::ParameterAttachment> endAttachment;
    juce::RangedAudioParameter* startParameter = nullptr;
    juce::RangedAudioParameter* endParameter = nullptr;

    double startFraction = 0.0;
    double endFraction = 1.0;
    Handle dragging = Handle::none;
    int shownGeneration = -1;
    bool shownSampleMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TockWaveform)
};

} // namespace fcm::ui

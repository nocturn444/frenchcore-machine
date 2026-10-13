// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/WavetableSelector.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::ui
{

// Shows the wavetable of one oscillator: the name, a faint stack of its frames and, bright, the
// waveform at the current Position (docs/specs/wavetable-display.md). It only reads: the selector
// for the frames and an attachment for Position.
class WavetableDisplay : public juce::Component, private juce::ChangeListener
{
  public:
    WavetableDisplay(core::WavetableSelector& selector, juce::RangedAudioParameter& position);
    ~WavetableDisplay() override;

    void paint(juce::Graphics& g) override;

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    core::WavetableSelector& selector;
    float position = 0.0f;
    juce::ParameterAttachment positionAttachment;
};

} // namespace fcm::ui

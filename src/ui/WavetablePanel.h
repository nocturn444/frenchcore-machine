// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/WavetableSelector.h"
#include "ui/WavetableDisplay.h"

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::ui
{

// One display and one "Load Wavetable..." button per oscillator, a "Load Tock Sample..." button, and a line
// with the last message (docs/specs/wavetable-display.md, wavetable-user.md §2). It only asks for a file;
// reading it is the processor's job.
class WavetablePanel : public juce::Component, private juce::ChangeListener
{
  public:
    static constexpr int kHeight = 150;

    // `onLoad(oscillator, file)` is called on the message thread with the chosen WAV file.
    // `message` is read whenever `changes` announces something.
    WavetablePanel(juce::AudioProcessorValueTreeState& state, core::WavetableSelector& selector1,
                   core::WavetableSelector& selector2, std::function<void(int, const juce::File&)> onLoad,
                   std::function<void(const juce::File&)> onLoadTock, juce::ChangeBroadcaster& changes,
                   std::function<juce::String()> message);
    ~WavetablePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void choose(int oscillator);
    void chooseTock();

    std::function<void(int, const juce::File&)> loadRequested;
    std::function<void(const juce::File&)> tockLoadRequested;
    juce::ChangeBroadcaster& broadcaster;
    std::function<juce::String()> currentMessage;
    WavetableDisplay display1;
    WavetableDisplay display2;
    juce::TextButton osc1Button{"Load Osc 1 Wavetable..."};
    juce::TextButton osc2Button{"Load Osc 2 Wavetable..."};
    juce::TextButton tockButton{"Load Tock Sample..."};
    juce::Label messageLabel;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace fcm::ui

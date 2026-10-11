// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm::ui
{

// A strip with one "Load Wavetable..." button per oscillator and a line with the last message
// (docs/specs/wavetable-user.md §2). It only asks for a file; reading it is the processor's job.
class WavetablePanel : public juce::Component, private juce::ChangeListener
{
  public:
    static constexpr int kHeight = 44;

    // `onLoad(oscillator, file)` is called on the message thread with the chosen WAV file.
    // `message` is read whenever `changes` announces something.
    WavetablePanel(std::function<void(int, const juce::File&)> onLoad, juce::ChangeBroadcaster& changes,
                   std::function<juce::String()> message);
    ~WavetablePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void choose(int oscillator);

    std::function<void(int, const juce::File&)> loadRequested;
    juce::ChangeBroadcaster& broadcaster;
    std::function<juce::String()> currentMessage;
    juce::TextButton osc1Button{"Load Osc 1 Wavetable..."};
    juce::TextButton osc2Button{"Load Osc 2 Wavetable..."};
    juce::Label messageLabel;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace fcm::ui

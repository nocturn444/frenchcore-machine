// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WavetablePanel.h"

namespace fcm::ui
{

WavetablePanel::WavetablePanel(std::function<void(int, const juce::File&)> onLoad,
                               juce::ChangeBroadcaster& changes, std::function<juce::String()> message)
    : loadRequested(std::move(onLoad)), broadcaster(changes), currentMessage(std::move(message))
{
    osc1Button.onClick = [this] { choose(0); };
    osc2Button.onClick = [this] { choose(1); };
    addAndMakeVisible(osc1Button);
    addAndMakeVisible(osc2Button);

    messageLabel.setJustificationType(juce::Justification::centredLeft);
    messageLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(messageLabel);

    broadcaster.addChangeListener(this);
    changeListenerCallback(nullptr);
}

WavetablePanel::~WavetablePanel()
{
    broadcaster.removeChangeListener(this);
}

void WavetablePanel::choose(int oscillator)
{
    chooser = std::make_unique<juce::FileChooser>("Choose a wavetable (WAV)", juce::File{}, "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, oscillator](const juce::FileChooser& finished)
                         {
                             const auto file = finished.getResult();
                             if (file != juce::File{})
                                 loadRequested(oscillator, file);
                         });
}

void WavetablePanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    messageLabel.setText(currentMessage ? currentMessage() : juce::String(), juce::dontSendNotification);
}

void WavetablePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15161a));
}

void WavetablePanel::resized()
{
    auto area = getLocalBounds().reduced(12, 6);
    const int buttonWidth = std::min(240, area.getWidth() / 4);
    osc1Button.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(8);
    osc2Button.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(12);
    messageLabel.setBounds(area);
}

} // namespace fcm::ui

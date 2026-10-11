// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WavetablePanel.h"

#include "core/ParameterIds.h"

namespace fcm::ui
{

WavetablePanel::WavetablePanel(juce::AudioProcessorValueTreeState& state, core::WavetableSelector& selector1,
                               core::WavetableSelector& selector2,
                               std::function<void(int, const juce::File&)> onLoad,
                               juce::ChangeBroadcaster& changes, std::function<juce::String()> message)
    : loadRequested(std::move(onLoad)), broadcaster(changes), currentMessage(std::move(message)),
      display1(selector1, *state.getParameter(
                              core::ParameterIds::oscillator(0, core::ParameterIds::Oscillator::kPosition))),
      display2(selector2, *state.getParameter(
                              core::ParameterIds::oscillator(1, core::ParameterIds::Oscillator::kPosition)))
{
    addAndMakeVisible(display1);
    addAndMakeVisible(display2);
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
    messageLabel.setBounds(area.removeFromBottom(22));
    area.removeFromBottom(4);

    const int gap = 12;
    const int half = (area.getWidth() - gap) / 2;
    auto left = area.removeFromLeft(half);
    area.removeFromLeft(gap);
    auto right = area;

    osc1Button.setBounds(left.removeFromBottom(28));
    left.removeFromBottom(4);
    display1.setBounds(left);

    osc2Button.setBounds(right.removeFromBottom(28));
    right.removeFromBottom(4);
    display2.setBounds(right);
}

} // namespace fcm::ui

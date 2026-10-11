// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginEditor.h"

#include "plugin/PluginProcessor.h"

namespace fcm
{

namespace
{
std::vector<juce::String> groupsShownElsewhere()
{
    std::vector<juce::String> groups{"ampEnv", "pitchEnv", "punchTail"};
    const auto& registry = dsp::defaultEffectRegistry();
    for (int i = 0; i < registry.count(); ++i)
        groups.emplace_back(std::string(registry.descriptor(i)->id));
    return groups;
}

// Default size of the editor (docs/UI_LAYOUT.md). The Standalone adds the audition strip below it;
// the aspect ratio is fixed for whichever total height applies.
constexpr int kDefaultWidth = 1100;
constexpr int kPanelHeight = 860;
constexpr int kEnvelopeRowHeight = 540;
} // namespace

EnvelopeRow::EnvelopeRow(PluginProcessor& owner)
    : ampPanel(
          owner.getAmpEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::ampConfig(owner.getState())),
      pitchPanel(
          owner.getPitchEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::pitchConfig())
{
    addAndMakeVisible(ampPanel);
    addAndMakeVisible(pitchPanel);
}

void EnvelopeRow::resized()
{
    auto area = getLocalBounds();
    ampPanel.setBounds(area.removeFromLeft(area.getWidth() / 2));
    pitchPanel.setBounds(area);
}

PluginEditor::PluginEditor(PluginProcessor& owner)
    : AudioProcessorEditor(owner),
      // The envelope parameters are shown by the envelope panels and the effects by the FX rack, not by
      // the generic list.
      presetBar(owner.getPresets()), parameterPanel(owner.getState(), groupsShownElsewhere()),
      wavetablePanel(
          owner.getState(), owner.selectorFor(0), owner.selectorFor(1),
          [&owner](int oscillator, const juce::File& file) { owner.loadUserWavetable(oscillator, file); },
          [&owner](const juce::File& file) { owner.loadUserTockSample(file); }, owner.wavetableChanged(),
          [&owner] { return owner.wavetableMessage(); }),
      envelopeRow(owner), fxRack(owner.getState(), owner.getFxOrder(), dsp::defaultEffectRegistry())
{
    addAndMakeVisible(presetBar);
    addAndMakeVisible(parameterPanel);
    addAndMakeVisible(wavetablePanel);
    lowerTabs.setTabBarDepth(28);
    lowerTabs.setOutline(0);
    lowerTabs.addTab("Envelopes", juce::Colour(0xff15161a), &envelopeRow, false);
    lowerTabs.addTab("Effects", juce::Colour(0xff15161a), &fxRack, false);
    addAndMakeVisible(lowerTabs);

    int totalHeight = kPanelHeight + ui::WavetablePanel::kHeight + ui::PresetBar::kHeight;
    if (owner.isStandalone())
    {
        auditionPanel = std::make_unique<ui::AuditionPanel>(owner.getAuditionState());
        addAndMakeVisible(*auditionPanel);
        totalHeight += ui::AuditionPanel::kHeight;
    }

    setResizable(true, true);
    setResizeLimits(kDefaultWidth * 2 / 3, totalHeight * 2 / 3, kDefaultWidth * 2, totalHeight * 2);
    getConstrainer()->setFixedAspectRatio(double(kDefaultWidth) / double(totalHeight));
    setSize(kDefaultWidth, totalHeight);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();

    // Every part keeps its share of the height as the window scales.
    const int designHeight = kPanelHeight + ui::WavetablePanel::kHeight + ui::PresetBar::kHeight +
                             (auditionPanel != nullptr ? ui::AuditionPanel::kHeight : 0);
    // The full height, taken before any strip is cut off: `area` shrinks as the strips are removed.
    const int fullHeight = area.getHeight();
    const auto scaled = [&](int designPixels) { return fullHeight * designPixels / designHeight; };

    if (auditionPanel != nullptr)
        auditionPanel->setBounds(area.removeFromBottom(scaled(ui::AuditionPanel::kHeight)));

    presetBar.setBounds(area.removeFromTop(scaled(ui::PresetBar::kHeight)));
    lowerTabs.setBounds(area.removeFromBottom(scaled(kEnvelopeRowHeight)));

    wavetablePanel.setBounds(area.removeFromBottom(scaled(ui::WavetablePanel::kHeight)));
    parameterPanel.setBounds(area);
}

} // namespace fcm

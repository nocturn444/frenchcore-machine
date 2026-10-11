// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginEditor.h"

#include "plugin/PluginProcessor.h"

namespace fcm
{

namespace
{
// Default size of the editor (docs/UI_LAYOUT.md). The Standalone adds the audition strip below it;
// the aspect ratio is fixed for whichever total height applies.
constexpr int kDefaultWidth = 1100;
constexpr int kPanelHeight = 860;
constexpr int kEnvelopeRowHeight = 540;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& owner)
    : AudioProcessorEditor(owner),
      // The envelope parameters are shown by the envelope panels, not by the generic list.
      parameterPanel(owner.getState(), {"ampEnv", "pitchEnv", "punchTail"}),
      wavetablePanel([&owner](int oscillator, const juce::File& file)
                     { owner.loadUserWavetable(oscillator, file); }, owner.wavetableChanged(),
                     [&owner] { return owner.wavetableMessage(); }),
      ampPanel(
          owner.getAmpEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::ampConfig(owner.getState())),
      pitchPanel(
          owner.getPitchEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::pitchConfig())
{
    addAndMakeVisible(parameterPanel);
    addAndMakeVisible(wavetablePanel);
    addAndMakeVisible(ampPanel);
    addAndMakeVisible(pitchPanel);

    int totalHeight = kPanelHeight + ui::WavetablePanel::kHeight;
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
    const int designHeight = kPanelHeight + ui::WavetablePanel::kHeight +
                             (auditionPanel != nullptr ? ui::AuditionPanel::kHeight : 0);
    // The full height, taken before any strip is cut off: `area` shrinks as the strips are removed.
    const int fullHeight = area.getHeight();
    const auto scaled = [&](int designPixels) { return fullHeight * designPixels / designHeight; };

    if (auditionPanel != nullptr)
        auditionPanel->setBounds(area.removeFromBottom(scaled(ui::AuditionPanel::kHeight)));

    auto envelopes = area.removeFromBottom(scaled(kEnvelopeRowHeight));
    ampPanel.setBounds(envelopes.removeFromLeft(envelopes.getWidth() / 2));
    pitchPanel.setBounds(envelopes);

    wavetablePanel.setBounds(area.removeFromBottom(scaled(ui::WavetablePanel::kHeight)));
    parameterPanel.setBounds(area);
}

} // namespace fcm

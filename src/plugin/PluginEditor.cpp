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
      ampPanel(
          owner.getAmpEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::ampConfig(owner.getState())),
      pitchPanel(
          owner.getPitchEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::pitchConfig())
{
    addAndMakeVisible(parameterPanel);
    addAndMakeVisible(ampPanel);
    addAndMakeVisible(pitchPanel);

    int totalHeight = kPanelHeight;
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

    if (auditionPanel != nullptr)
    {
        // The strip scales with the window like everything else.
        const int stripHeight =
            area.getHeight() * ui::AuditionPanel::kHeight / (kPanelHeight + ui::AuditionPanel::kHeight);
        auditionPanel->setBounds(area.removeFromBottom(stripHeight));
    }

    // The envelope row keeps its share of the height as the window scales.
    auto envelopes = area.removeFromBottom(area.getHeight() * kEnvelopeRowHeight / kPanelHeight);
    ampPanel.setBounds(envelopes.removeFromLeft(envelopes.getWidth() / 2));
    pitchPanel.setBounds(envelopes);

    parameterPanel.setBounds(area);
}

} // namespace fcm

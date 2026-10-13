// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/AuditionPanel.h"
#include "ui/EnvelopePanel.h"
#include "ui/FcmLookAndFeel.h"
#include "ui/FxRackPanel.h"
#include "ui/OscillatorPanel.h"
#include "ui/PresetBar.h"
#include "ui/SectionPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm
{

class PluginProcessor;

// The tock's box: its controls and a button to load a sample of your own.
class TockPanel : public ui::ControlPanel
{
  public:
    TockPanel(juce::AudioProcessorValueTreeState& state, std::function<void(const juce::File&)> onLoad);

  private:
    void choose();

    std::function<void(const juce::File&)> loadRequested;
    juce::TextButton loadButton{"Load sample..."};
    std::unique_ptr<juce::FileChooser> chooser;
};

// The box around the FX rack.
class FxArea : public ui::SectionPanel
{
  public:
    explicit FxArea(ui::FxRackPanel& rackPanel) : SectionPanel("Effects"), rack(rackPanel)
    {
        addAndMakeVisible(rack);
    }

    void resized() override { rack.setBounds(contentArea()); }

  private:
    ui::FxRackPanel& rack;
};

// The editor draws everything on a fixed design grid (docs/UI_LAYOUT.md) and scales the whole of it to
// the window, so the layout never rearranges or squeezes when the window is resized; only its size
// changes. The window keeps the aspect ratio of the grid.
class PluginEditor : public juce::AudioProcessorEditor
{
  public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // The size of the design grid, without and with the Standalone audition strip.
    static constexpr int kDesignWidth = 1500;
    static int designHeight(bool withAudition);

  private:
    void layoutContent();

    // First, so it is destroyed last: every child uses it until then.
    ui::FcmLookAndFeel lookAndFeel;
    // Everything lives in one component that is scaled as a whole.
    juce::Component content;

    ui::PresetBar presetBar;
    ui::OscillatorPanel osc1;
    ui::OscillatorPanel osc2;
    ui::EnvelopePanel ampPanel;
    ui::EnvelopePanel pitchPanel;
    ui::FxRackPanel fxRack;
    FxArea fxArea;
    ui::PlaceholderPanel lfoPanel;
    TockPanel tockPanel;
    ui::ControlPanel punchTailPanel;
    ui::ControlPanel globalPanel;
    ui::PlaceholderPanel otherPanel;
    std::unique_ptr<ui::AuditionPanel> auditionPanel; // Standalone only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fcm

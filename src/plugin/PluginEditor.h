// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/AuditionPanel.h"
#include "ui/EffectsBox.h"
#include "ui/EnvelopePanel.h"
#include "ui/FcmLookAndFeel.h"
#include "ui/OscillatorPanel.h"
#include "ui/PresetBar.h"
#include "ui/SectionPanel.h"
#include "ui/TockWaveform.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm
{

class PluginProcessor;

// The tock's box (docs/specs/tock.md): its controls, a button to load a sample of your own, the
// waveform with the Trim handles and the Detect button.
class TockPanel : public ui::ControlPanel
{
  public:
    TockPanel(juce::AudioProcessorValueTreeState& state, const core::TockSelector& selector,
              std::function<void(const juce::File&)> onLoad);

  private:
    void choose();
    void detect();

    juce::AudioProcessorValueTreeState& parameters;
    const core::TockSelector& tockSelector;
    std::function<void(const juce::File&)> loadRequested;
    juce::TextButton loadButton{"Load sample..."};
    juce::TextButton detectButton{"Detect"};
    ui::TockWaveform waveform;
    std::unique_ptr<juce::FileChooser> chooser;
};

// The noise oscillator's box (docs/specs/noise-oscillator.md): its controls and a button to load a
// sound of your own. It takes the place that was kept for the LFOs.
class NoisePanel : public ui::ControlPanel
{
  public:
    NoisePanel(juce::AudioProcessorValueTreeState& state, std::function<void(const juce::File&)> onLoad);

  private:
    void choose();

    std::function<void(const juce::File&)> loadRequested;
    juce::TextButton loadButton{"Load..."};
    std::unique_ptr<juce::FileChooser> chooser;
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
    ui::EffectsBox fxArea;
    NoisePanel noisePanel;
    TockPanel tockPanel;
    ui::ControlPanel punchTailPanel;
    ui::ControlPanel globalPanel;
    ui::PlaceholderPanel otherPanel;
    std::unique_ptr<ui::AuditionPanel> auditionPanel; // Standalone only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fcm

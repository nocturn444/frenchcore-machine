// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/AuditionPanel.h"
#include "ui/EnvelopePanel.h"
#include "ui/FxRackPanel.h"
#include "ui/GenericParameterPanel.h"
#include "ui/PresetBar.h"
#include "ui/WavetablePanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm
{

class PluginProcessor;

// The two envelope panels side by side: one tab of the lower area.
class EnvelopeRow : public juce::Component
{
  public:
    explicit EnvelopeRow(PluginProcessor& owner);

    void resized() override;

    ui::EnvelopePanel ampPanel;
    ui::EnvelopePanel pitchPanel;
};

class PluginEditor : public juce::AudioProcessorEditor
{
  public:
    explicit PluginEditor(PluginProcessor&);

    void resized() override;

  private:
    ui::PresetBar presetBar;
    ui::GenericParameterPanel parameterPanel;
    ui::WavetablePanel wavetablePanel;
    EnvelopeRow envelopeRow;
    ui::FxRackPanel fxRack;
    // The lower area: the envelopes and the effects share it (docs/specs/fx-rack.md).
    juce::TabbedComponent lowerTabs{juce::TabbedButtonBar::TabsAtTop};
    std::unique_ptr<ui::AuditionPanel> auditionPanel; // Standalone only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fcm

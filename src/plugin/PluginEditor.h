// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/AuditionPanel.h"
#include "ui/GenericParameterPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace fcm
{

class PluginProcessor;

class PluginEditor : public juce::AudioProcessorEditor
{
  public:
    explicit PluginEditor(PluginProcessor&);

    void resized() override;

  private:
    ui::GenericParameterPanel parameterPanel;
    std::unique_ptr<ui::AuditionPanel> auditionPanel; // Standalone only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fcm

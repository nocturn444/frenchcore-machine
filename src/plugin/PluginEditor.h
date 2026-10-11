// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/GenericParameterPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fcm

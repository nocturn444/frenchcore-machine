// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/ParameterControl.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// The Mixer tab of the Effects box (docs/specs/mixer-and-buses.md section 2): one strip per sound
// source (Level, Destination, Mute, Solo) and one per bus (Level, Apply To). The Level of a source is
// the source's own Level parameter, so turning it here turns it on its panel. A source that is off is
// drawn dimmed. Everything is attached to parameters; the panel holds no state.
class MixerPanel : public juce::Component
{
  public:
    explicit MixerPanel(juce::AudioProcessorValueTreeState& state);

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    // A column of controls under a name.
    struct Strip : public juce::Component
    {
        juce::String name;
        std::unique_ptr<ParameterControl> level;
        std::unique_ptr<ParameterControl> menu; // Destination or Apply To
        std::unique_ptr<ParameterControl> mute;
        std::unique_ptr<ParameterControl> solo;
        std::unique_ptr<juce::ParameterAttachment> dimming; // follows the source's On switch

        void paint(juce::Graphics& g) override;
        void resized() override;
    };

    std::vector<std::unique_ptr<Strip>> sources;
    std::vector<std::unique_ptr<Strip>> buses;
};

} // namespace fcm::ui

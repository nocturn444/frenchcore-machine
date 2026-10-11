// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// Phase 2 stand-in for the real panels of docs/UI_LAYOUT.md: one slider or switch per parameter,
// in a column per parameter group ("global", "osc1", "osc2": the ID prefix). It only talks to the
// parameters through attachments, like every panel must (docs/ARCHITECTURE.md §7).
class GenericParameterPanel : public juce::Component
{
  public:
    // Groups listed in `excludedGroups` (ID prefixes) are skipped: a dedicated panel shows them.
    // With `onlyGroup` set, only that group (an effect's, for the FX rack) is shown, under `title`.
    explicit GenericParameterPanel(juce::AudioProcessorValueTreeState& state,
                                   const std::vector<juce::String>& excludedGroups = {},
                                   const juce::String& onlyGroup = {}, const juce::String& title = {});

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    struct Row
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ComboBoxParameterAttachment> comboAttachment;
        std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
        std::unique_ptr<juce::ButtonParameterAttachment> buttonAttachment;

        juce::Component& control()
        {
            if (slider != nullptr)
                return *slider;
            return combo != nullptr ? static_cast<juce::Component&>(*combo) : *toggle;
        }
    };

    struct Group
    {
        std::unique_ptr<juce::Label> heading;
        std::vector<Row> rows;
    };

    static juce::String headingFor(const juce::String& prefix);
    void addRow(Group& group, juce::RangedAudioParameter& parameter);

    std::vector<Group> groups;
};

} // namespace fcm::ui

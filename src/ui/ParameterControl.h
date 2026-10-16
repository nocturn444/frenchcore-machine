// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// One parameter as a control with its name: a knob for a number, a switch for an on/off, a menu for a
// choice. It is attached to the parameter in the usual way (docs/ARCHITECTURE.md §7) and picks its kind
// from the parameter, so a panel only says which parameters it shows.
class ParameterControl : public juce::Component
{
  public:
    enum class Kind
    {
        knob,
        toggle,
        choice
    };

    // `compact` is for a strip where the normal control would not fit: a number is a thin horizontal
    // slider with its name on the left, a menu or switch fills its area with no name above.
    ParameterControl(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                     juce::String label = {}, bool compact = false);

    Kind kind() const noexcept { return controlKind; }

    // Whether the mouse wheel turns a knob (the default). Off, the wheel goes to the parent, so a
    // scrolling list of strips scrolls over its knobs instead of changing them by accident.
    void setWheelAdjusts(bool adjusts);

    // Calls `listener` with the new state when the user clicks a switch (not when automation moves it).
    void addToggledListener(std::function<void(bool)> listener);
    bool isValid() const noexcept { return parameter != nullptr; }

    // Height of a switch or menu cell; knobs take whatever height they are given.
    static constexpr int kSwitchHeight = 40;

    void resized() override;

  private:
    Kind controlKind = Kind::knob;
    bool isCompact = false;
    juce::RangedAudioParameter* parameter = nullptr;
    std::vector<std::function<void(bool)>> toggledListeners;

    juce::Label name;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> buttonAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> comboAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterControl)
};

} // namespace fcm::ui

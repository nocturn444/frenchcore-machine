// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fcm::ui
{

// The look shared by the whole editor (docs/UI_LAYOUT.md): for now the stock look with smaller text in
// menus, so the names of effect types and wavetables fit the narrow cells of the grid. The visual
// design pass will grow this class; no panel changes its fonts itself.
class FcmLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font(juce::FontOptions(14.0f)); }
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return juce::Font(juce::FontOptions(juce::jmin(14.0f, static_cast<float>(buttonHeight) * 0.6f)));
    }
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(juce::FontOptions(13.0f));
        return label;
    }
};

} // namespace fcm::ui

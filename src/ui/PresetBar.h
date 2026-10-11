// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/PresetManager.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fcm::ui
{

// The strip at the top of the window: preset menu, previous and next, name field and Save
// (docs/specs/preset-browser.md). It talks to the sound only through core::PresetManager.
class PresetBar : public juce::Component, private juce::ChangeListener
{
  public:
    static constexpr int kHeight = 32;

    explicit PresetBar(core::PresetManager& manager);
    ~PresetBar() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void showMenu();
    void saveClicked();
    void doSave(const juce::String& name);
    void showCurrent();

    core::PresetManager& presets;
    juce::TextButton previousButton{"<"};
    juce::TextButton menuButton;
    juce::TextButton nextButton{">"};
    juce::TextEditor nameField;
    juce::TextButton saveButton{"Save"};
    juce::Label messageLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};

} // namespace fcm::ui

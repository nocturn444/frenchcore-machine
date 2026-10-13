// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/PresetBar.h"

#include "ui/Style.h"

namespace fcm::ui
{

PresetBar::PresetBar(core::PresetManager& manager, std::function<juce::String()> extra,
                     juce::ChangeBroadcaster& extraChanges)
    : presets(manager), extraMessage(std::move(extra)), extraBroadcaster(extraChanges)
{
    for (auto* button : {&previousButton, &menuButton, &nextButton, &saveButton})
        addAndMakeVisible(*button);

    previousButton.onClick = [this] { presets.previous(); };
    nextButton.onClick = [this] { presets.next(); };
    menuButton.onClick = [this] { showMenu(); };
    saveButton.onClick = [this] { saveClicked(); };

    nameField.setTextToShowWhenEmpty("Name for a new preset", juce::Colours::grey);
    nameField.setInputRestrictions(core::kMaxPresetNameLength);
    nameField.onReturnKey = [this] { saveClicked(); };
    addAndMakeVisible(nameField);

    messageLabel.setJustificationType(juce::Justification::centredLeft);
    messageLabel.setColour(juce::Label::textColourId, style::kWarning);
    messageLabel.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(messageLabel);

    presets.addChangeListener(this);
    extraBroadcaster.addChangeListener(this);
    showCurrent();
}

PresetBar::~PresetBar()
{
    presets.removeChangeListener(this);
    extraBroadcaster.removeChangeListener(this);
}

void PresetBar::paint(juce::Graphics& g)
{
    g.fillAll(style::kHeader);
}

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced(4, 3);
    previousButton.setBounds(area.removeFromLeft(28));
    area.removeFromLeft(4);
    menuButton.setBounds(area.removeFromLeft(240));
    area.removeFromLeft(4);
    nextButton.setBounds(area.removeFromLeft(28));
    area.removeFromLeft(16);
    saveButton.setBounds(area.removeFromRight(60));
    area.removeFromRight(4);
    nameField.setBounds(area.removeFromRight(200));
    area.removeFromRight(8);
    messageLabel.setBounds(area);
}

void PresetBar::changeListenerCallback(juce::ChangeBroadcaster*)
{
    showCurrent();
}

void PresetBar::showCurrent()
{
    menuButton.setButtonText(presets.currentName());
    // A preset message wins; otherwise the loader's last message (a refused wavetable, for example).
    auto message = presets.lastMessage();
    if (message.isEmpty() && extraMessage)
        message = extraMessage();
    messageLabel.setText(message, juce::dontSendNotification);
}

void PresetBar::showMenu()
{
    // Read again each time, so a file added from outside shows up.
    presets.refresh();

    juce::PopupMenu menu;
    const auto& entries = presets.entries();
    const int numFactory = presets.numFactory();
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        if (i == 0)
            menu.addSectionHeader("Factory");
        if (i == numFactory)
            menu.addSectionHeader("Yours");
        menu.addItem(i + 1, entries[static_cast<size_t>(i)].name, true, i == presets.currentIndex());
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
                       [safe = juce::Component::SafePointer<PresetBar>(this)](int result)
                       {
                           if (safe != nullptr && result > 0)
                               safe->presets.load(result - 1);
                       });
}

void PresetBar::saveClicked()
{
    const auto name = nameField.getText().trim();
    if (!presets.wouldReplace(name))
    {
        doSave(name);
        return;
    }

    // Replacing a file is the one thing here that loses something, so ask first.
    const auto options = juce::MessageBoxOptions()
                             .withIconType(juce::MessageBoxIconType::QuestionIcon)
                             .withTitle("Replace preset?")
                             .withMessage("A preset called \"" + name + "\" already exists.")
                             .withButton("Replace")
                             .withButton("Cancel");
    juce::AlertWindow::showAsync(options,
                                 [name, safe = juce::Component::SafePointer<PresetBar>(this)](int result)
                                 {
                                     if (safe != nullptr && result == 1)
                                         safe->doSave(name);
                                 });
}

void PresetBar::doSave(const juce::String& name)
{
    if (presets.save(name))
        nameField.clear();
}

} // namespace fcm::ui

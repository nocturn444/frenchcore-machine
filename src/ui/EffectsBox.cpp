// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EffectsBox.h"

#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
constexpr const char* kTabNames[] = {"Mixer", "Bus A", "Bus B", "Main"};
constexpr int kTabGap = 4;
constexpr int kBelowTabs = 6;
} // namespace

namespace
{
// Gives every rack the same environment plus the way to open the larger editor in this box.
DecorationEnvironment
withEditor(DecorationEnvironment environment,
           std::function<void(std::unique_ptr<juce::Component>, const juce::String&)> show)
{
    environment.showEditor = std::move(show);
    return environment;
}
} // namespace

EffectsBox::EffectsBox(juce::AudioProcessorValueTreeState& state, core::FxRackState& rackState,
                       const dsp::EffectRegistry& registry, DecorationEnvironment environment)
    : SectionPanel("Effects"), racks(rackState), mixer(state),
      rackPanels{FxRackPanel(state, rackState, dsp::RackId::busA, registry,
                             withEditor(environment, [this](auto content, const auto& title)
                                        { showEditor(std::move(content), title); })),
                 FxRackPanel(state, rackState, dsp::RackId::busB, registry,
                             withEditor(environment, [this](auto content, const auto& title)
                                        { showEditor(std::move(content), title); })),
                 FxRackPanel(state, rackState, dsp::RackId::main, registry,
                             withEditor(environment, [this](auto content, const auto& title)
                                        { showEditor(std::move(content), title); }))}
{
    addChildComponent(mixer);
    for (auto& panel : rackPanels)
        addChildComponent(panel);

    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& button = tabs[static_cast<size_t>(i)];
        button.setButtonText(kTabNames[i]);
        button.setClickingTogglesState(true);
        button.setRadioGroupId(1);
        button.setColour(juce::TextButton::buttonOnColourId, style::kAccent.darker(0.5f));
        button.setColour(juce::TextButton::textColourOnId, style::kText);
        button.onClick = [this, i] { showTab(static_cast<Tab>(i)); };
        addAndMakeVisible(button);
    }
    showTab(mainTab);
    racks.addChangeListener(this);
}

EffectsBox::~EffectsBox()
{
    racks.removeChangeListener(this);
}

void EffectsBox::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // An effect may have been removed or moved: the editor of one of them must not outlive it.
    closeEditor();
}

EffectsBox::Editor::Editor(std::unique_ptr<juce::Component> editorContent, const juce::String& editorTitle,
                           std::function<void()> finished)
    : content(std::move(editorContent)), title(editorTitle), onDone(std::move(finished))
{
    done.onClick = [this]
    {
        if (onDone)
            onDone();
    };
    addAndMakeVisible(done);
    if (content != nullptr)
        addAndMakeVisible(*content);
    setWantsKeyboardFocus(true);
}

void EffectsBox::Editor::paint(juce::Graphics& g)
{
    g.setColour(style::kPanel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), style::kCornerSize);
    g.setColour(style::kPanelBorder);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), style::kCornerSize, 1.0f);
    g.setColour(style::kText);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(title, getLocalBounds().removeFromTop(kHeaderHeight).reduced(8, 0),
               juce::Justification::centredLeft);
}

void EffectsBox::Editor::resized()
{
    auto area = getLocalBounds().reduced(4);
    auto header = area.removeFromTop(kHeaderHeight);
    done.setBounds(header.removeFromRight(70).reduced(0, 2));
    if (content != nullptr)
        content->setBounds(area);
}

bool EffectsBox::Editor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onDone)
            onDone();
        return true;
    }
    return false;
}

void EffectsBox::showEditor(std::unique_ptr<juce::Component> content, const juce::String& title)
{
    // Replace the one that is open; the callbacks come from a button or a key of the old one, so it
    // is destroyed after they return.
    closeEditor();
    if (content == nullptr)
        return;

    editor = std::make_unique<Editor>(std::move(content), title,
                                      [safe = juce::Component::SafePointer<EffectsBox>(this)]
                                      {
                                          if (safe != nullptr)
                                              safe->closeEditor();
                                      });
    addAndMakeVisible(*editor);
    editor->toFront(true);
    resized();
    editor->grabKeyboardFocus();
}

void EffectsBox::closeEditor()
{
    if (editor == nullptr)
        return;

    // It can be closed from inside a click handler of a sibling (a tab) or of its own Done button, and
    // JUCE walks the siblings of a radio button while handling its click: hide it now, delete it a
    // moment later.
    editor->setVisible(false);
    std::shared_ptr<Editor> leaving(editor.release());
    juce::MessageManager::callAsync([leaving] {});
}

void EffectsBox::showTab(Tab tab)
{
    closeEditor();
    shown = tab;
    for (int i = 0; i < kNumTabs; ++i)
        tabs[static_cast<size_t>(i)].setToggleState(i == tab, juce::dontSendNotification);

    mixer.setVisible(tab == mixerTab);
    rackPanels[0].setVisible(tab == busATab);
    rackPanels[1].setVisible(tab == busBTab);
    rackPanels[2].setVisible(tab == mainTab);
}

void EffectsBox::resized()
{
    auto area = contentArea();
    auto row = area.removeFromTop(kTabHeight);
    const int width = (row.getWidth() - kTabGap * (kNumTabs - 1)) / kNumTabs;
    for (auto& button : tabs)
    {
        button.setBounds(row.removeFromLeft(width));
        row.removeFromLeft(kTabGap);
    }

    area.removeFromTop(kBelowTabs);
    mixer.setBounds(area);
    for (auto& panel : rackPanels)
        panel.setBounds(area);
    if (editor != nullptr)
        editor->setBounds(area);
}

} // namespace fcm::ui

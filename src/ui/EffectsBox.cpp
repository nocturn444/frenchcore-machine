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

EffectsBox::EffectsBox(juce::AudioProcessorValueTreeState& state, core::FxRackState& racks,
                       const dsp::EffectRegistry& registry)
    : SectionPanel("Effects"), mixer(state),
      rackPanels{FxRackPanel(state, racks, dsp::RackId::busA, registry),
                 FxRackPanel(state, racks, dsp::RackId::busB, registry),
                 FxRackPanel(state, racks, dsp::RackId::main, registry)}
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
}

void EffectsBox::showTab(Tab tab)
{
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
}

} // namespace fcm::ui

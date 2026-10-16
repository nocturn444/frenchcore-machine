// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/MixerPanel.h"

#include "core/ParameterIds.h"
#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
constexpr int kNameHeight = 24;
constexpr int kMenuHeight = ParameterControl::kSwitchHeight + 4;
constexpr int kSwitchRowHeight = 28;
constexpr int kStripGap = 6;
constexpr int kDividerGap = 14;
} // namespace

void MixerPanel::Strip::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(style::kHeader);
    g.fillRoundedRectangle(bounds, style::kCornerSize);
    g.setColour(style::kText);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(name, getLocalBounds().removeFromTop(kNameHeight), juce::Justification::centred);
}

void MixerPanel::Strip::resized()
{
    auto area = getLocalBounds().reduced(4, 0);
    area.removeFromTop(kNameHeight);

    if (solo != nullptr)
    {
        // One under the other: a strip is narrow, and the switches need their full names.
        auto rows = area.removeFromBottom(2 * kSwitchRowHeight + 6).reduced(0, 3);
        mute->setBounds(rows.removeFromTop(kSwitchRowHeight));
        solo->setBounds(rows.removeFromTop(kSwitchRowHeight));
    }
    menu->setBounds(area.removeFromBottom(kMenuHeight));
    level->setBounds(area.reduced(0, 4));
}

MixerPanel::MixerPanel(juce::AudioProcessorValueTreeState& state)
{
    namespace Ids = core::ParameterIds;

    for (const auto& source : Ids::kMixerSources)
    {
        auto strip = std::make_unique<Strip>();
        strip->name = source.name;
        strip->level = std::make_unique<ParameterControl>(state, source.level, "Level");
        strip->menu = std::make_unique<ParameterControl>(state, Ids::mixerDestination(source), "Destination");
        strip->mute = std::make_unique<ParameterControl>(state, Ids::mixerMute(source), "Mute", true);
        strip->solo = std::make_unique<ParameterControl>(state, Ids::mixerSolo(source), "Solo", true);
        for (auto* child : {strip->level.get(), strip->menu.get(), strip->mute.get(), strip->solo.get()})
            strip->addAndMakeVisible(*child);

        // A source that is off is drawn dimmed.
        if (auto* on = state.getParameter(source.on))
        {
            auto* shown = strip.get();
            strip->dimming = std::make_unique<juce::ParameterAttachment>(
                *on, [shown](float value) { shown->setAlpha(value >= 0.5f ? 1.0f : 0.45f); });
            strip->dimming->sendInitialUpdate();
        }

        addAndMakeVisible(*strip);
        sources.push_back(std::move(strip));
    }

    for (int bus = 0; bus < 2; ++bus)
    {
        auto strip = std::make_unique<Strip>();
        strip->name = Ids::kBusNames[static_cast<size_t>(bus)];
        strip->level = std::make_unique<ParameterControl>(state, Ids::busLevel(bus), "Level");
        strip->menu = std::make_unique<ParameterControl>(state, Ids::busRegion(bus), "Apply To");
        for (auto* child : {strip->level.get(), strip->menu.get()})
            strip->addAndMakeVisible(*child);
        addAndMakeVisible(*strip);
        buses.push_back(std::move(strip));
    }
}

void MixerPanel::paint(juce::Graphics& g)
{
    // A thin line between the sounds and the buses they go to.
    if (!sources.empty() && !buses.empty())
    {
        const int x = sources.back()->getRight() + kDividerGap / 2;
        g.setColour(style::kPanelBorder);
        g.drawLine(static_cast<float>(x), 4.0f, static_cast<float>(x), static_cast<float>(getHeight() - 4),
                   1.0f);
    }
}

void MixerPanel::resized()
{
    const int count = static_cast<int>(sources.size() + buses.size());
    if (count == 0)
        return;

    const int usable = getWidth() - kStripGap * (count - 1) - kDividerGap;
    const int width = usable / count;
    int x = 0;
    for (size_t i = 0; i < sources.size() + buses.size(); ++i)
    {
        auto& strip = i < sources.size() ? sources[i] : buses[i - sources.size()];
        if (i == sources.size())
            x += kDividerGap;
        strip->setBounds(x, 0, width, getHeight());
        x += width + kStripGap;
    }
}

} // namespace fcm::ui

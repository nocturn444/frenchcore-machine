// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/FxRackPanel.h"

#include <algorithm>

namespace fcm::ui
{

FxRackPanel::Slot::Slot(FxRackPanel& owner, juce::String id, const juce::String& displayName,
                        juce::AudioProcessorValueTreeState& state)
    : rack(owner), effectId(std::move(id)), name(displayName), controls(state, {}, effectId, displayName)
{
    addAndMakeVisible(controls);
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void FxRackPanel::Slot::paint(juce::Graphics& g)
{
    const auto handle = getLocalBounds().removeFromTop(kHandleHeight);
    g.setColour(dragging ? juce::Colour(0xff3a6ea5) : juce::Colour(0xff2a2d36));
    g.fillRect(handle);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(juce::String::fromUTF8("\xe2\x89\xa1  drag to reorder"), handle.reduced(8, 0),
               juce::Justification::centredLeft);
}

void FxRackPanel::Slot::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kHandleHeight);
    controls.setBounds(area);
}

void FxRackPanel::Slot::mouseDown(const juce::MouseEvent& e)
{
    if (e.getMouseDownY() < kHandleHeight)
    {
        dragging = true;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
}

void FxRackPanel::Slot::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging)
        rack.dragSlot(effectId, e);
}

void FxRackPanel::Slot::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

FxRackPanel::FxRackPanel(juce::AudioProcessorValueTreeState& state, core::FxOrderState& orderState,
                         const dsp::EffectRegistry& registry)
    : order(orderState)
{
    for (int i = 0; i < registry.count(); ++i)
    {
        const auto& descriptor = *registry.descriptor(i);
        slots.push_back(std::make_unique<Slot>(*this, juce::String(std::string(descriptor.id)),
                                               juce::String(std::string(descriptor.displayName)), state));
        addAndMakeVisible(*slots.back());
    }

    order.addChangeListener(this);
}

FxRackPanel::~FxRackPanel()
{
    order.removeChangeListener(this);
}

void FxRackPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15161a));
    if (slots.empty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawText("No effects", getLocalBounds(), juce::Justification::centred);
    }
}

void FxRackPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    layoutSlots();
}

void FxRackPanel::resized()
{
    layoutSlots();
}

void FxRackPanel::layoutSlots()
{
    if (slots.empty())
        return;

    const int count = static_cast<int>(slots.size());
    const int width = getWidth() / count;
    const auto& ids = order.ids();

    for (auto& slot : slots)
    {
        const auto position = std::find(ids.begin(), ids.end(), slot->id().toStdString()) - ids.begin();
        slot->setBounds(static_cast<int>(position) * width, 0, width, getHeight());
    }
}

void FxRackPanel::dragSlot(const juce::String& effectId, const juce::MouseEvent& e)
{
    if (slots.empty())
        return;

    const auto& ids = order.ids();
    const auto current = std::find(ids.begin(), ids.end(), effectId.toStdString()) - ids.begin();
    const double x = e.getEventRelativeTo(this).position.x;
    const int target =
        core::FxOrderState::positionAt(x, static_cast<double>(getWidth()) / static_cast<double>(slots.size()),
                                       static_cast<int>(slots.size()));
    if (target != current)
        order.move(static_cast<int>(current), target);
}

} // namespace fcm::ui

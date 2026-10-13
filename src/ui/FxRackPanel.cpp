// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/FxRackPanel.h"

#include "core/ParameterIds.h"
#include "ui/EffectDecorations.h"
#include "ui/Style.h"

#include <algorithm>

namespace fcm::ui
{

namespace
{
constexpr int kSlotGap = 4;
constexpr int kKnobRowHeight = 78;
constexpr int kGroupKnobRowHeight = 64;
constexpr int kGroupTitleHeight = 16;
constexpr int kMixWidth = 200;
constexpr int kOnWidth = 84;
constexpr int kBodyInset = 6;

// "Band 1 Freq" with the group "Band 1" gives "Freq".
juce::String withoutGroup(const juce::String& name, const juce::String& group)
{
    return group.isNotEmpty() && name.startsWith(group + " ") ? name.substring(group.length() + 1) : name;
}
} // namespace

FxRackPanel::Slot::Slot(FxRackPanel& owner, const dsp::EffectRegistry& registry, int registryIndex,
                        juce::AudioProcessorValueTreeState& state)
    : rack(owner)
{
    const auto& descriptor = *registry.descriptor(registryIndex);
    effectId = juce::String(std::string(descriptor.id));
    name = juce::String(std::string(descriptor.displayName));

    onSwitch = std::make_unique<ParameterControl>(
        state, juce::String(core::ParameterIds::effect(descriptor.id, core::ParameterIds::kEffectEnabled)),
        "On", true);
    mixSlider = std::make_unique<ParameterControl>(
        state, juce::String(core::ParameterIds::effect(descriptor.id, core::ParameterIds::kEffectMix)), "Mix",
        true);
    addAndMakeVisible(*onSwitch);
    addAndMakeVisible(*mixSlider);

    if (const auto* extra = decorationFor(descriptor.id); extra != nullptr && extra->make)
    {
        decoration = extra->make(state);
        decorationHeight = extra->height;
        addAndMakeVisible(*decoration);
    }

    // The controls, in the effect's own order. A suffix like "b1.freq" belongs to the group "b1".
    for (const auto& spec : registry.parameters(registryIndex))
    {
        const juce::String suffix(std::string(spec.idSuffix));
        const juce::String groupKey =
            suffix.contains(".") ? suffix.upToFirstOccurrenceOf(".", false, false) : juce::String();
        const juce::String specName(std::string(spec.name));
        const juce::String groupName =
            groupKey.isNotEmpty() ? specName.upToLastOccurrenceOf(" ", false, false) : juce::String();

        auto found =
            std::find_if(groups.begin(), groups.end(), [&](const Group& g) { return g.name == groupName; });
        if (found == groups.end())
        {
            groups.emplace_back();
            groups.back().name = groupName;
            found = groups.end() - 1;
        }

        auto control = std::make_unique<ParameterControl>(
            state, juce::String(core::ParameterIds::effect(descriptor.id, spec.idSuffix)),
            withoutGroup(specName, groupName));
        if (!control->isValid())
            continue;
        addAndMakeVisible(*control);
        (control->kind() == ParameterControl::Kind::knob ? found->knobs : found->switches)
            .push_back(std::move(control));
    }

    // The height it needs: the handle, the picture and either one row (an effect without sub-groups) or
    // a title, a row of switches and a row of knobs.
    const bool grouped = groups.size() > 1 || (!groups.empty() && groups.front().name.isNotEmpty());
    height = kHandleHeight + decorationHeight + 2 * kBodyInset;
    height +=
        grouped ? kGroupTitleHeight + ParameterControl::kSwitchHeight + kGroupKnobRowHeight : kKnobRowHeight;
}

void FxRackPanel::Slot::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(style::kPanel);
    g.fillRoundedRectangle(bounds, style::kCornerSize);
    g.setColour(style::kPanelBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), style::kCornerSize, 1.0f);

    const auto handle = getLocalBounds().removeFromTop(kHandleHeight).toFloat();
    g.setColour(dragging ? juce::Colour(0xff3a6ea5) : style::kHeader);
    g.fillRoundedRectangle(handle, style::kCornerSize);
    g.setColour(style::kText);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8("\xe2\x89\xa1  ") + name, handle.reduced(8.0f, 0.0f),
               juce::Justification::centredLeft);

    // Group titles over the columns.
    g.setColour(style::kMutedText);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    if (groups.size() > 1)
    {
        auto body = getLocalBounds().reduced(kBodyInset, 0);
        body.removeFromTop(kHandleHeight + decorationHeight + kBodyInset);
        const int columnWidth = body.getWidth() / static_cast<int>(groups.size());
        for (size_t i = 0; i < groups.size(); ++i)
            g.drawText(groups[i].name,
                       juce::Rectangle<int>(body.getX() + static_cast<int>(i) * columnWidth, body.getY(),
                                            columnWidth, kGroupTitleHeight),
                       juce::Justification::centred);
    }
}

void FxRackPanel::Slot::resized()
{
    auto area = getLocalBounds();
    auto handle = area.removeFromTop(kHandleHeight);
    handle.removeFromRight(6);
    mixSlider->setBounds(handle.removeFromRight(kMixWidth));
    onSwitch->setBounds(handle.removeFromRight(kOnWidth));

    area = area.reduced(kBodyInset, kBodyInset);
    if (decoration != nullptr)
        decoration->setBounds(area.removeFromTop(decorationHeight));

    if (groups.empty())
        return;

    const bool grouped = groups.size() > 1 || groups.front().name.isNotEmpty();
    if (grouped)
        area.removeFromTop(kGroupTitleHeight);

    const int columnWidth = area.getWidth() / static_cast<int>(groups.size());
    for (size_t i = 0; i < groups.size(); ++i)
    {
        auto& group = groups[i];
        auto column = juce::Rectangle<int>(area.getX() + static_cast<int>(i) * columnWidth, area.getY(),
                                           columnWidth, area.getHeight())
                          .reduced(2, 0);

        if (grouped)
        {
            // Switches and menus on one row, the knobs on the next.
            auto switchRow = column.removeFromTop(ParameterControl::kSwitchHeight);
            const int numSwitches = static_cast<int>(group.switches.size());
            for (int s = 0; s < numSwitches; ++s)
            {
                const bool isToggle =
                    group.switches[static_cast<size_t>(s)]->kind() == ParameterControl::Kind::toggle;
                // A switch needs only its tick and "On"; the menus share the rest.
                const int width = isToggle ? juce::jmin(46, switchRow.getWidth() / 3)
                                           : switchRow.getWidth() / (numSwitches - s);
                group.switches[static_cast<size_t>(s)]->setBounds(switchRow.removeFromLeft(width));
            }
            const int numKnobs = static_cast<int>(group.knobs.size());
            for (int k = 0; k < numKnobs; ++k)
                group.knobs[static_cast<size_t>(k)]->setBounds(
                    column.removeFromLeft(column.getWidth() / (numKnobs - k)));
        }
        else
        {
            // One row: menus and switches first (Type before Drive), then the knobs. A menu gets more width
            // than a knob, a switch a little less.
            std::vector<ParameterControl*> all;
            for (auto& control : group.switches)
                all.push_back(control.get());
            for (auto& control : group.knobs)
                all.push_back(control.get());
            const auto weightOf = [](const ParameterControl* control)
            {
                return control->kind() == ParameterControl::Kind::choice
                           ? 1.8f
                           : (control->kind() == ParameterControl::Kind::toggle ? 0.8f : 1.0f);
            };
            float totalWeight = 0.0f;
            for (const auto* control : all)
                totalWeight += weightOf(control);
            const int rowWidth = column.getWidth();
            float soFar = 0.0f;
            int used = 0;
            for (auto* control : all)
            {
                soFar += weightOf(control);
                const int right = juce::roundToInt(static_cast<float>(rowWidth) * soFar / totalWeight);
                control->setBounds(column.getX() + used, column.getY(), right - used, column.getHeight());
                used = right;
            }
        }
    }
}

void FxRackPanel::Slot::mouseDown(const juce::MouseEvent& e)
{
    if (e.getMouseDownY() < kHandleHeight && e.originalComponent == this)
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
        slots.push_back(std::make_unique<Slot>(*this, registry, i, state));
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
    if (slots.empty())
    {
        g.setColour(style::kMutedText);
        g.drawText("No effects", getLocalBounds(), juce::Justification::centred);
    }
}

int FxRackPanel::preferredHeight() const
{
    int total = 0;
    for (const auto& slot : slots)
        total += slot->preferredHeight() + kSlotGap;
    return juce::jmax(0, total - kSlotGap);
}

void FxRackPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    layoutSlots();
}

void FxRackPanel::resized()
{
    layoutSlots();
}

std::vector<FxRackPanel::Slot*> FxRackPanel::slotsInOrder() const
{
    std::vector<Slot*> result;
    for (const auto& id : order.ids())
        for (const auto& slot : slots)
            if (slot->id().toStdString() == id)
                result.push_back(slot.get());
    return result;
}

void FxRackPanel::layoutSlots()
{
    int y = 0;
    for (auto* slot : slotsInOrder())
    {
        slot->setBounds(0, y, getWidth(), slot->preferredHeight());
        y += slot->preferredHeight() + kSlotGap;
    }
}

void FxRackPanel::dragSlot(const juce::String& effectId, const juce::MouseEvent& e)
{
    const auto ordered = slotsInOrder();
    if (ordered.size() < 2)
        return;

    std::vector<double> sizes;
    int current = 0;
    for (size_t i = 0; i < ordered.size(); ++i)
    {
        sizes.push_back(ordered[i]->preferredHeight() + kSlotGap);
        if (ordered[i]->id() == effectId)
            current = static_cast<int>(i);
    }

    const int target = core::FxOrderState::positionIn(sizes, e.getEventRelativeTo(this).position.y);
    if (target != current)
        order.move(current, target);
}

} // namespace fcm::ui

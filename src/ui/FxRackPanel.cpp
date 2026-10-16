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
constexpr int kButtonWidth = 30;
constexpr int kAddButtonHeight = 30;
constexpr int kEmptyNoteHeight = 26;
constexpr int kScrollBarWidth = 12;

// The controls that are visible: a control that depends on another parameter may be hidden.
std::vector<ParameterControl*> shown(const std::vector<std::unique_ptr<ParameterControl>>& controls)
{
    std::vector<ParameterControl*> result;
    for (const auto& control : controls)
        if (control->isVisible())
            result.push_back(control.get());
    return result;
}

// "Band 1 Freq" with the group "Band 1" gives "Freq".
juce::String withoutGroup(const juce::String& name, const juce::String& group)
{
    return group.isNotEmpty() && name.startsWith(group + " ") ? name.substring(group.length() + 1) : name;
}
} // namespace

FxRackPanel::Slot::Slot(FxRackPanel& owner, const dsp::EffectRegistry& registry,
                        const std::string& instanceId, juce::AudioProcessorValueTreeState& state)
    : rack(owner), instance(instanceId)
{
    const int instanceIndex = registry.indexOfInstance(instanceId);
    const int registryIndex = dsp::EffectRegistry::effectOf(instanceIndex);
    const int copy = dsp::EffectRegistry::copyOf(instanceIndex);
    const auto& descriptor = *registry.descriptor(registryIndex);
    name = juce::String(std::string(descriptor.displayName)) +
           (copy > 1 ? " " + juce::String(copy) : juce::String());

    onSwitch = std::make_unique<ParameterControl>(
        state, juce::String(core::ParameterIds::effect(instanceId, core::ParameterIds::kEffectEnabled)), "On",
        true);
    mixSlider = std::make_unique<ParameterControl>(
        state, juce::String(core::ParameterIds::effect(instanceId, core::ParameterIds::kEffectMix)), "Mix",
        true);
    addAndMakeVisible(*onSwitch);
    addAndMakeVisible(*mixSlider);

    duplicateButton.setTooltip("Duplicate this effect, with its settings");
    removeButton.setTooltip("Remove this effect");
    duplicateButton.onClick = [this] { rack.racks.duplicate(rack.rackId, instance); };
    removeButton.onClick = [this]
    {
        // Removing destroys this strip while its own button is handling the click: defer it.
        juce::Component::SafePointer<FxRackPanel> panel(&rack);
        const auto id = instance;
        juce::MessageManager::callAsync(
            [panel, id]
            {
                if (panel != nullptr)
                    panel->racks.remove(panel->rackId, id);
            });
    };
    addAndMakeVisible(duplicateButton);
    addAndMakeVisible(removeButton);

    if (const auto* extra = decorationFor(descriptor.id); extra != nullptr && extra->make)
    {
        decoration = extra->make(state, juce::String(instanceId));
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
            state, juce::String(core::ParameterIds::effect(instanceId, spec.idSuffix)),
            withoutGroup(specName, groupName));
        if (!control->isValid())
            continue;
        addAndMakeVisible(*control);

        // Shown only for some values of another parameter of the same effect.
        if (!spec.shownWhenSuffix.empty())
            if (auto* other = state.getParameter(
                    juce::String(core::ParameterIds::effect(instanceId, spec.shownWhenSuffix))))
            {
                auto* shown = control.get();
                const unsigned mask = spec.shownWhenMask;
                auto attachment = std::make_unique<juce::ParameterAttachment>(
                    *other,
                    [this, shown, mask](float value)
                    {
                        const int entry = juce::jlimit(0, 31, juce::roundToInt(value));
                        const bool show = ((mask >> entry) & 1u) != 0;
                        if (shown->isVisible() != show)
                        {
                            shown->setVisible(show);
                            if (ready)
                                resized();
                        }
                    });
                attachment->sendInitialUpdate();
                conditions.push_back(std::move(attachment));
            }

        (control->kind() == ParameterControl::Kind::knob ? found->knobs : found->switches)
            .push_back(std::move(control));
    }

    // The height it needs: the handle, the picture and either one row (an effect without sub-groups) or
    // a title, a row of switches and a row of knobs.
    const bool grouped = groups.size() > 1 || (!groups.empty() && groups.front().name.isNotEmpty());
    height = kHandleHeight + decorationHeight + 2 * kBodyInset;
    height +=
        grouped ? kGroupTitleHeight + ParameterControl::kSwitchHeight + kGroupKnobRowHeight : kKnobRowHeight;
    ready = true;
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
    handle.removeFromRight(4);
    removeButton.setBounds(handle.removeFromRight(kButtonWidth).reduced(0, 2));
    duplicateButton.setBounds(handle.removeFromRight(kButtonWidth).reduced(0, 2));
    handle.removeFromRight(4);
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
            const auto switches = shown(group.switches);
            const int numSwitches = static_cast<int>(switches.size());
            for (int s = 0; s < numSwitches; ++s)
            {
                const bool isToggle =
                    switches[static_cast<size_t>(s)]->kind() == ParameterControl::Kind::toggle;
                // A switch needs only its tick and "On"; the menus share the rest.
                const int width = isToggle ? juce::jmin(46, switchRow.getWidth() / 3)
                                           : switchRow.getWidth() / (numSwitches - s);
                switches[static_cast<size_t>(s)]->setBounds(switchRow.removeFromLeft(width));
            }
            const auto knobs = shown(group.knobs);
            const int numKnobs = static_cast<int>(knobs.size());
            for (int k = 0; k < numKnobs; ++k)
                knobs[static_cast<size_t>(k)]->setBounds(
                    column.removeFromLeft(column.getWidth() / (numKnobs - k)));
        }
        else
        {
            // One row: menus and switches first (Type before Drive), then the knobs. A menu gets more width
            // than a knob, a switch a little less.
            std::vector<ParameterControl*> all = shown(group.switches);
            for (auto* control : shown(group.knobs))
                all.push_back(control);
            if (all.empty())
                continue;
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
        rack.dragSlot(instance, e);
}

void FxRackPanel::Slot::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void FxRackPanel::Content::paint(juce::Graphics& g)
{
    if (empty)
    {
        g.setColour(style::kMutedText);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("Empty: the sound passes through", getLocalBounds().removeFromTop(kEmptyNoteHeight),
                   juce::Justification::centred);
    }
}

FxRackPanel::FxRackPanel(juce::AudioProcessorValueTreeState& state, core::FxRackState& rackState,
                         dsp::RackId rack, const dsp::EffectRegistry& effectRegistry)
    : parameters(state), racks(rackState), rackId(rack), registry(effectRegistry)
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(kScrollBarWidth);
    addAndMakeVisible(viewport);

    addButton.setTooltip("Add an effect at the end of this rack");
    addButton.onClick = [this] { showAddMenu(); };
    content.addAndMakeVisible(addButton);

    rebuildSlots();
    racks.addChangeListener(this);
}

FxRackPanel::~FxRackPanel()
{
    racks.removeChangeListener(this);
}

int FxRackPanel::preferredHeight() const
{
    int total = 0;
    for (const auto& slot : slots)
        total += slot->preferredHeight() + kSlotGap;
    return total + (slots.empty() ? kEmptyNoteHeight + kSlotGap : 0) + kAddButtonHeight;
}

void FxRackPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    rebuildSlots();
}

void FxRackPanel::resized()
{
    viewport.setBounds(getLocalBounds());
    layoutSlots();
}

void FxRackPanel::rebuildSlots()
{
    const auto& wanted = racks.ids(rackId);

    // Strips of effects that left the rack go; strips of effects that joined are built. A strip that
    // stays is kept, so a drag in progress and the controls' state survive.
    std::vector<std::unique_ptr<Slot>> kept;
    for (auto& slot : slots)
        if (std::find(wanted.begin(), wanted.end(), slot->id()) != wanted.end())
            kept.push_back(std::move(slot));
    slots = std::move(kept);

    for (const auto& id : wanted)
    {
        const bool present =
            std::any_of(slots.begin(), slots.end(), [&](const auto& s) { return s->id() == id; });
        if (!present)
        {
            slots.push_back(std::make_unique<Slot>(*this, registry, id, parameters));
            content.addAndMakeVisible(*slots.back());
        }
    }

    content.empty = slots.empty();
    layoutSlots();
}

std::vector<FxRackPanel::Slot*> FxRackPanel::slotsInOrder() const
{
    std::vector<Slot*> result;
    for (const auto& id : racks.ids(rackId))
        for (const auto& slot : slots)
            if (slot->id() == id)
                result.push_back(slot.get());
    return result;
}

void FxRackPanel::layoutSlots()
{
    const bool scrolls = preferredHeight() > getHeight();
    const int width = juce::jmax(0, getWidth() - (scrolls ? kScrollBarWidth : 0));

    int y = 0;
    for (auto* slot : slotsInOrder())
    {
        slot->setBounds(0, y, width, slot->preferredHeight());
        slot->setCanDuplicate(racks.canDuplicate(rackId, slot->id()));
        y += slot->preferredHeight() + kSlotGap;
    }
    if (slots.empty())
        y += kEmptyNoteHeight + kSlotGap;

    addButton.setBounds(0, y, width, kAddButtonHeight);
    addButton.setEnabled(static_cast<int>(slots.size()) < dsp::kMaxRackEffects);
    content.setSize(width, y + kAddButtonHeight);
    content.repaint();
}

void FxRackPanel::showAddMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < registry.count(); ++i)
    {
        const juce::String name(std::string(registry.descriptor(i)->displayName));
        menu.addItem(i + 1, name, racks.freeCopies(i) > 0);
    }

    juce::Component::SafePointer<FxRackPanel> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton),
                       [safe](int chosen)
                       {
                           if (safe != nullptr && chosen > 0)
                               safe->racks.add(safe->rackId, chosen - 1);
                       });
}

void FxRackPanel::dragSlot(const std::string& instanceId, const juce::MouseEvent& e)
{
    const auto ordered = slotsInOrder();
    if (ordered.size() < 2)
        return;

    std::vector<double> sizes;
    int current = 0;
    for (size_t i = 0; i < ordered.size(); ++i)
    {
        sizes.push_back(ordered[i]->preferredHeight() + kSlotGap);
        if (ordered[i]->id() == instanceId)
            current = static_cast<int>(i);
    }

    const int target = core::FxRackState::positionIn(sizes, e.getEventRelativeTo(&content).position.y);
    if (target != current)
        racks.move(rackId, current, target);
}

} // namespace fcm::ui

// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/SectionPanel.h"

#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
constexpr int kInset = 8;
constexpr int kMaxKnobRowHeight = 118;
} // namespace

void SectionPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(style::kPanel);
    g.fillRoundedRectangle(bounds, style::kCornerSize);
    g.setColour(style::kPanelBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), style::kCornerSize, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour(style::kText);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText(title, getLocalBounds().removeFromTop(style::kTitleHeight).reduced(kInset, 0),
                   juce::Justification::centredLeft);
    }
}

juce::Rectangle<int> SectionPanel::contentArea() const
{
    auto area = getLocalBounds().reduced(kInset, kInset / 2);
    if (title.isNotEmpty())
        area.removeFromTop(style::kTitleHeight - kInset / 2);
    return area;
}

ControlPanel::ControlPanel(juce::AudioProcessorValueTreeState& state, juce::String title)
    : SectionPanel(std::move(title)), parameters(state)
{
}

int ControlPanel::addRow(const std::vector<Item>& items)
{
    Row row;
    for (const auto& item : items)
    {
        auto control = std::make_unique<ParameterControl>(parameters, item.parameterId, item.label);
        if (!control->isValid())
            continue;
        row.hasKnob = row.hasKnob || control->kind() == ParameterControl::Kind::knob;
        addAndMakeVisible(*control);
        row.cells.push_back({control.get(), 1.0f});
        controls.push_back(std::move(control));
    }
    rows.push_back(std::move(row));
    return static_cast<int>(rows.size()) - 1;
}

void ControlPanel::addCustom(int row, juce::Component& component, float weight)
{
    jassert(row >= 0 && row < static_cast<int>(rows.size()));
    if (row < 0 || row >= static_cast<int>(rows.size()))
        return;
    addAndMakeVisible(component);
    rows[static_cast<size_t>(row)].cells.push_back({&component, weight});
}

void ControlPanel::resized()
{
    auto area = contentArea();
    if (rows.empty())
        return;

    // Rows without a knob keep one height; the rows with a knob share what is left.
    int fixed = 0;
    int knobRows = 0;
    for (const auto& row : rows)
    {
        if (row.hasKnob)
            ++knobRows;
        else
            fixed += ParameterControl::kSwitchHeight;
    }
    const int knobHeight =
        knobRows > 0 ? juce::jmin(kMaxKnobRowHeight, juce::jmax(0, area.getHeight() - fixed) / knobRows) : 0;

    for (auto& row : rows)
    {
        auto rowArea = area.removeFromTop(row.hasKnob ? knobHeight : ParameterControl::kSwitchHeight);
        float totalWeight = 0.0f;
        for (const auto& cell : row.cells)
            totalWeight += cell.weight;
        if (totalWeight <= 0.0f)
            continue;

        const int width = rowArea.getWidth();
        int used = 0;
        float weightSoFar = 0.0f;
        for (const auto& cell : row.cells)
        {
            weightSoFar += cell.weight;
            const int right = juce::roundToInt(static_cast<float>(width) * weightSoFar / totalWeight);
            auto bounds = juce::Rectangle<int>(rowArea.getX() + used, rowArea.getY(), right - used,
                                               rowArea.getHeight());
            // A custom component (a button) keeps a normal height in the middle of the row.
            if (dynamic_cast<ParameterControl*>(cell.component) == nullptr)
                bounds = bounds.withSizeKeepingCentre(bounds.getWidth() - 6, 26);
            cell.component->setBounds(bounds);
            used = right;
        }
    }
}

PlaceholderPanel::PlaceholderPanel(juce::String panelTitle, juce::String text)
    : SectionPanel(std::move(panelTitle)), note(std::move(text))
{
}

void PlaceholderPanel::paint(juce::Graphics& g)
{
    SectionPanel::paint(g);
    g.setColour(style::kMutedText.withAlpha(0.6f));
    g.setFont(juce::FontOptions(13.0f));
    g.drawFittedText(note, contentArea(), juce::Justification::centred, 4);
}

} // namespace fcm::ui

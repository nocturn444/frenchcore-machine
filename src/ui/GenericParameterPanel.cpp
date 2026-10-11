// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/GenericParameterPanel.h"

namespace fcm::ui
{

namespace
{
constexpr int kMargin = 12;
constexpr int kColumnGap = 16;
constexpr int kRowGap = 4;
constexpr int kHeadingHeight = 28;
constexpr int kMaxRowHeight = 36;
constexpr int kValueBoxWidth = 80;
} // namespace

juce::String GenericParameterPanel::headingFor(const juce::String& prefix)
{
    if (prefix == "global")
        return "Global";
    if (prefix == "tock")
        return "Tock";
    if (prefix.startsWith("osc"))
        return "Oscillator " + prefix.fromFirstOccurrenceOf("osc", false, false);
    // An effect's controls: its ID with a capital letter.
    return prefix.substring(0, 1).toUpperCase() + prefix.substring(1);
}

GenericParameterPanel::GenericParameterPanel(juce::AudioProcessorValueTreeState& state,
                                             const std::vector<juce::String>& excludedGroups,
                                             const juce::String& onlyGroup, const juce::String& title)
{
    // Groups appear in the order of their first parameter, which is the layout order.
    std::vector<juce::String> prefixes;

    for (auto* parameter : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
            continue;

        const auto prefix = ranged->paramID.upToFirstOccurrenceOf(".", false, false);
        if (onlyGroup.isNotEmpty() && prefix != onlyGroup)
            continue;
        if (std::find(excludedGroups.begin(), excludedGroups.end(), prefix) != excludedGroups.end())
            continue;

        auto found = std::find(prefixes.begin(), prefixes.end(), prefix);
        if (found == prefixes.end())
        {
            prefixes.push_back(prefix);
            groups.emplace_back();
            groups.back().heading = std::make_unique<juce::Label>(
                juce::String(), title.isNotEmpty() ? title : headingFor(prefix));
            groups.back().heading->setFont(juce::FontOptions(18.0f, juce::Font::bold));
            addAndMakeVisible(*groups.back().heading);
            found = prefixes.end() - 1;
        }

        addRow(groups[static_cast<size_t>(found - prefixes.begin())], *ranged);
    }
}

void GenericParameterPanel::addRow(Group& group, juce::RangedAudioParameter& parameter)
{
    Row row;
    row.label = std::make_unique<juce::Label>(juce::String(), parameter.getName(64));
    row.label->setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(*row.label);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(&parameter))
    {
        row.combo = std::make_unique<juce::ComboBox>();
        row.combo->setTitle(parameter.getName(64));
        row.combo->addItemList(choice->choices, 1);
        row.comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(parameter, *row.combo);
        addAndMakeVisible(*row.combo);
    }
    else if (parameter.isBoolean())
    {
        row.toggle = std::make_unique<juce::ToggleButton>();
        row.toggle->setTitle(parameter.getName(64));
        row.buttonAttachment = std::make_unique<juce::ButtonParameterAttachment>(parameter, *row.toggle);
        addAndMakeVisible(*row.toggle);
    }
    else
    {
        row.slider =
            std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        row.slider->setTitle(parameter.getName(64));
        row.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, kValueBoxWidth, 24);
        if (parameter.getLabel().isNotEmpty())
            row.slider->setTextValueSuffix(" " + parameter.getLabel());
        row.sliderAttachment = std::make_unique<juce::SliderParameterAttachment>(parameter, *row.slider);
        addAndMakeVisible(*row.slider);
    }

    group.rows.push_back(std::move(row));
}

void GenericParameterPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15161a));
}

void GenericParameterPanel::resized()
{
    if (groups.empty())
        return;

    auto area = getLocalBounds().reduced(kMargin);
    const int numGroups = static_cast<int>(groups.size());
    const int columnWidth = (area.getWidth() - kColumnGap * (numGroups - 1)) / numGroups;

    size_t mostRows = 0;
    for (const auto& group : groups)
        mostRows = std::max(mostRows, group.rows.size());
    const int rowHeight = std::min(kMaxRowHeight, (area.getHeight() - kHeadingHeight) /
                                                      std::max(1, static_cast<int>(mostRows)));

    for (int g = 0; g < numGroups; ++g)
    {
        auto& group = groups[static_cast<size_t>(g)];
        auto column = area.withX(area.getX() + g * (columnWidth + kColumnGap)).withWidth(columnWidth);

        group.heading->setBounds(column.removeFromTop(kHeadingHeight));

        for (auto& row : group.rows)
        {
            auto rowArea = column.removeFromTop(rowHeight).reduced(0, kRowGap / 2);
            row.label->setBounds(rowArea.removeFromLeft(rowArea.getWidth() * 2 / 5));
            row.control().setBounds(rowArea);
        }
    }
}

} // namespace fcm::ui

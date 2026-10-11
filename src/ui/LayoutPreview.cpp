// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/LayoutPreview.h"

namespace fcm::ui
{

// Grid of 4 columns x 5 rows. Row 0 is the preset bar; see docs/UI_LAYOUT.md.
const std::array<LayoutPreview::Area, 11> LayoutPreview::kAreas{{
    {"Preset bar: SAVE / SAVE AS / SELECT / INIT", 0, 0, 4, 1, false},
    {"Oscillator 1", 0, 1, 1, 1, false},
    {"Oscillator 2", 1, 1, 1, 1, false},
    {"FX rack", 2, 1, 2, 2, false},
    {"Amp envelope", 0, 2, 1, 1, false},
    {"Pitch envelope", 1, 2, 1, 1, false},
    {"LFO 1/2 (after MVP)", 0, 3, 1, 2, true},
    {"Tock", 1, 3, 1, 1, false},
    {"Punch / Tail", 2, 3, 1, 1, false},
    {"Global", 3, 3, 1, 2, false},
    {"Other (reserved)", 1, 4, 2, 1, true},
}};

void LayoutPreview::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15161a));

    constexpr int kColumns = 4;
    constexpr float kHeaderRows = 0.5f; // the preset bar is half a row tall
    constexpr float kRows = 4.0f + kHeaderRows;
    const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    const float columnWidth = bounds.getWidth() / kColumns;
    const float rowHeight = bounds.getHeight() / kRows;

    auto rowTop = [&](int row) { return row == 0 ? 0.0f : (kHeaderRows + float(row - 1)) * rowHeight; };
    auto rowBottom = [&](int row) { return row == 0 ? kHeaderRows * rowHeight : rowTop(row) + rowHeight; };

    g.setFont(juce::FontOptions(14.0f));
    for (const auto& area : kAreas)
    {
        const float x = bounds.getX() + float(area.column) * columnWidth;
        const float y = bounds.getY() + rowTop(area.row);
        const float w = float(area.columnSpan) * columnWidth;
        const float h = rowBottom(area.row + area.rowSpan - 1) - rowTop(area.row);
        const auto rect = juce::Rectangle<float>(x, y, w, h).reduced(4.0f);

        g.setColour(area.reserved ? juce::Colour(0xff24252b) : juce::Colour(0xff2c2f38));
        g.fillRoundedRectangle(rect, 4.0f);
        g.setColour(area.reserved ? juce::Colours::grey : juce::Colours::white);
        g.drawText(area.name, rect.reduced(8.0f), juce::Justification::topLeft);
    }
}

} // namespace fcm::ui

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace fcm::ui
{

// Placeholder editor content for Phase 1: draws the agreed area grid from docs/UI_LAYOUT.md so the
// layout can be checked at every window size. Each area is replaced by its real panel as the
// matching feature lands.
class LayoutPreview : public juce::Component
{
  public:
    void paint(juce::Graphics& g) override;

  private:
    struct Area
    {
        const char* name;
        int column, row, columnSpan, rowSpan;
        bool reserved;
    };

    static const std::array<Area, 11> kAreas;
};

} // namespace fcm::ui

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace fcm::ui::style
{

// The colours and sizes the panels share. The visual design pass changes them here, and later in a
// LookAndFeel (docs/UI_LAYOUT.md); no panel hard-codes its own.
inline const juce::Colour kWindow{0xff111216};
inline const juce::Colour kPanel{0xff1b1d23};
inline const juce::Colour kPanelBorder{0xff2e313a};
inline const juce::Colour kHeader{0xff262a33};
inline const juce::Colour kText{0xffe4e7ee};
inline const juce::Colour kMutedText{0xff8f95a3};
inline const juce::Colour kAccent{0xff3fa0d0};
inline const juce::Colour kWarning{0xffe0a050};

inline constexpr float kCornerSize = 5.0f;
inline constexpr int kGap = 6;
inline constexpr int kTitleHeight = 24;

} // namespace fcm::ui::style

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/EffectDecorations.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fcm::ui
{

// The picture of an effect's drawn curve in its strip (docs/specs/fx-volumelfo.md section 2): the
// cycle with a faint grid of quarters. Clicking it opens the larger CurveEditor over the rack.
class CurveThumbnail : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::ChangeListener
{
  public:
    explicit CurveThumbnail(const DecorationContext& context);
    ~CurveThumbnail() override;

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void mouseUp(const juce::MouseEvent& event) override;

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    DecorationContext context;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveThumbnail)
};

} // namespace fcm::ui

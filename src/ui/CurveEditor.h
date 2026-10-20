// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/EffectCurveState.h"
#include "dsp/fx/EffectRegistry.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fcm::ui
{

// The larger editor of an effect's drawn curve (docs/specs/fx-volumelfo.md section 2), shown over the
// rack by the Effects box. A Shape menu with the effect's ready-made shapes, a Grid menu that dragged
// points snap to, and the same gestures as the envelope editor:
//
//   drag a point          move it (it cannot pass its neighbours; the ends only move up and down)
//   double-click          add a point / delete an extra point
//   drag a segment        bend it
//
// It edits through core::EffectCurveState only, with the functions of core::curve_edit.
class CurveEditor : public juce::Component, private juce::ChangeListener
{
  public:
    CurveEditor(core::EffectCurveState& curves, const dsp::EffectRegistry& registry, int instance);
    ~CurveEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    // The steps of the Grid menu, in cycles (0: off), in the order of the menu.
    static float gridStep(int menuIndex) noexcept;

  private:
    static constexpr int kToolbarHeight = 26;

    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen(float x, float y) const;
    float xAt(float screenX) const;
    float yAt(float screenY) const;
    float snap() const;

    int pointAt(juce::Point<float> position) const;
    int segmentAt(juce::Point<float> position) const;
    void edit(const std::function<void(dsp::EffectCurve&)>& change);

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    core::EffectCurveState& curves;
    const dsp::EffectRegistry& registry;
    int instance;

    juce::Label shapeLabel{{}, "Shape"};
    juce::Label gridLabel{{}, "Grid"};
    juce::ComboBox shapes;
    juce::ComboBox grid;

    enum class Drag
    {
        none,
        point,
        segment
    };
    Drag drag = Drag::none;
    int dragIndex = -1;
    float dragStartBend = 0.0f;
    float dragStartY = 0.0f;
    int hoverPoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveEditor)
};

} // namespace fcm::ui

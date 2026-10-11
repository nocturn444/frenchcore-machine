// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/EnvelopeState.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

namespace fcm::ui
{

// The envelope display (docs/specs/envelope-breakpoint.md §1): the line, its points, and the
// mouse editing. It edits through core::EnvelopeState only, so the audio thread gets every change.
//
//   drag a point          move it in time and level
//   double-click          add a point on the line / delete an extra point
//   drag a segment        bend it (up makes the line bulge up)
class EnvelopeEditor : public juce::Component, private juce::ChangeListener, private juce::Timer
{
  public:
    // `tempo` gives the current tempo, for the tempo-synced mode. `markerMs`, when set, gives the
    // time in envelope milliseconds of a dashed vertical marker (the Punch / Tail split on the amp
    // envelope), or nothing to hide it.
    EnvelopeEditor(core::EnvelopeState& state, std::function<float()> tempo,
                   std::function<std::optional<float>()> markerMs = {});
    ~EnvelopeEditor() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    // The visible time range: the smallest of a few steps that fits the whole shape.
    float viewMs() const;

  private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen(float timeMs, float value) const;
    // The time under a horizontal position; in sync mode snapped to 1/32 notes.
    float timeAt(float x) const;
    // Edits the shape. A tempo-synced shape is first rewritten for the current tempo, so the grid
    // and the snapping match what is heard.
    void editShape(const std::function<void(dsp::BreakpointEnvelope&)>& change);
    float valueAt(float y) const;

    // Index of the point under the position, or -1.
    int pointAt(juce::Point<float> position) const;
    // Index of the segment (its first point) whose line passes near the position, or -1.
    int segmentAt(juce::Point<float> position) const;

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }
    void timerCallback() override;

    core::EnvelopeState& state;
    std::function<float()> tempo;
    std::function<std::optional<float>()> markerMs;
    std::optional<float> shownMarker;

    enum class Drag
    {
        none,
        point,
        segment
    };
    Drag drag = Drag::none;
    int dragIndex = -1;
    float dragStartCurve = 0.0f;
    float dragStartY = 0.0f;
    int hoverPoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeEditor)
};

} // namespace fcm::ui

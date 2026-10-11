// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EnvelopeEditor.h"

#include <cmath>

namespace fcm::ui
{

namespace
{
constexpr float kPadding = 10.0f;
constexpr float kMainRadius = 6.0f;
constexpr float kExtraRadius = 4.5f;
constexpr float kHitRadius = 9.0f;
constexpr float kSegmentHitDistance = 7.0f;
constexpr float kPixelsForFullCurve = 80.0f;
constexpr float kRepaintHz = 20.0f;

// The display fits the shape in the first of these that holds it, so it does not rescale while a
// point is dragged, only when a boundary is crossed.
constexpr float kViewSteps[] = {250.0f, 500.0f, 1000.0f, 2000.0f};

const juce::Colour kBackground(0xff15161a);
const juce::Colour kGrid(0xff2a2d36);
const juce::Colour kLine(0xff3fa9d6);
const juce::Colour kFill(0x223fa9d6);
const juce::Colour kMainPoint(0xfff2b134);
const juce::Colour kExtraPoint(0xffe8ecf2);
const juce::Colour kMarker(0xffe0556b);
} // namespace

EnvelopeEditor::EnvelopeEditor(core::EnvelopeState& envelopeState,
                               std::function<std::optional<float>()> marker)
    : state(envelopeState), markerMs(std::move(marker))
{
    state.addChangeListener(this);
    if (markerMs)
        startTimerHz(static_cast<int>(kRepaintHz));
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

EnvelopeEditor::~EnvelopeEditor()
{
    state.removeChangeListener(this);
}

float EnvelopeEditor::viewMs() const
{
    const float last = state.get().points().back().timeMs * 1.1f;
    for (float step : kViewSteps)
        if (last <= step)
            return step;
    return kViewSteps[std::size(kViewSteps) - 1];
}

juce::Rectangle<float> EnvelopeEditor::plotArea() const
{
    return getLocalBounds().toFloat().reduced(kPadding);
}

juce::Point<float> EnvelopeEditor::toScreen(float timeMs, float value) const
{
    const auto area = plotArea();
    return {area.getX() + timeMs / viewMs() * area.getWidth(), area.getBottom() - value * area.getHeight()};
}

float EnvelopeEditor::timeAt(float x) const
{
    const auto area = plotArea();
    return (x - area.getX()) / area.getWidth() * viewMs();
}

float EnvelopeEditor::valueAt(float y) const
{
    const auto area = plotArea();
    return (area.getBottom() - y) / area.getHeight();
}

int EnvelopeEditor::pointAt(juce::Point<float> position) const
{
    const auto& points = state.get().points();
    int best = -1;
    float bestDistance = kHitRadius;

    // Later points win on a tie, so the second of two points at the same time can be grabbed.
    for (int i = 0; i < static_cast<int>(points.size()); ++i)
    {
        const auto& point = points[static_cast<size_t>(i)];
        const float distance = toScreen(point.timeMs, point.value).getDistanceFrom(position);
        if (distance <= bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

int EnvelopeEditor::segmentAt(juce::Point<float> position) const
{
    const auto shape = state.get().toShape();
    const auto area = plotArea();
    if (position.x < area.getX() || position.x > area.getRight())
        return -1;

    const float time = timeAt(position.x);
    if (time > shape->lastTimeMs())
        return -1;

    int segment = 0;
    const float lineY = toScreen(0.0f, shape->valueAt(time, segment)).y;
    if (std::abs(lineY - position.y) > kSegmentHitDistance)
        return -1;

    // `segment` is the last point at or before the time: the segment that starts there.
    return segment < shape->count - 1 ? segment : -1;
}

void EnvelopeEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBackground);
    const auto area = plotArea();

    // Time grid, in steps that suit the visible range.
    const float range = viewMs();
    const float gridStep =
        range <= 250.0f ? 50.0f : (range <= 500.0f ? 100.0f : (range <= 1000.0f ? 250.0f : 500.0f));
    g.setFont(juce::FontOptions(11.0f));
    for (float t = 0.0f; t <= range; t += gridStep)
    {
        const float x = toScreen(t, 0.0f).x;
        g.setColour(kGrid);
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
        g.setColour(juce::Colours::grey);
        g.drawText(juce::String(juce::roundToInt(t)) + " ms",
                   juce::Rectangle<float>(x + 2.0f, area.getBottom() - 14.0f, 56.0f, 12.0f),
                   juce::Justification::centredLeft);
    }
    for (float level : {0.25f, 0.5f, 0.75f, 1.0f})
    {
        g.setColour(kGrid);
        g.drawHorizontalLine(juce::roundToInt(toScreen(0.0f, level).y), area.getX(), area.getRight());
    }

    // The line, sampled with the same function the audio thread uses.
    const auto shape = state.get().toShape();
    juce::Path line;
    int segment = 0;
    const int width = juce::jmax(2, static_cast<int>(area.getWidth()));
    for (int px = 0; px <= width; ++px)
    {
        const float time = static_cast<float>(px) / static_cast<float>(width) * range;
        const auto p = toScreen(time, shape->valueAt(time, segment));
        if (px == 0)
            line.startNewSubPath(p);
        else
            line.lineTo(p);
    }

    juce::Path fill(line);
    fill.lineTo(toScreen(range, 0.0f));
    fill.lineTo(toScreen(0.0f, 0.0f));
    fill.closeSubPath();
    g.setColour(kFill);
    g.fillPath(fill);
    g.setColour(kLine);
    g.strokePath(line, juce::PathStrokeType(2.0f));

    // The split marker.
    if (shownMarker.has_value() && *shownMarker >= 0.0f && *shownMarker <= range)
    {
        const float x = toScreen(*shownMarker, 0.0f).x;
        const float dashes[] = {4.0f, 4.0f};
        g.setColour(kMarker);
        g.drawDashedLine({x, area.getY(), x, area.getBottom()}, dashes, 2, 1.5f);
    }

    // The points: main ones are larger and gold, extra ones are small and white.
    const auto& points = state.get().points();
    for (int i = 0; i < static_cast<int>(points.size()); ++i)
    {
        const auto& point = points[static_cast<size_t>(i)];
        const auto centre = toScreen(point.timeMs, point.value);
        const float radius = point.isMain ? kMainRadius : kExtraRadius;
        const bool active = i == hoverPoint || (drag == Drag::point && i == dragIndex);

        g.setColour((point.isMain ? kMainPoint : kExtraPoint).withAlpha(active ? 1.0f : 0.85f));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        if (active)
        {
            g.setColour(juce::Colours::white);
            g.drawEllipse(centre.x - radius - 2.0f, centre.y - radius - 2.0f, (radius + 2.0f) * 2.0f,
                          (radius + 2.0f) * 2.0f, 1.5f);
        }
    }
}

void EnvelopeEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto position = event.position;
    drag = Drag::none;

    if (const int index = pointAt(position); index >= 0)
    {
        drag = Drag::point;
        dragIndex = index;
    }
    else if (const int segment = segmentAt(position); segment >= 0)
    {
        const auto& points = state.get().points();
        // A flat segment has no visible curvature to bend.
        if (!juce::exactlyEqual(points[static_cast<size_t>(segment)].value,
                                points[static_cast<size_t>(segment) + 1].value))
        {
            drag = Drag::segment;
            dragIndex = segment;
            dragStartCurve = points[static_cast<size_t>(segment)].curve;
            dragStartY = position.y;
        }
    }
    repaint();
}

void EnvelopeEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (drag == Drag::point)
    {
        const auto position = event.position;
        state.edit([&](dsp::BreakpointEnvelope& envelope)
                   { envelope.movePoint(dragIndex, timeAt(position.x), valueAt(position.y)); });
    }
    else if (drag == Drag::segment)
    {
        const auto& points = state.get().points();
        const bool rising =
            points[static_cast<size_t>(dragIndex) + 1].value > points[static_cast<size_t>(dragIndex)].value;
        // Dragging up makes the line bulge up: that is a more negative curve on a rising segment
        // (fast first) and a more positive one on a falling segment (slow first).
        const float sign = rising ? -1.0f : 1.0f;
        const float curve = dragStartCurve + sign * (dragStartY - event.position.y) / kPixelsForFullCurve;
        state.edit([&](dsp::BreakpointEnvelope& envelope) { envelope.setCurve(dragIndex, curve); });
    }
}

void EnvelopeEditor::mouseUp(const juce::MouseEvent&)
{
    drag = Drag::none;
    dragIndex = -1;
    repaint();
}

void EnvelopeEditor::mouseMove(const juce::MouseEvent& event)
{
    const int point = pointAt(event.position);
    const int segment = point < 0 ? segmentAt(event.position) : -1;

    setMouseCursor(point >= 0 ? juce::MouseCursor::DraggingHandCursor
                              : (segment >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                              : juce::MouseCursor::NormalCursor));
    if (point != hoverPoint)
    {
        hoverPoint = point;
        repaint();
    }
}

void EnvelopeEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    const auto position = event.position;

    if (const int index = pointAt(position); index >= 0)
    {
        // Only extra points can be deleted; the knobs need the main ones.
        state.edit([&](dsp::BreakpointEnvelope& envelope) { envelope.removePoint(index); });
        hoverPoint = -1;
        return;
    }

    if (plotArea().expanded(2.0f).contains(position))
        state.edit([&](dsp::BreakpointEnvelope& envelope)
                   { envelope.addPoint(timeAt(position.x), valueAt(position.y)); });
}

void EnvelopeEditor::timerCallback()
{
    const auto marker = markerMs();
    const bool changed = marker.has_value() != shownMarker.has_value() ||
                         (marker.has_value() && std::abs(*marker - *shownMarker) > 1.0e-3f);
    if (changed)
    {
        shownMarker = marker;
        repaint();
    }
}

} // namespace fcm::ui

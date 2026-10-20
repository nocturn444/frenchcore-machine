// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/CurveEditor.h"

#include "core/EffectCurveEdit.h"
#include "ui/Style.h"

#include <cmath>

namespace fcm::ui
{

namespace
{
constexpr float kPadding = 14.0f;
constexpr float kEndRadius = 6.0f;
constexpr float kPointRadius = 4.5f;
constexpr float kHitRadius = 9.0f;
constexpr float kSegmentHitDistance = 7.0f;
constexpr float kPixelsForFullBend = 80.0f;

const juce::Colour kBackground(0xff15161a);
const juce::Colour kGrid(0xff2a2d36);
const juce::Colour kStrongGrid(0xff3a3e4a);
const juce::Colour kLine(0xff3fa9d6);
const juce::Colour kFill(0x223fa9d6);
const juce::Colour kEndPoint(0xfff2b134);
const juce::Colour kExtraPoint(0xffe8ecf2);

constexpr const char* kGridNames[] = {"Off", "1/4", "1/8", "1/16", "1/32"};
constexpr int kDefaultGrid = 3; // 1/16
} // namespace

float CurveEditor::gridStep(int menuIndex) noexcept
{
    switch (menuIndex)
    {
    case 1:
        return 1.0f / 4.0f;
    case 2:
        return 1.0f / 8.0f;
    case 3:
        return 1.0f / 16.0f;
    case 4:
        return 1.0f / 32.0f;
    default:
        return 0.0f;
    }
}

CurveEditor::CurveEditor(core::EffectCurveState& state, const dsp::EffectRegistry& effects, int instanceIndex)
    : curves(state), registry(effects), instance(instanceIndex)
{
    curves.addChangeListener(this);

    for (auto* label : {&shapeLabel, &gridLabel})
    {
        label->setColour(juce::Label::textColourId, style::kMutedText);
        label->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*label);
    }

    // Choosing a ready-made shape replaces the drawing; the menu then goes back to its prompt, since
    // further editing makes the shape something else (like the envelopes' "Load shape...").
    shapes.setTextWhenNothingSelected("Load shape...");
    const auto presets = registry.curvePresets(dsp::EffectRegistry::effectOf(instance));
    for (size_t i = 0; i < presets.size(); ++i)
        shapes.addItem(presets[i].name, static_cast<int>(i) + 1);
    shapes.onChange = [this]
    {
        const int id = shapes.getSelectedId();
        if (id <= 0)
            return;
        const auto list = registry.curvePresets(dsp::EffectRegistry::effectOf(instance));
        if (static_cast<size_t>(id) <= list.size())
            curves.set(instance, list[static_cast<size_t>(id) - 1].curve);
        shapes.setSelectedId(0, juce::dontSendNotification);
    };
    addAndMakeVisible(shapes);

    for (int i = 0; i < 5; ++i)
        grid.addItem(kGridNames[i], i + 1);
    grid.setSelectedId(kDefaultGrid + 1, juce::dontSendNotification);
    grid.onChange = [this] { repaint(); };
    addAndMakeVisible(grid);
}

CurveEditor::~CurveEditor()
{
    curves.removeChangeListener(this);
}

float CurveEditor::snap() const
{
    return gridStep(grid.getSelectedId() - 1);
}

juce::Rectangle<float> CurveEditor::plotArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromTop(static_cast<float>(kToolbarHeight));
    return area.reduced(kPadding);
}

juce::Point<float> CurveEditor::toScreen(float x, float y) const
{
    const auto area = plotArea();
    return {area.getX() + x * area.getWidth(), area.getBottom() - y * area.getHeight()};
}

float CurveEditor::xAt(float screenX) const
{
    const auto area = plotArea();
    return (screenX - area.getX()) / area.getWidth();
}

float CurveEditor::yAt(float screenY) const
{
    const auto area = plotArea();
    return (area.getBottom() - screenY) / area.getHeight();
}

void CurveEditor::resized()
{
    auto bar = getLocalBounds().removeFromTop(kToolbarHeight).reduced(2, 2);
    gridLabel.setBounds(bar.removeFromRight(120).removeFromLeft(40));
    grid.setBounds(getLocalBounds().removeFromTop(kToolbarHeight).reduced(2, 2).removeFromRight(80));
    shapeLabel.setBounds(bar.removeFromLeft(50));
    shapes.setBounds(bar.removeFromLeft(160));
}

void CurveEditor::edit(const std::function<void(dsp::EffectCurve&)>& change)
{
    auto edited = curves.get(instance);
    change(edited);
    curves.set(instance, edited);
}

int CurveEditor::pointAt(juce::Point<float> position) const
{
    const auto& curve = curves.get(instance);
    int best = -1;
    float bestDistance = kHitRadius;

    // Later points win on a tie, so the second of two points at one position can be grabbed.
    for (int i = 0; i < curve.count; ++i)
    {
        const auto& point = curve.points[static_cast<size_t>(i)];
        const float distance = toScreen(point.x, point.y).getDistanceFrom(position);
        if (distance <= bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

int CurveEditor::segmentAt(juce::Point<float> position) const
{
    const auto& curve = curves.get(instance);
    const auto area = plotArea();
    if (curve.count < 2 || position.x < area.getX() || position.x > area.getRight())
        return -1;

    const float x = xAt(position.x);
    int segment = 0;
    const float lineY = toScreen(0.0f, curve.valueAt(x, segment)).y;
    if (std::abs(lineY - position.y) > kSegmentHitDistance)
        return -1;
    return segment < curve.count - 1 ? segment : -1;
}

void CurveEditor::paint(juce::Graphics& g)
{
    g.setColour(kBackground);
    g.fillRect(plotArea().expanded(kPadding - 2.0f));

    const auto& curve = curves.get(instance);
    if (curve.count < 2)
        return;

    const auto area = plotArea();

    // The grid the points snap to, and stronger lines at the quarters.
    g.setFont(juce::FontOptions(11.0f));
    if (const float step = snap(); step > 0.0f)
    {
        g.setColour(kGrid);
        for (float x = 0.0f; x <= 1.0001f; x += step)
            g.drawVerticalLine(juce::roundToInt(toScreen(x, 0.0f).x), area.getY(), area.getBottom());
    }
    for (int quarter = 0; quarter <= 4; ++quarter)
    {
        const float x = static_cast<float>(quarter) / 4.0f;
        g.setColour(kStrongGrid);
        g.drawVerticalLine(juce::roundToInt(toScreen(x, 0.0f).x), area.getY(), area.getBottom());
        g.setColour(juce::Colours::grey);
        g.drawText(quarter == 0 ? "0" : (quarter == 4 ? "1" : juce::String(quarter) + "/4"),
                   juce::Rectangle<float>(toScreen(x, 0.0f).x + 3.0f, area.getBottom() - 14.0f, 30.0f, 12.0f),
                   juce::Justification::centredLeft);
    }
    for (float level : {0.25f, 0.5f, 0.75f, 1.0f})
    {
        g.setColour(kGrid);
        g.drawHorizontalLine(juce::roundToInt(toScreen(0.0f, level).y), area.getX(), area.getRight());
    }

    // The line, sampled with the same function the audio thread uses.
    juce::Path line;
    int segment = 0;
    const int width = juce::jmax(2, static_cast<int>(area.getWidth()));
    for (int px = 0; px <= width; ++px)
    {
        const float x = static_cast<float>(px) / static_cast<float>(width);
        const auto p = toScreen(x, curve.valueAt(x, segment));
        if (px == 0)
            line.startNewSubPath(p);
        else
            line.lineTo(p);
    }
    juce::Path fill(line);
    fill.lineTo(toScreen(1.0f, 0.0f));
    fill.lineTo(toScreen(0.0f, 0.0f));
    fill.closeSubPath();
    g.setColour(kFill);
    g.fillPath(fill);
    g.setColour(kLine);
    g.strokePath(line, juce::PathStrokeType(2.0f));

    // The points: the two ends are larger and gold, the others small and white.
    for (int i = 0; i < curve.count; ++i)
    {
        const auto& point = curve.points[static_cast<size_t>(i)];
        const auto centre = toScreen(point.x, point.y);
        const bool end = i == 0 || i == curve.count - 1;
        const float radius = end ? kEndRadius : kPointRadius;
        const bool active = i == hoverPoint || (drag == Drag::point && i == dragIndex);

        g.setColour((end ? kEndPoint : kExtraPoint).withAlpha(active ? 1.0f : 0.85f));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        if (active)
        {
            g.setColour(juce::Colours::white);
            g.drawEllipse(centre.x - radius - 2.0f, centre.y - radius - 2.0f, (radius + 2.0f) * 2.0f,
                          (radius + 2.0f) * 2.0f, 1.5f);
        }
    }
}

void CurveEditor::mouseDown(const juce::MouseEvent& event)
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
        const auto& curve = curves.get(instance);
        // A flat segment has no visible bend to change.
        if (!juce::exactlyEqual(curve.points[static_cast<size_t>(segment)].y,
                                curve.points[static_cast<size_t>(segment) + 1].y))
        {
            drag = Drag::segment;
            dragIndex = segment;
            dragStartBend = curve.points[static_cast<size_t>(segment)].curve;
            dragStartY = position.y;
        }
    }
    repaint();
}

void CurveEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (drag == Drag::point)
    {
        const auto position = event.position;
        edit([&](dsp::EffectCurve& curve)
             { core::curve_edit::movePoint(curve, dragIndex, xAt(position.x), yAt(position.y), snap()); });
    }
    else if (drag == Drag::segment)
    {
        const auto& curve = curves.get(instance);
        const bool rising = curve.points[static_cast<size_t>(dragIndex) + 1].y >
                            curve.points[static_cast<size_t>(dragIndex)].y;
        // Dragging up makes the line bulge up: a more negative bend on a rising segment (fast first)
        // and a more positive one on a falling segment (slow first), as in the envelope editor.
        const float sign = rising ? -1.0f : 1.0f;
        const float bend = dragStartBend + sign * (dragStartY - event.position.y) / kPixelsForFullBend;
        edit([&](dsp::EffectCurve& edited) { core::curve_edit::setBend(edited, dragIndex, bend); });
    }
}

void CurveEditor::mouseUp(const juce::MouseEvent&)
{
    drag = Drag::none;
    dragIndex = -1;
    repaint();
}

void CurveEditor::mouseMove(const juce::MouseEvent& event)
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

void CurveEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    const auto position = event.position;

    if (const int index = pointAt(position); index >= 0)
    {
        edit([&](dsp::EffectCurve& curve) { core::curve_edit::removePoint(curve, index); });
        hoverPoint = -1;
        return;
    }

    if (plotArea().expanded(2.0f).contains(position))
        edit([&](dsp::EffectCurve& curve)
             { core::curve_edit::addPoint(curve, xAt(position.x), yAt(position.y), snap()); });
}

} // namespace fcm::ui

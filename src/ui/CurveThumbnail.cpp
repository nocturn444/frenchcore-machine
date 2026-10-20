// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/CurveThumbnail.h"

#include "dsp/fx/EffectRegistry.h"
#include "ui/CurveEditor.h"
#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
const juce::Colour kBackground(0xff15161a);
const juce::Colour kGrid(0xff2a2d36);
const juce::Colour kLine(0xff3fa9d6);
const juce::Colour kFill(0x223fa9d6);
constexpr float kPadding = 4.0f;
} // namespace

CurveThumbnail::CurveThumbnail(const DecorationContext& decorationContext) : context(decorationContext)
{
    if (context.environment.curves != nullptr)
        context.environment.curves->addChangeListener(this);
    setTooltip("The shape of the cycle. Click to edit it.");
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

CurveThumbnail::~CurveThumbnail()
{
    if (context.environment.curves != nullptr)
        context.environment.curves->removeChangeListener(this);
}

void CurveThumbnail::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(kBackground);
    g.fillRoundedRectangle(bounds, style::kCornerSize);

    const auto* curves = context.environment.curves;
    if (curves == nullptr || !curves->hasCurve(context.instance))
        return;

    const auto area = bounds.reduced(kPadding);
    const auto& curve = curves->get(context.instance);

    // A faint grid of quarters of the cycle.
    g.setColour(kGrid);
    for (int quarter = 0; quarter <= 4; ++quarter)
        g.drawVerticalLine(
            juce::roundToInt(area.getX() + area.getWidth() * static_cast<float>(quarter) / 4.0f), area.getY(),
            area.getBottom());
    for (float level : {0.0f, 0.5f, 1.0f})
        g.drawHorizontalLine(juce::roundToInt(area.getBottom() - level * area.getHeight()), area.getX(),
                             area.getRight());

    juce::Path line;
    int segment = 0;
    const int width = juce::jmax(2, static_cast<int>(area.getWidth()));
    for (int px = 0; px <= width; ++px)
    {
        const float x = static_cast<float>(px) / static_cast<float>(width);
        const juce::Point<float> p{area.getX() + x * area.getWidth(),
                                   area.getBottom() - curve.valueAt(x, segment) * area.getHeight()};
        if (px == 0)
            line.startNewSubPath(p);
        else
            line.lineTo(p);
    }
    juce::Path fill(line);
    fill.lineTo(area.getRight(), area.getBottom());
    fill.lineTo(area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setColour(kFill);
    g.fillPath(fill);
    g.setColour(kLine);
    g.strokePath(line, juce::PathStrokeType(1.5f));
}

void CurveThumbnail::mouseUp(const juce::MouseEvent& event)
{
    // A click, not the end of a drag that started elsewhere.
    if (!event.mouseWasClicked() || !context.environment.showEditor ||
        context.environment.curves == nullptr || !context.environment.curves->hasCurve(context.instance))
        return;

    const auto& registry = dsp::defaultEffectRegistry();
    const auto* descriptor = registry.descriptor(dsp::EffectRegistry::effectOf(context.instance));
    juce::String title =
        descriptor != nullptr ? juce::String(std::string(descriptor->displayName)) : juce::String();
    if (const int copy = dsp::EffectRegistry::copyOf(context.instance); copy > 1)
        title << " " << copy;

    context.environment.showEditor(
        std::make_unique<CurveEditor>(*context.environment.curves, registry, context.instance), title);
}

} // namespace fcm::ui

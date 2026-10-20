// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/PeakPad.h"

#include "core/ParameterIds.h"
#include "ui/Style.h"

#include <algorithm>
#include <cmath>

namespace fcm::ui
{

namespace
{
constexpr float kLabelHeight = 12.0f;
constexpr float kLeftMargin = 34.0f;
constexpr float kDotRadius = 6.0f;

float clamp01(float x) noexcept
{
    return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f;
}
} // namespace

float PeakPad::frequencyToPosition(float hz) noexcept
{
    if (!(hz > kMinHz))
        return 0.0f;
    return clamp01(std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz));
}

float PeakPad::positionToFrequency(float position) noexcept
{
    return kMinHz * std::pow(kMaxHz / kMinHz, clamp01(position));
}

float PeakPad::hardnessToPosition(float percent) noexcept
{
    return clamp01(percent * 0.01f);
}

float PeakPad::positionToHardness(float position) noexcept
{
    return clamp01(position) * 100.0f;
}

PeakPad::PeakPad(juce::AudioProcessorValueTreeState& state, const juce::String& instanceId)
{
    freqParameter = state.getParameter(core::ParameterIds::effect(instanceId.toStdString(), "freq"));
    hardnessParameter = state.getParameter(core::ParameterIds::effect(instanceId.toStdString(), "hardness"));

    // The attachments keep the dot in step with the knobs, automation and presets.
    if (freqParameter != nullptr)
    {
        freqAttachment = std::make_unique<juce::ParameterAttachment>(*freqParameter,
                                                                     [this](float value)
                                                                     {
                                                                         freq = value;
                                                                         repaint();
                                                                     });
        freqAttachment->sendInitialUpdate();
    }
    if (hardnessParameter != nullptr)
    {
        hardnessAttachment = std::make_unique<juce::ParameterAttachment>(*hardnessParameter,
                                                                         [this](float value)
                                                                         {
                                                                             hardness = value;
                                                                             repaint();
                                                                         });
        hardnessAttachment->sendInitialUpdate();
    }
    setTooltip("Drag the dot: left and right is the frequency that is boosted, up and down is how hard the "
               "clipping is. Double-click to reset.");
}

juce::Rectangle<float> PeakPad::plot() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom(kLabelHeight);
    area.removeFromLeft(kLeftMargin);
    return area.reduced(1.0f);
}

void PeakPad::paint(juce::Graphics& g)
{
    const auto area = plot();
    g.setColour(style::kWindow);
    g.fillRoundedRectangle(area, 3.0f);
    g.setColour(style::kPanelBorder);
    g.drawRoundedRectangle(area, 3.0f, 1.0f);

    // Frequency grid and labels.
    g.setFont(juce::FontOptions(10.0f));
    for (const auto& [hz, label] : {std::pair<float, const char*>{50.0f, "50"},
                                    {100.0f, "100"},
                                    {200.0f, "200"},
                                    {500.0f, "500"},
                                    {1000.0f, "1k"},
                                    {2000.0f, "2k"},
                                    {5000.0f, "5k"}})
    {
        const float x = area.getX() + frequencyToPosition(hz) * area.getWidth();
        g.setColour(style::kPanelBorder.withAlpha(0.6f));
        g.drawVerticalLine(juce::roundToInt(x), area.getY() + 1.0f, area.getBottom() - 1.0f);
        g.setColour(style::kMutedText);
        g.drawText(label,
                   juce::Rectangle<float>(x - 14.0f, area.getBottom() + 1.0f, 28.0f, kLabelHeight - 1.0f),
                   juce::Justification::centred);
    }
    // Hardness: a faint middle line and the two ends named.
    g.setColour(style::kPanelBorder.withAlpha(0.6f));
    g.drawHorizontalLine(juce::roundToInt(area.getCentreY()), area.getX() + 1.0f, area.getRight() - 1.0f);
    g.setColour(style::kMutedText);
    g.drawText("Hard", juce::Rectangle<float>(0.0f, area.getY(), kLeftMargin - 3.0f, 14.0f),
               juce::Justification::centredRight);
    g.drawText("Soft", juce::Rectangle<float>(0.0f, area.getBottom() - 14.0f, kLeftMargin - 3.0f, 14.0f),
               juce::Justification::centredRight);

    // The dot with its crosshair.
    const float x = area.getX() + frequencyToPosition(freq) * area.getWidth();
    const float y = area.getBottom() - hardnessToPosition(hardness) * area.getHeight();
    g.setColour(style::kAccent.withAlpha(0.45f));
    g.drawHorizontalLine(juce::roundToInt(y), area.getX() + 1.0f, area.getRight() - 1.0f);
    g.drawVerticalLine(juce::roundToInt(x), area.getY() + 1.0f, area.getBottom() - 1.0f);
    g.setColour(dragging ? style::kText : style::kAccent);
    g.fillEllipse(x - kDotRadius, y - kDotRadius, 2.0f * kDotRadius, 2.0f * kDotRadius);
}

void PeakPad::moveTo(juce::Point<float> position)
{
    const auto area = plot();
    if (area.getWidth() <= 0.0f || area.getHeight() <= 0.0f)
        return;

    const float horizontal = clamp01((position.x - area.getX()) / area.getWidth());
    const float vertical = clamp01((area.getBottom() - position.y) / area.getHeight());
    if (freqAttachment != nullptr)
        freqAttachment->setValueAsPartOfGesture(positionToFrequency(horizontal));
    if (hardnessAttachment != nullptr)
        hardnessAttachment->setValueAsPartOfGesture(positionToHardness(vertical));
}

void PeakPad::mouseDown(const juce::MouseEvent& e)
{
    dragging = true;
    if (freqAttachment != nullptr)
        freqAttachment->beginGesture();
    if (hardnessAttachment != nullptr)
        hardnessAttachment->beginGesture();
    moveTo(e.position);
    repaint();
}

void PeakPad::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging)
        moveTo(e.position);
}

void PeakPad::mouseUp(const juce::MouseEvent&)
{
    if (!dragging)
        return;
    dragging = false;
    if (freqAttachment != nullptr)
        freqAttachment->endGesture();
    if (hardnessAttachment != nullptr)
        hardnessAttachment->endGesture();
    repaint();
}

void PeakPad::mouseDoubleClick(const juce::MouseEvent&)
{
    // The two clicks of the double-click have already moved the dot; both values go back to their defaults.
    if (freqParameter != nullptr && freqAttachment != nullptr)
        freqAttachment->setValueAsCompleteGesture(
            freqParameter->convertFrom0to1(freqParameter->getDefaultValue()));
    if (hardnessParameter != nullptr && hardnessAttachment != nullptr)
        hardnessAttachment->setValueAsCompleteGesture(
            hardnessParameter->convertFrom0to1(hardnessParameter->getDefaultValue()));
}

} // namespace fcm::ui

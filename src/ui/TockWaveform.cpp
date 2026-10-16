// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/TockWaveform.h"

#include "core/ParameterIds.h"
#include "ui/Style.h"

#include <algorithm>

namespace fcm::ui
{

namespace
{
constexpr int kGrabDistance = 7; // points either side of a handle that still grab it
} // namespace

TockWaveform::TockWaveform(juce::AudioProcessorValueTreeState& state, const core::TockSelector& selectorIn)
    : parameters(state), selector(selectorIn)
{
    mode = state.getRawParameterValue(core::ParameterIds::kTockMode);
    startParameter = state.getParameter(core::ParameterIds::kTockTrimStart);
    endParameter = state.getParameter(core::ParameterIds::kTockTrimEnd);

    // The attachments keep the display in step with the parameters (automation, presets, undo).
    if (startParameter != nullptr)
    {
        startAttachment = std::make_unique<juce::ParameterAttachment>(
            *startParameter,
            [this](float value)
            {
                startFraction = std::clamp(static_cast<double>(value) * 0.01, 0.0, 1.0);
                repaint();
            });
        startAttachment->sendInitialUpdate();
    }
    if (endParameter != nullptr)
    {
        endAttachment = std::make_unique<juce::ParameterAttachment>(
            *endParameter,
            [this](float value)
            {
                endFraction = std::clamp(static_cast<double>(value) * 0.01, 0.0, 1.0);
                repaint();
            });
        endAttachment->sendInitialUpdate();
    }

    setTooltip(
        "Drag the two handles to choose the part of the sample that plays; double-click one to reset it");
    startTimerHz(15);
}

bool TockWaveform::sampleModeOn() const
{
    return mode != nullptr && mode->load() >= 0.5f;
}

void TockWaveform::timerCallback()
{
    // A new sample, or a change of Mode, is noticed here (neither is a parameter of this display).
    if (selector.generation() != shownGeneration || sampleModeOn() != shownSampleMode)
    {
        shownGeneration = selector.generation();
        shownSampleMode = sampleModeOn();
        repaint();
    }
}

juce::Rectangle<float> TockWaveform::plot() const
{
    return getLocalBounds().toFloat().reduced(1.0f);
}

int TockWaveform::xOf(double fraction) const
{
    const auto area = plot();
    return juce::roundToInt(area.getX() + static_cast<float>(fraction) * area.getWidth());
}

double TockWaveform::fractionAt(int x) const
{
    const auto area = plot();
    return area.getWidth() > 0.0f
               ? std::clamp(static_cast<double>((static_cast<float>(x) - area.getX()) / area.getWidth()), 0.0,
                            1.0)
               : 0.0;
}

TockWaveform::Handle TockWaveform::handleAt(juce::Point<int> position) const
{
    const int toStart = std::abs(position.x - xOf(startFraction));
    const int toEnd = std::abs(position.x - xOf(endFraction));
    if (std::min(toStart, toEnd) > kGrabDistance)
        return Handle::none;
    // When both are on top of each other, the one that can still move away is the useful one.
    if (toStart == toEnd)
        return position.x < xOf(startFraction) ? Handle::start : Handle::end;
    return toStart < toEnd ? Handle::start : Handle::end;
}

void TockWaveform::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(style::kWindow);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(style::kPanelBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    if (!sampleModeOn())
    {
        g.setColour(style::kMutedText);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("Synth", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto area = plot();
    const auto& peaks = selector.displayPeaks();
    const float middle = area.getCentreY();
    const float half = area.getHeight() * 0.5f - 2.0f;

    // The whole sample dimmed, then the played part bright on top.
    const auto drawPeaks = [&](juce::Colour colour, double from, double to)
    {
        g.setColour(colour);
        const int bins = static_cast<int>(peaks.size());
        for (int i = 0; i < bins; ++i)
        {
            const double a = static_cast<double>(i) / bins;
            const double b = static_cast<double>(i + 1) / bins;
            if (b <= from || a >= to)
                continue;
            const float x = area.getX() + static_cast<float>(a) * area.getWidth();
            const float w = std::max(1.0f, area.getWidth() / static_cast<float>(bins));
            const float h = std::max(1.0f, peaks[static_cast<size_t>(i)] * half);
            g.fillRect(x, middle - h, w, 2.0f * h);
        }
    };
    drawPeaks(style::kMutedText.withAlpha(0.35f), 0.0, 1.0);
    const double from = std::min(startFraction, endFraction);
    const double to = std::max(startFraction, endFraction);
    drawPeaks(style::kAccent, from, to);

    for (const double fraction : {startFraction, endFraction})
    {
        const int x = xOf(fraction);
        g.setColour(style::kText);
        g.fillRect(static_cast<float>(x) - 1.0f, area.getY(), 2.0f, area.getHeight());
        g.fillRect(static_cast<float>(x) - 3.0f, area.getY(), 6.0f, 6.0f);
    }
}

void TockWaveform::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(sampleModeOn() && handleAt(e.getPosition()) != Handle::none
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

void TockWaveform::mouseDown(const juce::MouseEvent& e)
{
    if (!sampleModeOn())
        return;
    dragging = handleAt(e.getPosition());
    if (dragging == Handle::start && startAttachment != nullptr)
        startAttachment->beginGesture();
    else if (dragging == Handle::end && endAttachment != nullptr)
        endAttachment->beginGesture();
}

void TockWaveform::mouseDrag(const juce::MouseEvent& e)
{
    // The handles may cross: the part that plays is always between them, but a Trim End before Trim
    // Start is a legal (1 ms) setting, so nothing is forced here.
    const auto percent = static_cast<float>(fractionAt(e.x) * 100.0);
    if (dragging == Handle::start && startAttachment != nullptr)
        startAttachment->setValueAsPartOfGesture(percent);
    else if (dragging == Handle::end && endAttachment != nullptr)
        endAttachment->setValueAsPartOfGesture(percent);
}

void TockWaveform::mouseUp(const juce::MouseEvent&)
{
    if (dragging == Handle::start && startAttachment != nullptr)
        startAttachment->endGesture();
    else if (dragging == Handle::end && endAttachment != nullptr)
        endAttachment->endGesture();
    dragging = Handle::none;
}

void TockWaveform::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!sampleModeOn())
        return;
    const auto handle = handleAt(e.getPosition());
    if (handle == Handle::start && startAttachment != nullptr)
        startAttachment->setValueAsCompleteGesture(0.0f);
    else if (handle == Handle::end && endAttachment != nullptr)
        endAttachment->setValueAsCompleteGesture(100.0f);
}

} // namespace fcm::ui

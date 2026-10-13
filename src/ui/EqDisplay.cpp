// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EqDisplay.h"

#include "ui/Style.h"

#include <cmath>

namespace fcm::ui
{

namespace
{
constexpr float kDotRadius = 7.0f;
constexpr float kHitRadius = 13.0f;
constexpr float kPlotInset = 6.0f;

const juce::Colour kBandColours[core::kEqBands] = {juce::Colour(0xffe0705a), juce::Colour(0xffe0b050),
                                                   juce::Colour(0xff6cc070), juce::Colour(0xff5aa0e0)};
} // namespace

// Pixel positions from the shared axes: x from the left, y from the top (gain 0 is the middle).
static double hzToX(double hz, double width) noexcept
{
    return width * core::eqFrequencyToUnit(hz);
}

static double xToHz(double x, double width) noexcept
{
    return width > 0.0 ? core::eqUnitToFrequency(x / width) : core::kEqMinHz;
}

static double dbToY(double db, double height) noexcept
{
    return height * (1.0 - core::eqGainToUnit(db));
}

static double yToDb(double y, double height) noexcept
{
    return height > 0.0 ? core::eqUnitToGain(1.0 - y / height) : 0.0;
}

EqDisplay::EqDisplay(juce::AudioProcessorValueTreeState& parameters) : state(parameters)
{
    const auto repaintOnChange = [this](float)
    {
        values = core::readEqBands(state);
        repaint();
    };

    for (int i = 0; i < core::kEqBands; ++i)
    {
        const juce::String prefix = "eq.b" + juce::String(i + 1) + ".";
        auto& band = bands[static_cast<size_t>(i)];
        band.on = state.getParameter(prefix + "on");
        band.freq = state.getParameter(prefix + "freq");
        band.gain = state.getParameter(prefix + "gain");
        band.q = state.getParameter(prefix + "q");
        auto* type = state.getParameter(prefix + "type");

        const auto attach = [&](juce::RangedAudioParameter* parameter)
        {
            return parameter != nullptr
                       ? std::make_unique<juce::ParameterAttachment>(*parameter, repaintOnChange)
                       : nullptr;
        };
        band.onAttachment = attach(band.on);
        band.freqAttachment = attach(band.freq);
        band.gainAttachment = attach(band.gain);
        band.qAttachment = attach(band.q);
        band.typeAttachment = attach(type);
    }
    values = core::readEqBands(state);
}

juce::Rectangle<float> EqDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced(kPlotInset);
}

juce::Point<float> EqDisplay::dotPosition(int band) const
{
    const auto area = plot();
    const auto& v = values[static_cast<size_t>(band)];
    // A cut has no gain: its dot rides the 0 dB line.
    const double db = v.isCut() ? 0.0 : v.gain;
    return {area.getX() + static_cast<float>(hzToX(v.freq, area.getWidth())),
            area.getY() + static_cast<float>(dbToY(db, area.getHeight()))};
}

int EqDisplay::bandAt(juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = kHitRadius;
    for (int i = 0; i < core::kEqBands; ++i)
    {
        const float distance = dotPosition(i).getDistanceFrom(position);
        if (distance <= bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

void EqDisplay::paint(juce::Graphics& g)
{
    const auto area = plot();
    g.setColour(juce::Colour(0xff121317));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

    // Grid: octave-ish frequency lines and 6 dB steps.
    g.setFont(juce::FontOptions(10.5f));
    for (const double hz : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
    {
        const float x = area.getX() + static_cast<float>(hzToX(hz, area.getWidth()));
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
        g.setColour(style::kMutedText.withAlpha(0.8f));
        g.drawText(hz >= 1000.0 ? juce::String(hz / 1000.0, hz >= 10000.0 ? 0 : 0) + "k"
                                : juce::String(hz, 0),
                   juce::Rectangle<float>(x + 2.0f, area.getBottom() - 12.0f, 28.0f, 11.0f),
                   juce::Justification::centredLeft);
    }
    for (const double db : {-12.0, -6.0, 0.0, 6.0, 12.0})
    {
        const float y = area.getY() + static_cast<float>(dbToY(db, area.getHeight()));
        g.setColour(juce::Colours::white.withAlpha(db == 0.0 ? 0.18f : 0.07f));
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
    }

    // The combined curve, one point per two pixels.
    juce::Path curve;
    const int steps = juce::jmax(2, juce::roundToInt(area.getWidth() / 2.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const double x = area.getWidth() * i / steps;
        const double db = core::eqResponseDb(values, xToHz(x, area.getWidth()));
        const float px = area.getX() + static_cast<float>(x);
        const float py = area.getY() + static_cast<float>(dbToY(db, area.getHeight()));
        if (i == 0)
            curve.startNewSubPath(px, py);
        else
            curve.lineTo(px, py);
    }
    juce::Path fill = curve;
    const float zeroY = area.getY() + static_cast<float>(dbToY(0.0, area.getHeight()));
    fill.lineTo(area.getRight(), zeroY);
    fill.lineTo(area.getX(), zeroY);
    fill.closeSubPath();
    g.setColour(style::kAccent.withAlpha(0.16f));
    g.fillPath(fill);
    g.setColour(style::kAccent);
    g.strokePath(curve, juce::PathStrokeType(1.8f));

    // The dots.
    for (int i = 0; i < core::kEqBands; ++i)
    {
        const auto centre = dotPosition(i);
        const auto colour = kBandColours[i].withAlpha(values[static_cast<size_t>(i)].on ? 1.0f : 0.35f);
        const float radius = (i == hovered || i == dragged) ? kDotRadius + 1.5f : kDotRadius;
        g.setColour(colour);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1),
                   juce::Rectangle<float>(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f),
                   juce::Justification::centred);
    }
}

void EqDisplay::setValue(juce::RangedAudioParameter* parameter, juce::ParameterAttachment& attachment,
                         double value)
{
    if (parameter == nullptr)
        return;
    const auto& range = parameter->getNormalisableRange();
    const float clipped = juce::jlimit(range.start, range.end, static_cast<float>(value));
    attachment.setValueAsPartOfGesture(range.snapToLegalValue(clipped));
}

void EqDisplay::mouseDown(const juce::MouseEvent& e)
{
    dragged = bandAt(e.position);
    if (dragged < 0)
        return;

    auto& band = bands[static_cast<size_t>(dragged)];
    if (band.freqAttachment != nullptr)
        band.freqAttachment->beginGesture();
    if (band.gainAttachment != nullptr)
        band.gainAttachment->beginGesture();
    repaint();
}

void EqDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragged < 0)
        return;

    auto& band = bands[static_cast<size_t>(dragged)];
    const auto area = plot();
    if (band.freqAttachment != nullptr)
        setValue(band.freq, *band.freqAttachment, xToHz(e.position.x - area.getX(), area.getWidth()));
    // A cut has no gain to drag.
    if (band.gainAttachment != nullptr && !values[static_cast<size_t>(dragged)].isCut())
        setValue(band.gain, *band.gainAttachment, yToDb(e.position.y - area.getY(), area.getHeight()));
}

void EqDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragged >= 0)
    {
        auto& band = bands[static_cast<size_t>(dragged)];
        if (band.freqAttachment != nullptr)
            band.freqAttachment->endGesture();
        if (band.gainAttachment != nullptr)
            band.gainAttachment->endGesture();
    }
    dragged = -1;
    repaint();
}

void EqDisplay::mouseMove(const juce::MouseEvent& e)
{
    const int now = bandAt(e.position);
    if (now != hovered)
    {
        hovered = now;
        setMouseCursor(now >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EqDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int index = bandAt(e.position);
    if (index < 0)
        return;

    auto& band = bands[static_cast<size_t>(index)];
    if (band.on != nullptr && band.onAttachment != nullptr)
    {
        band.onAttachment->beginGesture();
        band.onAttachment->setValueAsPartOfGesture(values[static_cast<size_t>(index)].on ? 0.0f : 1.0f);
        band.onAttachment->endGesture();
    }
}

void EqDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int index = bandAt(e.position);
    if (index < 0)
    {
        Component::mouseWheelMove(e, wheel);
        return;
    }

    auto& band = bands[static_cast<size_t>(index)];
    if (band.qAttachment == nullptr)
        return;

    // A turn of the wheel changes the Q by about 12 %, so the steps feel even from 0.1 to 10.
    const double factor = std::pow(1.12, static_cast<double>(wheel.deltaY) * 8.0);
    band.qAttachment->beginGesture();
    setValue(band.q, *band.qAttachment, values[static_cast<size_t>(index)].q * factor);
    band.qAttachment->endGesture();
}

} // namespace fcm::ui

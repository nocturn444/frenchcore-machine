// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WavetableDisplay.h"

#include "core/WavetableShape.h"
#include "dsp/oscillator/WavetableData.h"

namespace fcm::ui
{

namespace
{
constexpr size_t kFrameSize = dsp::WavetableData::kFrameSize;
constexpr int kMaxStackedFrames = 12;
const juce::Colour kBackground(0xff0e0f12);
const juce::Colour kFaint(0xff3a4a66);
const juce::Colour kBright(0xff6fb7ff);

juce::Path pathFor(const float* samples, juce::Rectangle<float> area)
{
    juce::Path path;
    const int points = std::max(2, static_cast<int>(area.getWidth()));
    for (int i = 0; i < points; ++i)
    {
        const float s = samples[static_cast<size_t>(i) * (kFrameSize - 1) / static_cast<size_t>(points - 1)];
        const float x =
            area.getX() + area.getWidth() * static_cast<float>(i) / static_cast<float>(points - 1);
        const float y = area.getCentreY() - s * area.getHeight() * 0.5f;
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }
    return path;
}
} // namespace

WavetableDisplay::WavetableDisplay(core::WavetableSelector& selectorIn,
                                   juce::RangedAudioParameter& positionParameter)
    : selector(selectorIn), positionAttachment(positionParameter,
                                               [this](float value)
                                               {
                                                   position = value;
                                                   repaint();
                                               })
{
    positionAttachment.sendInitialUpdate();
    selector.changes().addChangeListener(this);
}

WavetableDisplay::~WavetableDisplay()
{
    selector.changes().removeChangeListener(this);
}

void WavetableDisplay::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}

void WavetableDisplay::paint(juce::Graphics& g)
{
    g.fillAll(kBackground);

    const auto& frames = selector.displayFrames();
    auto area = getLocalBounds().toFloat().reduced(6.0f);

    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(selector.displayName(), area.removeFromTop(18.0f).toNearestInt(),
               juce::Justification::centredLeft);

    if (frames.empty() || frames.size() % kFrameSize != 0)
        return;

    const size_t numFrames = frames.size() / kFrameSize;
    const int stacked = static_cast<int>(std::min<size_t>(numFrames, kMaxStackedFrames));

    // The stack of frames, drawn from the back (last frame) to the front (first frame).
    g.setColour(kFaint);
    for (int i = stacked - 1; i >= 0; --i)
    {
        const size_t frame =
            stacked > 1 ? static_cast<size_t>(i) * (numFrames - 1) / static_cast<size_t>(stacked - 1) : 0;
        g.strokePath(pathFor(frames.data() + frame * kFrameSize, area), juce::PathStrokeType(1.0f));
    }

    // The waveform at the current Position.
    const auto current = core::interpolatedFrame(frames, kFrameSize, position);
    if (!current.empty())
    {
        g.setColour(kBright);
        g.strokePath(pathFor(current.data(), area), juce::PathStrokeType(2.0f));
    }
}

} // namespace fcm::ui

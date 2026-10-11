// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

namespace fcm::dsp
{

// Slides the settling pitch (a fractional MIDI note number) toward a target with a linear ramp in
// semitones, which is even-sounding musically (docs/specs/voice-note-handling.md §4.2). The ramp
// always takes the glide time, whatever the interval.
class PitchGlide
{
  public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        hasValue = false;
        samplesLeft = 0;
        current = target = 0.0f;
        step = 0.0f;
    }

    // Calling it again with the same target is a no-op, so it can be called every block.
    void setTarget(float note, bool glideOn, float glideMs) noexcept
    {
        if (hasValue && juce::exactlyEqual(note, target))
            return;

        // A glide time that is not a sensible number (NaN, huge) must not overflow the sample count.
        const double safeMs =
            std::isfinite(glideMs) ? std::clamp(static_cast<double>(glideMs), 0.0, 60000.0) : 0.0;
        const auto glideSamples = static_cast<int>(safeMs * 0.001 * sampleRate + 0.5);
        target = note;
        if (!hasValue || !glideOn || glideSamples < 1)
        {
            current = note;
            samplesLeft = 0;
        }
        else
        {
            samplesLeft = glideSamples;
            step = (target - current) / static_cast<float>(glideSamples);
        }
        hasValue = true;
    }

    // Advances one sample and returns the pitch in MIDI note numbers.
    float next() noexcept
    {
        if (samplesLeft > 0)
        {
            current += step;
            if (--samplesLeft == 0)
                current = target;
        }
        return current;
    }

    bool hasPitch() const noexcept { return hasValue; }

  private:
    double sampleRate = 44100.0;
    bool hasValue = false;
    int samplesLeft = 0;
    float current = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
};

} // namespace fcm::dsp

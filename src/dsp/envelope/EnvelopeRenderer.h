// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/EnvelopeShape.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

// Plays an EnvelopeShape one sample at a time (docs/specs/envelope-breakpoint.md §4.2). Real-time
// safe: no allocation, no locks.
//
// Amp use: after the last point the level is held while the key is down, release() fades it out
// linearly from where it is, and a held level of 0 ends the envelope. Pitch use: release() is not
// called and the last level is held for ever.
class EnvelopeRenderer
{
  public:
    static constexpr float kSilence = 1.0e-4f; // below this a held level counts as finished

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        phase = Phase::idle;
        positionMs = 0.0;
        segment = 0;
        level = 0.0f;
    }

    // May be called at any time, also while sounding: the envelope continues on the new shape.
    void setShape(const EnvelopeShape* newShape) noexcept
    {
        if (newShape != shape)
        {
            shape = newShape;
            segment = 0;
        }
    }

    // Starts from time 0. `timeScale` multiplies every time of the shape, release included.
    void start(float timeScale) noexcept
    {
        // std::max would pass a NaN through, which would freeze the envelope and the voice with it.
        scale = std::isfinite(timeScale) ? std::max(timeScale, 0.01f) : 1.0f;
        msPerSample = 1000.0 / (sampleRate * static_cast<double>(scale));
        positionMs = 0.0;
        segment = 0;
        level = 0.0f;
        phase = shape != nullptr && shape->count > 0 ? Phase::running : Phase::idle;
    }

    // The key was let go: fade out linearly from the current level. Does nothing when idle.
    void release() noexcept
    {
        if (phase != Phase::running)
            return;

        const double releaseSamples = std::max(1.0, static_cast<double>(shape->releaseMs) * 0.001 *
                                                        sampleRate * static_cast<double>(scale));
        releaseStep = level / static_cast<float>(releaseSamples);
        phase = Phase::releasing;
    }

    bool isActive() const noexcept { return phase != Phase::idle; }

    float next() noexcept
    {
        switch (phase)
        {
        case Phase::running:
        {
            level = shape->valueAt(static_cast<float>(positionMs), segment);
            positionMs += msPerSample;

            // At the end of the shape a level of 0 means nothing more will sound.
            if (positionMs >= static_cast<double>(shape->lastTimeMs()) && shape->lastValue() < kSilence)
                phase = Phase::idle;
            return level;
        }
        case Phase::releasing:
            level -= releaseStep;
            if (level <= 0.0f)
            {
                level = 0.0f;
                phase = Phase::idle;
            }
            return level;
        case Phase::idle:
            break;
        }
        return 0.0f;
    }

  private:
    enum class Phase
    {
        idle,
        running,
        releasing
    };

    const EnvelopeShape* shape = nullptr;
    double sampleRate = 44100.0;
    float scale = 1.0f;
    double msPerSample = 1000.0 / 44100.0;
    double positionMs = 0.0;
    int segment = 0;
    float level = 0.0f;
    float releaseStep = 0.0f;
    Phase phase = Phase::idle;
};

} // namespace fcm::dsp

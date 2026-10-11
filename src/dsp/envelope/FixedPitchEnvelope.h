// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

// Phase 2 placeholder for the pitch envelope (replaced by the BreakpointEnvelope in Phase 3).
// Returns the pitch offset in semitones above the settling pitch: a fast exponential fall that
// reaches exactly 0 after kDropMs, so the tail sits on the MIDI note's pitch with no residual
// error (docs/specs/voice-note-handling.md §4.4). Shape: (e^(-t/tau) - e^(-T/tau)) / (1 - e^(-T/tau))
// with T / tau = kShape.
class FixedPitchEnvelope
{
  public:
    static constexpr float kAmountSemitones = 48.0f;
    static constexpr double kDropMs = 90.0;
    static constexpr double kShape = 5.0; // T / tau: how steep the exponential is

    void prepare(double sampleRate) noexcept
    {
        dropSamples = std::max(1, static_cast<int>(std::lround(kDropMs * 0.001 * sampleRate)));
        // d^dropSamples = e^-kShape exactly, so the fall ends on the sample the drop time is up.
        decay = std::exp(-kShape / dropSamples);
        floorValue = std::exp(-kShape);
        reset();
    }

    void reset() noexcept
    {
        remaining = 0;
        running = 1.0;
    }

    void start() noexcept
    {
        remaining = dropSamples;
        running = 1.0;
    }

    bool isActive() const noexcept { return remaining > 0; }

    float next() noexcept
    {
        if (remaining <= 0)
            return 0.0f;

        const double shaped = (running - floorValue) / (1.0 - floorValue);
        running *= decay;
        --remaining;
        return kAmountSemitones * static_cast<float>(std::max(shaped, 0.0));
    }

  private:
    int dropSamples = 1;
    int remaining = 0;
    double decay = 0.99;
    double running = 1.0;
    double floorValue = 0.0;
};

} // namespace fcm::dsp

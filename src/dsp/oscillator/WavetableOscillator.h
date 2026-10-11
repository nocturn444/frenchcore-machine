// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/oscillator/WavetableData.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

// Band-limited wavetable oscillator (docs/specs/oscillator-wavetable.md §4.2). The voice owns two
// instances. Real-time safe: no allocation or locks. The wavetable is owned elsewhere and must
// outlive the oscillator.
class WavetableOscillator
{
  public:
    void prepare(double newSampleRate) noexcept;

    // Pass nullptr to output silence.
    void setWavetable(const WavetableData* newTable) noexcept { table = newTable; }

    // 0..1 across all frames of the wavetable. Anything else (NaN, infinity, out of range) is
    // brought into range, so garbage can never make the oscillator read outside its table.
    void setPosition(float newPosition) noexcept
    {
        position = std::isfinite(newPosition) ? std::clamp(newPosition, 0.0f, 1.0f) : 0.0f;
    }

    // startPhase is in cycles (0..1); the whole part is dropped, and a value that is not a finite
    // number starts at 0.
    void reset(double startPhase = 0.0) noexcept { phase = wrapPhase(startPhase); }

    // Advances by one sample at the given frequency in Hz and returns the output.
    float process(double frequencyHz) noexcept;

    // Smallest mip level whose highest harmonic cannot exceed Nyquist at this frequency, always in
    // 0 .. kNumMipLevels - 1 (a frequency that is not a positive number gives 0).
    static int mipLevelFor(double frequencyHz, double sampleRate) noexcept;

    // `phase` reduced to [0, 1), strictly: a result of exactly 1 (from a tiny negative input that
    // rounds up) becomes 0, and anything not finite becomes 0.
    static double wrapPhase(double phase) noexcept;

  private:
    const WavetableData* table = nullptr;
    double sampleRate = 44100.0;
    double phase = 0.0;
    float position = 0.0f;
};

} // namespace fcm::dsp

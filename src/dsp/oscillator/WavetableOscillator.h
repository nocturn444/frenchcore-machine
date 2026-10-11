// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/oscillator/WavetableData.h"

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

    // 0..1 across all frames of the wavetable.
    void setPosition(float newPosition) noexcept { position = newPosition; }

    // startPhase is in cycles (0..1).
    void reset(double startPhase = 0.0) noexcept { phase = startPhase - std::floor(startPhase); }

    // Advances by one sample at the given frequency in Hz and returns the output.
    float process(double frequencyHz) noexcept;

    // Smallest mip level whose highest harmonic cannot exceed Nyquist at this frequency.
    static int mipLevelFor(double frequencyHz, double sampleRate) noexcept;

  private:
    const WavetableData* table = nullptr;
    double sampleRate = 44100.0;
    double phase = 0.0;
    float position = 0.0f;
};

} // namespace fcm::dsp

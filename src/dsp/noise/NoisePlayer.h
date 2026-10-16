// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/noise/NoiseSample.h"

#include <cstdint>

namespace fcm::dsp
{

// What a noise needs at note-on (docs/specs/noise-oscillator.md): whether it plays once, and where it
// starts: `phase` plus up to `random` of the length (both 0 to 1).
struct NoiseSettings
{
    bool oneShot = false;
    float phase = 0.0f;
    float random = 1.0f;
};

// Plays a NoiseSample at a pitch (docs/specs/noise-oscillator.md §4): linear interpolation at the rate
// `2^(semitones / 12) * sampleRate of the sample / engine rate`. In loop mode the last 5 ms are
// blended with the beginning so the wrap never clicks; One Shot stops at the end. Plain data, so it can
// be copied into the fading copy of the previous kick. Costs nothing when idle.
class NoisePlayer
{
  public:
    static constexpr double kLoopCrossfadeSeconds = 0.005;

    void prepare(double newSampleRate) noexcept;

    // The sound to play; nullptr is silence. Must outlive the block.
    void setSample(const NoiseSample* newSample) noexcept { sample = newSample; }

    // Begins a new noise at its start point. Settings that are not finite are brought into range.
    void start(const NoiseSettings& settings) noexcept;

    // The next output sample at `semitones` of pitch (the Pitch control plus Key Track's share of the
    // note). Exactly 0 when it is not running. A pitch that is not a number plays at 0.
    float process(double semitones) noexcept;

    void reset() noexcept;
    bool isRunning() const noexcept { return running; }

  private:
    float value(int index) const noexcept;

    double sampleRate = 44100.0;
    const NoiseSample* sample = nullptr;
    bool running = false;
    bool oneShot = false;
    double position = 0.0; // in samples of the sound
    std::uint32_t randomState = 0x2545f491u;
};

} // namespace fcm::dsp

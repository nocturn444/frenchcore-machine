// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/noise/NoiseSample.h"

#include <juce_core/juce_core.h>

namespace fcm::core
{

// Turning a WAV file into a noise sample (docs/specs/noise-oscillator.md §4). Like every file that
// anyone can share, it is untrusted: each limit is checked before the work it protects, and anything
// unusable is refused with a message. Runs on a background thread, never on the audio thread.
inline constexpr int kMinNoiseSamples = 16;
inline constexpr double kMaxNoiseSeconds = 10.0;
inline constexpr double kStoredNoiseRate = 48000.0; // a sample at a higher rate is resampled to this
inline constexpr int kMaxNoiseSamples = 480000;     // 10 seconds at 48 kHz
inline constexpr double kMinNoiseSampleRate = 8000.0;
inline constexpr double kMaxNoiseSampleRate = 192000.0;

struct NoiseImportResult
{
    dsp::NoiseSample sample; // empty samples on failure
    juce::String error;      // user-facing reason when the sample is empty

    bool ok() const noexcept { return !sample.samples.empty(); }
};

NoiseImportResult importNoiseSample(const void* data, size_t numBytes);

// Checks a sample that comes from saved state: 16 to 480000 samples, all finite and at most 4 in
// magnitude, and a sample rate of 8000 to 192000 Hz.
bool isValidStoredNoiseSample(const dsp::NoiseSample& sample) noexcept;

} // namespace fcm::core

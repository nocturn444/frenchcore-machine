// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/tock/TockSample.h"

#include <juce_core/juce_core.h>

namespace fcm::core
{

// Turning a WAV file into a Tock sample (docs/specs/tock.md §4.1). Like every file that anyone can
// share, it is untrusted: each limit is checked before the work it protects, and anything unusable
// is refused with a message. Runs on a background thread, never on the audio thread.
inline constexpr int kMinTockSamples = 16;
inline constexpr int kMaxTockSamples = 384000;
inline constexpr double kMaxTockSeconds = 2.0;
inline constexpr double kMinTockSampleRate = 8000.0;
inline constexpr double kMaxTockSampleRate = 192000.0;

struct TockImportResult
{
    dsp::TockSample sample; // empty samples on failure
    juce::String error;     // user-facing reason when the sample is empty

    bool ok() const noexcept { return !sample.samples.empty(); }
};

TockImportResult importTockSample(const void* data, size_t numBytes);

// Checks a sample that comes from saved state: 16 to 384000 samples, all finite and at most 4 in
// magnitude, and a sample rate of 8000 to 192000 Hz.
bool isValidStoredTockSample(const dsp::TockSample& sample) noexcept;

} // namespace fcm::core

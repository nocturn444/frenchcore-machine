// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace fcm::core
{

// Turning a WAV file into wavetable frames (docs/specs/wavetable-user.md §4). The bytes come from a
// file anyone can share, so every limit is checked before the work it protects, and anything
// unusable is refused with a message. Runs on a background thread, never on the audio thread.
inline constexpr size_t kMaxWavetableFileBytes = 32u * 1024u * 1024u;
inline constexpr int kMaxWavetableChannels = 8;
inline constexpr int kMinCycleSamples = 64;
inline constexpr int kMaxCycleSamples = 16384;
inline constexpr int kMaxWavetableSamples = 256 * 2048;

struct WavetableImportResult
{
    std::vector<float> frames; // numFrames * 2048 samples, empty on failure
    juce::String error;        // user-facing reason when `frames` is empty

    bool ok() const noexcept { return !frames.empty(); }
};

WavetableImportResult importWavetable(const void* data, size_t numBytes);

// Checks frames that come from saved state: a whole number of 2048-sample frames (1 to 256), every
// sample finite and at most 4 in magnitude. Returns false for anything else.
bool isValidStoredWavetable(const std::vector<float>& frames) noexcept;

// A display name that is safe to show and to save: printable characters only, at most 64 of them.
juce::String sanitiseWavetableName(const juce::String& name);

} // namespace fcm::core

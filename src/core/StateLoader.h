// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// States larger than this are not even parsed (docs/specs/state-safe-loading.md).
inline constexpr int kMaxStateBytes = 32 * 1024 * 1024;

enum class StateLoadResult
{
    loaded,
    loadedFromNewerVersion, // the known controls were loaded, the rest ignored
    rejected                // not the plugin's state: nothing was changed
};

// Loads a saved state into the parameters without trusting it: checks the root node, handles the
// version and migrations, replaces non-finite values with defaults, clamps everything into the
// parameters' ranges and ignores unknown controls. Message thread only.
StateLoadResult loadState(juce::AudioProcessorValueTreeState& state, juce::ValueTree saved,
                          const juce::Identifier& expectedType);

// The two layers of loadState, exposed for tests: cleans the PARAM entries of a saved tree, and
// resets any live parameter whose value is not finite or not within its range.
void sanitizeSavedParameters(juce::ValueTree& saved, const juce::AudioProcessorValueTreeState& state);

// Adds an entry with the default value for every parameter the saved tree does not mention, so a
// saved state always describes everything. Applied after sanitizeSavedParameters.
void addMissingParameters(juce::ValueTree& saved, const juce::AudioProcessorValueTreeState& state);
void sanitizeLiveParameters(juce::AudioProcessorValueTreeState& state);

} // namespace fcm::core

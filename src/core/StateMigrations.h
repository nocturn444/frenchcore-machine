// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <span>

namespace fcm::core
{

// One step that rewrites a saved state tree from version `fromVersion` to `fromVersion + 1`
// (docs/ARCHITECTURE.md §5). Steps only ever see trees that were already checked to be the
// plugin's state node.
struct Migration
{
    int fromVersion;
    void (*apply)(juce::ValueTree& state);
};

// The version a saved state declares. A missing, non-numeric or smaller-than-1 value counts as
// version 1 (the first format), and absurdly large numbers are capped, so the result is always
// usable.
int readStateVersion(const juce::ValueTree& state);

// Runs the steps from `fromVersion` up to `toVersion` in order. Returns false when a step is
// missing; the tree is then left at the last version that could be reached. Does nothing when
// `fromVersion >= toVersion`.
bool applyMigrations(juce::ValueTree& state, int fromVersion, int toVersion,
                     std::span<const Migration> steps);

// The steps of this build, one per version up to kStateVersion.
std::span<const Migration> stateMigrations() noexcept;

} // namespace fcm::core

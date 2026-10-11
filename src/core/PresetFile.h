// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace fcm::core
{

// The preset file format (docs/specs/presets.md §4.1): an XML wrapper with a name around the
// project state. Everything read from a file is untrusted input.
inline constexpr int kPresetFormatVersion = 1;
inline constexpr juce::int64 kMaxPresetBytes = 16 * 1024 * 1024;
inline constexpr int kMaxPresetNameLength = 64;
inline constexpr int kMaxPresetXmlDepth = 64;
inline const char* const kPresetExtension = ".fcmpreset";

// A name safe to use as a file name, or an empty string when nothing usable is left
// (docs/specs/presets.md §4.2). Idempotent.
juce::String sanitizePresetName(const juce::String& name);

// The text of a preset file for `state` (a project state tree) under `name` (sanitised here).
juce::String encodePreset(const juce::String& name, const juce::ValueTree& state);

struct PresetParseResult
{
    bool ok = false;
    juce::String name;     // sanitised, never empty when `ok`
    juce::ValueTree state; // the project state, still to be loaded as untrusted
    juce::String error;    // a short message for the user when not `ok`
};

PresetParseResult parsePreset(const juce::String& text);

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/PresetFile.h"

#include <vector>

namespace fcm::core
{

// The user's preset files in one folder (docs/specs/presets.md §4.2, §4.4). It knows nothing of the
// plugin: it lists, writes and reads files.
class PresetLibrary
{
  public:
    static constexpr int kMaxListed = 1000;

    explicit PresetLibrary(juce::File presetFolder) : folder(std::move(presetFolder)) {}

    // Documents/Frenchcore Machine/Presets
    static juce::File defaultFolder();

    const juce::File& getFolder() const noexcept { return folder; }

    // Regular `.fcmpreset` files directly in the folder, sorted by name without regard to case, at
    // most kMaxListed. Contents are not read.
    std::vector<juce::File> list() const;

    struct SaveResult
    {
        bool ok = false;
        juce::File file;
        juce::String message; // for the user when not `ok`
    };

    // Writes `state` as `<sanitised name>.fcmpreset` in the folder, replacing a file of that name.
    SaveResult save(const juce::String& name, const juce::ValueTree& state) const;

    // True if saving under `name` would replace a file.
    bool exists(const juce::String& name) const;

    static PresetParseResult load(const juce::File& file);

  private:
    juce::File folder;
};

} // namespace fcm::core

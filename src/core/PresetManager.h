// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/PresetLibrary.h"
#include "core/ProjectState.h"

#include <juce_events/juce_events.h>
#include <vector>

namespace fcm::core
{

// One numbered list of presets (the factory ones, then the user's files) and the current sound's
// name (docs/specs/presets.md §4.4). Message thread only. It announces changes to its listeners.
class PresetManager : public juce::ChangeBroadcaster
{
  public:
    PresetManager(ProjectState& project, PresetLibrary library, juce::Identifier rootType);

    struct Entry
    {
        juce::String name;
        bool factory = false;
        juce::File file; // user presets only
    };

    // Reads the folder again. The factory entries come first.
    void refresh();
    const std::vector<Entry>& entries() const noexcept { return list; }
    int numFactory() const noexcept;

    // Loads entry `index`. On failure the sound and the current name are unchanged and the message
    // says why.
    bool load(int index);
    bool next();
    bool previous();

    // Saves the current sound under `name`; it becomes the current preset. Replaces a file of the
    // same name (ask first with `wouldReplace`).
    bool save(const juce::String& name);
    bool wouldReplace(const juce::String& name) const { return library.exists(name); }

    const juce::String& currentName() const noexcept { return current; }
    int currentIndex() const noexcept { return currentEntry; }
    const juce::String& lastMessage() const noexcept { return message; }

  private:
    bool step(int direction);

    ProjectState& project;
    PresetLibrary library;
    juce::Identifier rootType;
    std::vector<Entry> list;
    juce::String current{"Init"};
    int currentEntry = -1;
    juce::String message;
};

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace fcm::core
{

// The presets that ship with the plugin (docs/specs/presets.md §4.3). Each one lists only the
// controls it sets; loading it as a state makes every other control its default.
class FactoryPresets
{
  public:
    static int count() noexcept;

    // Display name of preset `index`, or an empty string when out of range.
    static juce::String name(int index);

    // The preset as a project state tree with root `rootType`, or an invalid tree when `index` is out
    // of range. Message thread.
    static juce::ValueTree makeState(int index, const juce::Identifier& rootType);
};

} // namespace fcm::core

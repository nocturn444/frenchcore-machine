// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fcm::ui
{

// An optional picture an effect shows above its controls in the FX rack, such as the EQ's response
// curve. It is a table entry, not a change to the rack: an effect without an entry shows only its
// controls (docs/ADDING_AN_EFFECT.md).
struct EffectDecoration
{
    int height = 0;
    // `instanceId` is the copy of the effect the picture belongs to ("eq", "eq2", ...).
    // `note` gives the pitch the tail is on, in semitones from C3, for pictures that follow the note.
    std::function<std::unique_ptr<juce::Component>(juce::AudioProcessorValueTreeState&, const juce::String&,
                                                   const std::function<float()>&)>
        make;
};

// A switch that, when the user turns it on in the window, copies the value of one parameter of the
// effect into another so the sound does not jump (the EQ's Extreme starts its wider Gain and Q at
// the current ones, docs/specs/fx-eq.md section 8.3). Suffixes are the ones after the effect ID.
// It is the editor's doing: automating the switch alone does not copy.
struct CopyOnSwitch
{
    std::string switchSuffix;
    std::string fromSuffix;
    std::string toSuffix;
};

// The rules of an effect ID (empty for most).
const std::vector<CopyOnSwitch>& copyOnSwitchRules(std::string_view effectId);

// The decoration of an effect ID, or null.
const EffectDecoration* decorationFor(std::string_view effectId);

} // namespace fcm::ui

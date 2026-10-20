// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/EffectCurveState.h"

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fcm::ui
{

// What the window gives every decoration (docs/decisions/0012 item 4). The same environment serves
// all the racks; the rack adds the instance.
struct DecorationEnvironment
{
    // The pitch the tail is on, in semitones from C3, for pictures that follow the note (may be empty).
    std::function<float()> note;
    // The drawn curves of the effects that have one, or nullptr.
    core::EffectCurveState* curves = nullptr;
    // Asks the Effects box to show a larger editor over the rack, with a title and a Done button
    // (docs/decisions/0012 item 4). At most one is open at a time; a new one replaces the old. The
    // component is owned by the box and destroyed when the editor closes. May be empty.
    std::function<void(std::unique_ptr<juce::Component> editor, const juce::String& title)> showEditor;
};

// What a decoration is built from: the parameters, the copy of the effect it belongs to and the
// environment. A decoration may change parameters through `state` (with begin and end of a gesture,
// so the DAW records automation) and curves through `environment.curves`.
struct DecorationContext
{
    juce::AudioProcessorValueTreeState& state;
    juce::String instanceId; // the copy of the effect: "eq", "eq2", ...
    int instance = -1;       // its pool index (EffectRegistry::indexOfInstance)
    DecorationEnvironment environment;
};

// An optional picture or control an effect shows above its controls in the FX rack, such as the EQ's
// response curve. It is a table entry, not a change to the rack: an effect without an entry shows only
// its controls (docs/ADDING_AN_EFFECT.md).
struct EffectDecoration
{
    int height = 0;
    std::function<std::unique_ptr<juce::Component>(const DecorationContext&)> make;
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

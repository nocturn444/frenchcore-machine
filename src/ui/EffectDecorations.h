// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string_view>

namespace fcm::ui
{

// An optional picture an effect shows above its controls in the FX rack, such as the EQ's response
// curve. It is a table entry, not a change to the rack: an effect without an entry shows only its
// controls (docs/ADDING_AN_EFFECT.md).
struct EffectDecoration
{
    int height = 0;
    std::function<std::unique_ptr<juce::Component>(juce::AudioProcessorValueTreeState&)> make;
};

// The decoration of an effect ID, or null.
const EffectDecoration* decorationFor(std::string_view effectId);

} // namespace fcm::ui

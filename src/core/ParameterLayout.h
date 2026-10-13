// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectRegistry.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// Builds the full automatable parameter layout. Effects and other modules contribute their own
// parameter groups here as they are implemented.
// The effects come from `registry` (docs/specs/fx-chain.md): each one adds its on switch, its mix and
// the parameters it describes, so adding an effect never touches this file.
juce::AudioProcessorValueTreeState::ParameterLayout
createParameterLayout(const dsp::EffectRegistry& registry = dsp::defaultEffectRegistry());

} // namespace fcm::core

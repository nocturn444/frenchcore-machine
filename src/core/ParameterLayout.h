// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// Builds the full automatable parameter layout. Effects and other modules contribute their own
// parameter groups here as they are implemented.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace fcm::core

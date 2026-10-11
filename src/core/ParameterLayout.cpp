// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ParameterLayout.h"

#include "core/ParameterIds.h"

namespace fcm::core
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParameterIds::kOutputGain, 1}, "Output Gain",
        juce::NormalisableRange<float>{-48.0f, 12.0f, 0.1f}, 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")));

    return layout;
}

} // namespace fcm::core

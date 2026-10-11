// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginProcessor.h"

#include "core/ParameterIds.h"
#include "core/ParameterLayout.h"
#include "plugin/PluginEditor.h"

namespace fcm
{

namespace
{
const juce::Identifier kStateType{"FrenchcoreMachineState"};
const juce::Identifier kStateVersionProperty{"stateVersion"};
} // namespace

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, kStateType, core::createParameterLayout())
{
    state.state.setProperty(kStateVersionProperty, core::kStateVersion, nullptr);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock),
                    static_cast<juce::uint32>(getTotalNumOutputChannels())});
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process(buffer, midi);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes); xml != nullptr && xml->hasTagName(kStateType))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}

} // namespace fcm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fcm::PluginProcessor();
}

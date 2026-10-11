// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginProcessor.h"

#include "core/BoundedMidiCopy.h"
#include "core/ParameterIds.h"
#include "core/ParameterLayout.h"
#include "core/StateLoader.h"
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
      state(*this, nullptr, kStateType, core::createParameterLayout()), parameterReader(state)
{
    state.state.setProperty(kStateVersionProperty, core::kStateVersion, nullptr);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Parameters given before prepare() apply immediately instead of ramping from the defaults.
    engine.setParameters(parameterReader.read());
    engine.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock),
                    static_cast<juce::uint32>(getTotalNumOutputChannels())});

    if (isStandalone())
    {
        auditionSequencer.prepare(sampleRate);
        auditionMidi.ensureSize(core::kAuditionMidiBytes);
    }
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
    engine.setParameters(parameterReader.read()); // once per block

    if (isStandalone())
    {
        addAuditionEvents(buffer.getNumSamples(), midi);
        engine.process(buffer, auditionMidi);
        return;
    }

    engine.process(buffer, midi);
}

void PluginProcessor::addAuditionEvents(int numSamples, const juce::MidiBuffer& hostMidi)
{
    auditionMidi.clear();
    // Bounded copy: a burst bigger than the reserved room would make the buffer grow (allocate).
    core::copyMidiWithinBudget(hostMidi, auditionMidi, numSamples, core::kHostMidiBudgetBytes,
                               core::kHostMidiPriorityBudgetBytes);

    const bool autoPlay = auditionState.isAutoPlay();

    dsp::LiveNote event;
    while (auditionState.popNote(event))
    {
        if (autoPlay)
        {
            // While the loop runs the keys only choose its pitch (at the next kick).
            if (event.on)
                auditionSequencer.setNote(event.note);
        }
        else if (event.on)
        {
            auditionMidi.addEvent(
                juce::MidiMessage::noteOn(1, event.note, static_cast<juce::uint8>(event.velocity)), 0);
        }
        else
        {
            auditionMidi.addEvent(juce::MidiMessage::noteOff(1, event.note), 0);
        }
    }

    auditionSequencer.setEnabled(autoPlay);
    auditionSequencer.setBpm(auditionState.getBpm());
    auditionSequencer.process(numSamples, auditionMidi);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // The version is always written, whatever the tree held before (docs/specs/state-safe-loading.md).
    auto tree = state.copyState();
    tree.setProperty(kStateVersionProperty, core::kStateVersion, nullptr);

    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // The host's bytes are not trusted: too big or not our state means nothing changes, and
    // everything else goes through the checks of core::loadState.
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > core::kMaxStateBytes)
        return;

    if (auto xml = getXmlFromBinary(data, sizeInBytes); xml != nullptr && xml->hasTagName(kStateType))
        core::loadState(state, juce::ValueTree::fromXml(*xml), kStateType);
}

} // namespace fcm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fcm::PluginProcessor();
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginProcessor.h"

#include "core/BoundedMidiCopy.h"
#include "core/ParameterIds.h"
#include "core/ParameterLayout.h"
#include "core/StateLoader.h"
#include "core/TailLength.h"
#include "dsp/fx/EffectRegistry.h"
#include "plugin/PluginEditor.h"

#include <cmath>

namespace fcm
{

namespace
{
const juce::Identifier kStateType{"FrenchcoreMachineState"};
const juce::Identifier kStateVersionProperty{"stateVersion"};
} // namespace

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, kStateType, core::createParameterLayout()), parameterReader(state),
      project(core::ProjectState::Parts{
          state, kStateType, ampEnvelope, pitchEnvelope, fxOrder, {&wavetable1, &wavetable2}, tockSelector})
{
    state.state.setProperty(kStateVersionProperty, core::kStateVersion, nullptr);
    state.addParameterListener(core::ParameterIds::kQuality, this);
    engine.setEnvelopeSources(&ampEnvelope.source(), &pitchEnvelope.source());
    engine.setWavetableSources(&wavetable1.source(), &wavetable2.source());
    engine.setTockSource(&tockSelector.source());
}

double PluginProcessor::getTailLengthSeconds() const
{
    // After the key goes up the amp envelope fades out over its release, stretched by the time scale.
    return core::tailLengthSeconds(ampEnvelope.getReleaseSeconds(),
                                   state.getRawParameterValue(core::ParameterIds::kAmpEnvTimeScale)->load());
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Parameters given before prepare() apply immediately instead of ramping from the defaults.
    engine.setParameters(readEngineParameters());
    engine.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock),
                    static_cast<juce::uint32>(getTotalNumOutputChannels())});
    reportLatency();

    if (isStandalone())
    {
        auditionSequencer.prepare(sampleRate);
        auditionMidi.ensureSize(core::kAuditionMidiBytes);
    }
}

PluginProcessor::~PluginProcessor()
{
    state.removeParameterListener(core::ParameterIds::kQuality, this);
    cancelPendingUpdate();
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void PluginProcessor::handleAsyncUpdate()
{
    reportLatency();
}

void PluginProcessor::reportLatency()
{
    // The latency of the chain depends only on Quality, never on what is switched on.
    setLatencySamples(engine.latencySamples(parameterReader.read().oversamplingFactor));
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
    engine.setParameters(readEngineParameters()); // once per block

    if (isStandalone())
    {
        addAuditionEvents(buffer.getNumSamples(), midi);
        engine.process(buffer, auditionMidi);
        return;
    }

    engine.process(buffer, midi);
}

dsp::EngineParameters PluginProcessor::readEngineParameters()
{
    auto parameters = parameterReader.read();

    // The Standalone has its own BPM control; a plugin follows the host, and keeps the last tempo
    // when the host gives none (or 120 before it ever did).
    float bpm = tempoState.get();
    if (isStandalone())
        bpm = auditionState.getBpm();
    else if (const auto* host = getPlayHead())
        if (const auto position = host->getPosition())
            if (const auto hostBpm = position->getBpm())
                bpm = static_cast<float>(*hostBpm);

    // A host (or a damaged value) can hand over 0, a negative number or a NaN: keep the last good tempo.
    if (!std::isfinite(bpm) || bpm <= 0.0f)
        bpm = tempoState.get();

    tempoState.set(bpm);
    parameters.bpm = bpm;
    return parameters;
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
    if (auto xml = project.toXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // The host's bytes are not trusted: too big or not our state means nothing changes, and
    // everything else goes through the checks of core::ProjectState.
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > core::kMaxStateBytes)
        return;

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        project.restoreFromXml(*xml);
}

void PluginProcessor::loadUserWavetable(int oscillator, const juce::File& file)
{
    if (oscillator < 0 || oscillator >= dsp::kNumOscillators)
        return;

    wavetableLoader.load(
        file,
        [this, oscillator](core::WavetableImportResult result, juce::String name)
        {
            const juce::String label = "Osc " + juce::String(oscillator + 1) + ": ";
            if (!result.ok() || !selectorFor(oscillator).loadUser(std::move(result.frames), name))
            {
                lastWavetableMessage =
                    label + (result.error.isNotEmpty() ? result.error : "The file is not usable");
            }
            else
            {
                auto* parameter = state.getParameter(
                    core::ParameterIds::oscillator(oscillator, core::ParameterIds::Oscillator::kWavetable));
                parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(static_cast<float>(core::WavetableSelector::userEntry())));
                selectorFor(oscillator).update();
                lastWavetableMessage = label + "loaded " + selectorFor(oscillator).userName();
            }
            wavetableBroadcaster.sendChangeMessage();
        });
}

void PluginProcessor::loadUserTockSample(const juce::File& file)
{
    wavetableLoader.loadTock(
        file,
        [this](core::TockImportResult result, juce::String name)
        {
            const juce::String label = "Tock: ";
            if (!result.ok() || !tockSelector.loadUser(std::move(result.sample), name))
            {
                lastWavetableMessage =
                    label + (result.error.isNotEmpty() ? result.error : "The file is not usable");
            }
            else
            {
                auto* parameter = state.getParameter(core::ParameterIds::kTockSample);
                parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(static_cast<float>(core::TockSelector::userEntry())));
                tockSelector.update();
                lastWavetableMessage = label + "loaded " + tockSelector.userName();
            }
            wavetableBroadcaster.sendChangeMessage();
        });
}

} // namespace fcm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fcm::PluginProcessor();
}

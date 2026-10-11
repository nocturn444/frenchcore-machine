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
      state(*this, nullptr, kStateType, core::createParameterLayout()), parameterReader(state)
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
    // The version is always written, whatever the tree held before (docs/specs/state-safe-loading.md).
    auto tree = state.copyState();
    tree.setProperty(kStateVersionProperty, core::kStateVersion, nullptr);

    // The envelope shapes are state that is not a parameter: they travel as child nodes.
    for (auto* envelope : {&ampEnvelope, &pitchEnvelope})
    {
        tree.removeChild(tree.getChildWithName(core::EnvelopeState::nodeType(envelope->kind())), nullptr);
        tree.appendChild(envelope->toValueTree(), nullptr);
    }

    // User wavetables travel as child nodes too, embedded so the project does not need the files.
    for (int i = 0; i < dsp::kNumOscillators; ++i)
    {
        auto node = selectorFor(i).toValueTree();
        if (node.isValid())
        {
            tree.removeChild(tree.getChildWithName(node.getType()), nullptr);
            tree.appendChild(node, nullptr);
        }
    }

    tree.removeChild(tree.getChildWithName(core::FxOrderState::nodeType()), nullptr);
    tree.appendChild(fxOrder.toValueTree(), nullptr);

    {
        auto node = tockSelector.toValueTree();
        if (node.isValid())
        {
            tree.removeChild(tree.getChildWithName(node.getType()), nullptr);
            tree.appendChild(node, nullptr);
        }
    }

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
    {
        const auto tree = juce::ValueTree::fromXml(*xml);
        if (core::loadState(state, tree, kStateType) == core::StateLoadResult::rejected)
            return;

        // A missing or damaged node gives the default shape instead of failing the whole load.
        ampEnvelope.restoreFrom(tree.getChildWithName(core::EnvelopeState::nodeType(dsp::EnvelopeKind::amp)));
        pitchEnvelope.restoreFrom(
            tree.getChildWithName(core::EnvelopeState::nodeType(dsp::EnvelopeKind::pitch)));

        fxOrder.restoreFrom(tree.getChildWithName(core::FxOrderState::nodeType()));
        tockSelector.restoreFrom(tree.getChildWithName("UserTockSample"));
        tockSelector.update();

        // User wavetables first, then the table of each current Wavetable value (the new parameter
        // values schedule that too; doing it now makes the load complete at once).
        for (int i = 0; i < dsp::kNumOscillators; ++i)
        {
            selectorFor(i).restoreFrom(tree.getChildWithName("UserWavetable" + juce::String(i + 1)));
            selectorFor(i).update();
        }
    }
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

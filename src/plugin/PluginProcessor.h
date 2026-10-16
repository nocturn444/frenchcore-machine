// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/AuditionState.h"
#include "core/EngineParameterReader.h"
#include "core/EnvelopeState.h"
#include "core/FxRackState.h"
#include "core/PresetManager.h"
#include "core/ProjectState.h"
#include "core/TempoState.h"
#include "core/TockSelector.h"
#include "core/WavetableLoader.h"
#include "core/WavetableSelector.h"
#include "dsp/engine/AuditionSequencer.h"
#include "dsp/engine/Engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm
{

class PluginProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::AsyncUpdater
{
  public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    // Presets are handled by our own preset system (docs/ARCHITECTURE.md §5), not host programs.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // The on-screen keyboard and auto-play exist only in the Standalone (docs/specs/standalone-audition.md).
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }
    core::AuditionState& getAuditionState() noexcept { return auditionState; }

    // The editable envelopes; the UI edits them through these (docs/specs/envelope-breakpoint.md).
    core::EnvelopeState& getAmpEnvelope() noexcept { return ampEnvelope; }
    // The tempo that synced envelopes follow, for the editor to show and convert with.
    // In the Standalone it is the BPM control itself, so it is right even before audio runs.
    float currentTempo() const noexcept { return isStandalone() ? auditionState.getBpm() : tempoState.get(); }
    core::EnvelopeState& getPitchEnvelope() noexcept { return pitchEnvelope; }
    core::FxRackState& getFxRacks() noexcept { return fxRacks; }
    core::PresetManager& getPresets() noexcept { return presetManager; }
    // The pitch the tail is on, in semitones from C3, for displays that follow the note.
    float currentNoteSemitones() const noexcept { return engine.currentNoteSemitones(); }

    core::WavetableSelector& selectorFor(int oscillator) noexcept
    {
        return oscillator == 0 ? wavetable1 : wavetable2;
    }

    // User wavetables (docs/specs/wavetable-user.md). Message thread. The file is read and decoded
    // in the background; on success the oscillator switches to the User entry, otherwise the
    // reason is kept in wavetableMessage(). Both announce through wavetableChanged.
    void loadUserWavetable(int oscillator, const juce::File& file);
    // The same for the Tock's user sample (docs/specs/tock.md §4.1); the message is shared.
    void loadUserTockSample(const juce::File& file);
    // The same for the noise oscillator's user sound (docs/specs/noise-oscillator.md).
    void loadUserNoiseSample(const juce::File& file);
    const core::NoiseSelector& noise() const noexcept { return noiseSelector; }
    const juce::String& wavetableMessage() const noexcept { return lastWavetableMessage; }
    juce::ChangeBroadcaster& wavetableChanged() noexcept { return wavetableBroadcaster; }

  private:
    juce::AudioProcessorValueTreeState state;
    core::EngineParameterReader parameterReader;
    core::TempoState tempoState;
    core::EnvelopeState ampEnvelope{dsp::EnvelopeKind::amp};
    core::EnvelopeState pitchEnvelope{dsp::EnvelopeKind::pitch};
    // After `state`: they listen to its parameters and must go first on destruction.
    core::WavetableSelector wavetable1{state, 0};
    core::WavetableSelector wavetable2{state, 1};
    core::TockSelector tockSelector{state};
    core::NoiseSelector noiseSelector{state};
    // Everything that is saved with the project, gathered (docs/specs/project-state.md).
    core::ProjectState project;
    // The preset list and the current preset (docs/specs/presets.md).
    core::PresetManager presetManager;

    juce::String lastWavetableMessage;
    juce::ChangeBroadcaster wavetableBroadcaster;
    dsp::Engine engine;
    // What is in each rack, which the engine reads (docs/specs/mixer-and-buses.md).
    core::FxRackState fxRacks{state,
                              dsp::defaultEffectRegistry(),
                              {&engine.rackValue(dsp::RackId::busA), &engine.rackValue(dsp::RackId::busB),
                               &engine.rackValue(dsp::RackId::main)}};

    // Standalone only. The host's MIDI is copied here together with the audition events; the
    // capacity is reserved in prepareToPlay so processBlock does not allocate.
    core::AuditionState auditionState;
    dsp::AuditionSequencer auditionSequencer;
    juce::MidiBuffer auditionMidi;

    void addAuditionEvents(int numSamples, const juce::MidiBuffer& hostMidi);

    // The chain's latency changes with Quality; the host is told on the message thread.
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void reportLatency();
    // The parameters for this block, with the tempo from the host (or the Standalone's BPM).
    dsp::EngineParameters readEngineParameters();

    // Last, so it is destroyed first: pending loads are then dropped instead of calling back.
    core::WavetableLoader wavetableLoader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace fcm

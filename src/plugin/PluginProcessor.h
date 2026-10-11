// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/AuditionState.h"
#include "core/EngineParameterReader.h"
#include "core/EnvelopeState.h"
#include "core/TempoState.h"
#include "dsp/engine/AuditionSequencer.h"
#include "dsp/engine/Engine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm
{

class PluginProcessor : public juce::AudioProcessor
{
  public:
    PluginProcessor();
    ~PluginProcessor() override = default;

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
    const core::TempoState& getTempoState() const noexcept { return tempoState; }
    core::EnvelopeState& getPitchEnvelope() noexcept { return pitchEnvelope; }

  private:
    juce::AudioProcessorValueTreeState state;
    core::EngineParameterReader parameterReader;
    core::TempoState tempoState;
    core::EnvelopeState ampEnvelope{dsp::EnvelopeKind::amp};
    core::EnvelopeState pitchEnvelope{dsp::EnvelopeKind::pitch};
    dsp::Engine engine;

    // Standalone only. The host's MIDI is copied here together with the audition events; the
    // capacity is reserved in prepareToPlay so processBlock does not allocate.
    core::AuditionState auditionState;
    dsp::AuditionSequencer auditionSequencer;
    juce::MidiBuffer auditionMidi;

    void addAuditionEvents(int numSamples, const juce::MidiBuffer& hostMidi);
    // The parameters for this block, with the tempo from the host (or the Standalone's BPM).
    dsp::EngineParameters readEngineParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace fcm

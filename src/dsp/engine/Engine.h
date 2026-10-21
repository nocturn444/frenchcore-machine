// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/BusStage.h"
#include "dsp/engine/EngineParameters.h"
#include "dsp/engine/Mixer.h"
#include "dsp/engine/NoteHandler.h"
#include "dsp/engine/PitchGlide.h"
#include "dsp/engine/Voice.h"
#include "dsp/envelope/EnvelopeShape.h"
#include "dsp/envelope/PublishedPointer.h"
#include "dsp/fx/EffectPool.h"
#include "dsp/fx/EffectRack.h"
#include "dsp/noise/NoiseSample.h"
#include "dsp/oscillator/WavetableData.h"
#include "dsp/tock/TockSample.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace fcm::dsp
{

// Top-level audio engine: turns MIDI notes into a kick through the voice and the output stage
// (docs/ARCHITECTURE.md §4, docs/specs/voice-note-handling.md §4.5). The FX chain joins between the
// voice and the output stage in Phase 5.
class Engine
{
  public:
    // The effects of the plugin; tests may pass a registry of their own.
    explicit Engine(const EffectRegistry& registry = defaultEffectRegistry()) : pool(registry) {}

    // May allocate. Called before playback starts and whenever the sample rate or block size changes.
    // Parameters set before this call take effect at once; later changes are smoothed.
    void prepare(const juce::dsp::ProcessSpec& spec);

    // Real-time safe. Returns all internal state to its initial value.
    void reset() noexcept;

    // Where the envelope shapes come from (ADR 0006). Call before playback; the publishers must
    // outlive the engine. Without them the engine plays the default shapes.
    void setEnvelopeSources(PublishedPointer<EnvelopeShape>* amp,
                            PublishedPointer<EnvelopeShape>* pitch) noexcept
    {
        ampSource = amp;
        pitchSource = pitch;
    }

    // Where each oscillator's wavetable comes from (ADR 0008). Call before playback; the publishers
    // must outlive the engine. Without them, or before the first publish, an oscillator plays the
    // built-in sine.
    void setWavetableSources(PublishedPointer<WavetableData>* osc1,
                             PublishedPointer<WavetableData>* osc2) noexcept
    {
        wavetableSources = {osc1, osc2};
    }

    // Where the Tock's sample comes from (docs/specs/tock.md). Call before playback; the publisher
    // must outlive the engine. Without it the Tock has no sample in Sample mode.
    void setTockSource(PublishedPointer<TockSample>* source) noexcept { tockSource = source; }

    // Where the noise oscillator's sound comes from (docs/specs/noise-oscillator.md). Call before
    // playback; the publisher must outlive the engine. Without it the noise is silent.
    void setNoiseSource(PublishedPointer<NoiseSample>* source) noexcept { noiseSource = source; }

    // Where an effect instance's drawn curve comes from (docs/decisions/0012 item 3): the pool index
    // (EffectRegistry::instanceIndex) and its publisher, which must outlive the engine. Call before
    // playback. Without a source an effect uses its default curve.
    void setEffectCurveSource(int instance, PublishedPointer<EffectCurve>* source) noexcept
    {
        pool.setCurveSource(instance, source);
    }

    // Real-time safe. Call once per block, before process().
    void setParameters(const EngineParameters& newParameters) noexcept;

    // Real-time safe. Renders one block into the buffer, consuming the MIDI events in it.
    void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }

    // The pitch the tail is on, in semitones from MIDI note 60, as of the last block: what the effects
    // that follow the note were given. Any thread may read it (the EQ display does).
    float currentNoteSemitones() const noexcept { return noteSemitonesNow.load(std::memory_order_relaxed); }

    // The contents of a rack (any thread may store a packed value, EffectRack::pack) and the
    // latency the effects add at the current Quality, for the host (docs/specs/fx-chain.md,
    // docs/decisions/0010).
    // Any thread: the next block's parameters take effect at once, without the ramps that smooth a
    // control moved by hand (a preset or project was just loaded).
    void requestSnap() noexcept { snapRequested.store(true); }

    std::atomic<std::uint64_t>& rackValue(RackId rack) noexcept
    {
        return racks[static_cast<size_t>(rack)].value();
    }
    // Every sound passes one rack-sized delay on its way to Main and one in the Main rack itself
    // (docs/decisions/0010 item 6), so the total is twice the pool's.
    int latencySamples(int oversamplingFactor) const noexcept
    {
        return 2 * pool.latencySamples(oversamplingFactor);
    }

  private:
    void handleMidi(const juce::MidiMessageMetadata& event) noexcept;
    // Renders `count` samples starting at `offset` of the current chunk into the mixer's sums.
    void renderStems(int offset, int count) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void updatePitchTarget() noexcept;

    double sampleRate = 44100.0;
    juce::uint32 maxBlockSize = 0;
    juce::uint32 numChannels = 0;

    EngineParameters parameters;
    bool snapParameters = true;

    std::unique_ptr<const WavetableData> sineTable;
    NoteHandler notes;
    EffectPool pool;
    std::array<EffectRack, kNumRacks> racks;
    Mixer mixer;
    std::array<BusStage, 2> buses;
    DelayRing mainDirectRing;
    // Per chunk (at most maxBlockSize samples): one mono sum per destination, the punch weight and
    // the stereo input of each bus rack.
    std::array<std::vector<float>, kNumDestinations> sums;
    std::vector<float> punchWeights;
    std::array<juce::AudioBuffer<float>, 2> busBuffers;
    PitchGlide glide;
    Voice voice;
    PublishedPointer<EnvelopeShape>* ampSource = nullptr;
    PublishedPointer<EnvelopeShape>* pitchSource = nullptr;
    std::array<PublishedPointer<WavetableData>*, kNumOscillators> wavetableSources{};
    PublishedPointer<TockSample>* tockSource = nullptr;
    PublishedPointer<NoiseSample>* noiseSource = nullptr;
    // The engine's own sample clock and the position of the recent note-ons on it, so the racks can
    // tell each effect the sample where a note's sound reaches it (docs/decisions/0012 item 1).
    std::int64_t sampleClock = 0;
    std::int64_t noteClock = 0; // the clock position of the MIDI event being handled
    NoteLog noteLog;
    int lastNote = -1; // the note the settling pitch follows when Key Track is on
    juce::SmoothedValue<float> outputGain{1.0f};
    std::atomic<float> noteSemitonesNow{0.0f};
    std::atomic<bool> snapRequested{false};
};

} // namespace fcm::dsp

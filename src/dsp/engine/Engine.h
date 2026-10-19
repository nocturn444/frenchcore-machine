// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"
#include "dsp/engine/NoteHandler.h"
#include "dsp/engine/PitchGlide.h"
#include "dsp/engine/Voice.h"
#include "dsp/envelope/EnvelopeShape.h"
#include "dsp/envelope/PublishedPointer.h"
#include "dsp/fx/EffectPool.h"
#include "dsp/fx/EffectRack.h"
#include "dsp/oscillator/WavetableData.h"
#include "dsp/tock/TockSample.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

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

    // Real-time safe. Call once per block, before process().
    void setParameters(const EngineParameters& newParameters) noexcept;

    // Real-time safe. Renders one block into the buffer, consuming the MIDI events in it.
    void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }

    // The contents of a rack (any thread may store a packed value, EffectRack::pack) and the
    // latency the effects add at the current Quality, for the host (docs/specs/fx-chain.md,
    // docs/decisions/0010).
    std::atomic<std::uint64_t>& rackValue(RackId rack) noexcept
    {
        return racks[static_cast<size_t>(rack)].value();
    }
    int latencySamples(int oversamplingFactor) const noexcept
    {
        return pool.latencySamples(oversamplingFactor);
    }

  private:
    void handleMidi(const juce::MidiMessageMetadata& event) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void updatePitchTarget() noexcept;
    void renderSegment(float* output, int numSamples) noexcept;

    double sampleRate = 44100.0;
    juce::uint32 maxBlockSize = 0;
    juce::uint32 numChannels = 0;

    EngineParameters parameters;
    bool snapParameters = true;

    std::unique_ptr<const WavetableData> sineTable;
    NoteHandler notes;
    EffectPool pool;
    std::array<EffectRack, kNumRacks> racks;
    PitchGlide glide;
    Voice voice;
    PublishedPointer<EnvelopeShape>* ampSource = nullptr;
    PublishedPointer<EnvelopeShape>* pitchSource = nullptr;
    std::array<PublishedPointer<WavetableData>*, kNumOscillators> wavetableSources{};
    PublishedPointer<TockSample>* tockSource = nullptr;
    int lastNote = -1; // the note the settling pitch follows when Key Track is on
    juce::SmoothedValue<float> outputGain{1.0f};
};

} // namespace fcm::dsp

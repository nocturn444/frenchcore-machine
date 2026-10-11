// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"
#include "dsp/engine/NoteHandler.h"
#include "dsp/engine/PitchGlide.h"
#include "dsp/engine/Voice.h"
#include "dsp/envelope/FixedAmpEnvelope.h"
#include "dsp/oscillator/WavetableData.h"

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
    // Longest time a kick keeps sounding after its note, for the host's tail length.
    static constexpr double kTailSeconds = FixedAmpEnvelope::kMaxLengthSeconds + 0.1;

    // May allocate. Called before playback starts and whenever the sample rate or block size changes.
    // Parameters set before this call take effect at once; later changes are smoothed.
    void prepare(const juce::dsp::ProcessSpec& spec);

    // Real-time safe. Returns all internal state to its initial value.
    void reset() noexcept;

    // Real-time safe. Call once per block, before process().
    void setParameters(const EngineParameters& newParameters) noexcept;

    // Real-time safe. Renders one block into the buffer, consuming the MIDI events in it.
    void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }

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
    PitchGlide glide;
    Voice voice;
    int lastNote = -1; // the note the settling pitch follows when Key Track is on
    juce::SmoothedValue<float> outputGain{1.0f};
};

} // namespace fcm::dsp

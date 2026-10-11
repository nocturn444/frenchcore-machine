// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace fcm::dsp
{

// Top-level audio engine: owns the voice, the FX chain and the output stage (see
// docs/ARCHITECTURE.md §4). Phase 1 skeleton: it only renders silence so the plugin can be built
// and validated on every platform before any DSP lands.
class Engine
{
  public:
    // May allocate. Called before playback starts and whenever the sample rate or block size changes.
    void prepare(const juce::dsp::ProcessSpec& spec);

    // Real-time safe. Returns all internal state to its initial value.
    void reset() noexcept;

    // Real-time safe. Renders one block into the buffer, consuming the MIDI events in it.
    void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }

  private:
    double sampleRate = 44100.0;
    juce::uint32 maxBlockSize = 0;
    juce::uint32 numChannels = 0;
};

} // namespace fcm::dsp

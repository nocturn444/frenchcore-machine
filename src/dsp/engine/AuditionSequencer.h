// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace fcm::dsp
{

// Repeats one note once per beat at a set tempo, as MIDI events at sample-accurate positions
// (docs/specs/standalone-audition.md §4). It only exists to let the Standalone play a loop without
// a DAW. Real-time safe: no allocation as long as the MidiBuffer has capacity.
class AuditionSequencer
{
  public:
    static constexpr float kMinBpm = 60.0f;
    static constexpr float kMaxBpm = 300.0f;
    static constexpr double kGateFraction = 0.8; // how much of a beat the note is held
    static constexpr int kVelocity = 100;
    static constexpr int kDefaultNote = 36;

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    // Back to the start: off, nothing sounding.
    void reset() noexcept
    {
        wasEnabled = false;
        sounding = false;
        soundingNote = note;
        untilNextOn = 0.0;
        untilOff = 0.0;
    }

    // These are cheap and are meant to be called once per block with the current settings.
    void setEnabled(bool shouldRun) noexcept { enabled = shouldRun; }
    void setBpm(float newBpm) noexcept { bpm = juce::jlimit(kMinBpm, kMaxBpm, newBpm); }
    void setNote(int newNote) noexcept { note = juce::jlimit(0, 127, newNote); }

    // Appends this block's events to `out`.
    void process(int numSamples, juce::MidiBuffer& out) noexcept;

  private:
    void addEvent(juce::MidiBuffer& out, const juce::MidiMessage& message, double position,
                  int numSamples) const;

    double sampleRate = 44100.0;
    bool enabled = false;
    float bpm = 200.0f;
    int note = kDefaultNote;

    bool wasEnabled = false;
    bool sounding = false;
    int soundingNote = kDefaultNote;
    double untilNextOn = 0.0; // samples, may be fractional
    double untilOff = 0.0;
};

} // namespace fcm::dsp

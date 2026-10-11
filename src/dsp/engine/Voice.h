// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"
#include "dsp/envelope/FixedAmpEnvelope.h"
#include "dsp/envelope/FixedPitchEnvelope.h"
#include "dsp/oscillator/WavetableOscillator.h"

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace fcm::dsp
{

// The monophonic kick voice: two wavetable oscillators following the pitch envelope and the
// settling pitch, times the amp envelope (docs/specs/voice-note-handling.md §4.3). Real-time safe
// except prepare().
class Voice
{
  public:
    // How long the previous kick keeps fading out when a note retriggers it.
    static constexpr double kRetriggerFadeMs = 1.5;
    // Kicks that can fade out at once. Notes closer together than the fade (a chord sent at one
    // instant, for example) each need their own slot to avoid a step.
    static constexpr int kMaxGhosts = 4;

    // The wavetable must outlive the voice.
    void prepare(double sampleRate, const WavetableData* wavetable);
    void reset() noexcept;

    // Called once per block. With `snap` the smoothed values jump to the new targets instead of
    // ramping (first call after prepare).
    void setParameters(const EngineParameters& parameters, bool snap) noexcept;

    // Starts a kick. `velocityGain` is the final level factor (already scaled by sensitivity).
    void trigger(float velocityGain) noexcept;

    // Renders one sample. `settleNote` is the pitch the kick settles on, in MIDI note numbers.
    float process(float settleNote) noexcept;

    bool isActive() const noexcept;

  private:
    // Everything that makes up one sounding kick. Plain data, so it can be copied into the ghost.
    struct Core
    {
        std::array<WavetableOscillator, kNumOscillators> oscillators;
        FixedAmpEnvelope amp;
        FixedPitchEnvelope pitch;
        float velocityGain = 1.0f;
        bool active = false;
        bool hasRendered = false; // false until the first sample: a kick that never sounded needs no fade
    };

    // A previous kick fading out after a retrigger.
    struct Ghost
    {
        Core core;
        float gain = 0.0f;
    };

    struct Frame
    {
        std::array<float, kNumOscillators> level;
        std::array<float, kNumOscillators> semitones;
    };

    float render(Core& voiceCore, float settleNote, const Frame& frame) noexcept;

    Core core;
    std::array<Ghost, kMaxGhosts> ghosts;
    float ghostStep = 0.0f;

    double sampleRate = 44100.0;
    std::array<float, kNumOscillators> startPhases{};
    std::array<juce::SmoothedValue<float>, kNumOscillators> levels;
    std::array<juce::SmoothedValue<float>, kNumOscillators> semitones;
    std::array<juce::SmoothedValue<float>, kNumOscillators> positions;
};

} // namespace fcm::dsp

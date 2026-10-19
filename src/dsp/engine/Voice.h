// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"
#include "dsp/envelope/EnvelopeRenderer.h"
#include "dsp/envelope/EnvelopeShape.h"
#include "dsp/noise/NoisePlayer.h"
#include "dsp/oscillator/WavetableOscillator.h"
#include "dsp/tock/Tock.h"

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

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
    // Width of the smooth change between the punch and the tail gain (docs/specs/voice-punch-tail.md).
    static constexpr double kPunchTailCrossfadeMs = 4.0;

    // The wavetable must outlive the voice.
    void prepare(double sampleRate, const WavetableData* wavetable);
    void reset() noexcept;

    // Called once per block. With `snap` the smoothed values jump to the new targets instead of
    // ramping (first call after prepare).
    void setParameters(const EngineParameters& parameters, bool snap) noexcept;

    // Called once per block with the shapes the audio thread currently reads; nullptr means the
    // default shape. The shapes must outlive the block.
    void setShapes(const EnvelopeShape* ampShape, const EnvelopeShape* pitchShape) noexcept;

    // Called once per block with the wavetable each oscillator reads this block (nullptr means the
    // table given to prepare()). The tables must outlive the block. Fading copies of the previous
    // kick are updated too, so none of them keeps a table older than the one acquired this block
    // (docs/decisions/0008-wavetable-selection-and-publishing.md).
    void setWavetables(const std::array<const WavetableData*, kNumOscillators>& tables) noexcept;

    // Called once per block with the Tock sample for this block (nullptr: none). Like the wavetables,
    // it is also set on the fading copies of the previous kick.
    void setTockSample(const TockSample* sample) noexcept;

    // Called once per block with the noise for this block (nullptr: none). Like the Tock sample, it is
    // also set on the fading copies of the previous kick.
    void setNoiseSample(const NoiseSample* sample) noexcept;

    // Starts a kick. `velocityGain` is the final level factor (already scaled by sensitivity).
    // `noteSemitones` is the played note relative to C3 (what Key Track of the Tock follows).
    void trigger(float velocityGain, float noteSemitones = 0.0f) noexcept;

    // The last held key was released: fades the current kick out with the envelope's short release.
    void release() noexcept { core.amp.release(); }

    using Stems = std::array<float, kNumStems>;

    // Renders one sample into one stem per sound (docs/specs/mixer-and-buses.md §4), each already
    // through its own level, the amp envelope, the velocity and the Punch / Tail levels, and the
    // punch weight: 1 in the punch part of the newest kick, 0 in the tail, with the same smooth
    // change at the Split as the levels. `settleNote` is the pitch the kick settles on, in MIDI
    // note numbers. The noise stem is silent until the noise oscillator exists.
    void processStems(float settleNote, Stems& stems, float& punchWeight) noexcept;

    // The sum of the stems: what the voice sounded like before the mixer existed.
    float process(float settleNote) noexcept;

    bool isActive() const noexcept;

  private:
    // Everything that makes up one sounding kick. Plain data, so it can be copied into the ghost.
    struct Core
    {
        std::array<WavetableOscillator, kNumOscillators> oscillators;
        EnvelopeRenderer amp;
        EnvelopeRenderer pitch;
        Tock tock;
        bool tockAmpEnv = false; // fixed at note-on
        NoisePlayer noise;
        float velocityGain = 1.0f;
        double samplesSinceNoteOn = 0.0; // for the punch / tail split
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
        float pitchAmount;
        float punchGain;
        float tailGain;
        float punchSplitMs;
        float tockLevel;
        float noiseLevel;
        float noisePitch;
    };

    void render(Core& voiceCore, float settleNote, const Frame& frame, float scale, Stems& stems) noexcept;

    Core core;
    std::array<Ghost, kMaxGhosts> ghosts;
    float ghostStep = 0.0f;
    double samplesSinceTrigger = 1.0e12; // of the newest kick, for the punch weight; counts while idle too

    double sampleRate = 44100.0;
    const WavetableData* fallbackTable = nullptr; // what prepare() was given
    std::array<float, kNumOscillators> startPhases{};
    std::array<juce::SmoothedValue<float>, kNumOscillators> levels;
    std::array<juce::SmoothedValue<float>, kNumOscillators> semitones;
    std::array<juce::SmoothedValue<float>, kNumOscillators> positions;
    juce::SmoothedValue<float> pitchAmount;
    juce::SmoothedValue<float> punchSplitMs;
    juce::SmoothedValue<float> punchLevel;
    juce::SmoothedValue<float> tailLevel;
    juce::SmoothedValue<float> tockLevel;
    juce::SmoothedValue<float> noiseLevel;
    juce::SmoothedValue<float> noisePitch;
    bool noiseOn = false;
    bool noiseKeyTrack = false;
    NoiseSettings noiseSettings;
    TockSettings tockSettings;
    bool tockOn = false;
    bool tockAmpEnv = false;
    bool tockKeyTrack = false;
    float bpm = 120.0f;
    const EnvelopeShape* ampShapeInUse = nullptr; // for the tempo sync scale at note-on
    const EnvelopeShape* pitchShapeInUse = nullptr;
    float ampTimeScale = 1.0f; // read at every note-on
    float pitchTimeScale = 1.0f;

    // Shapes for the Voice's own use when nothing is published (tests, and before the first publish).
    std::unique_ptr<const EnvelopeShape> defaultAmpShape;
    std::unique_ptr<const EnvelopeShape> defaultPitchShape;
};

} // namespace fcm::dsp

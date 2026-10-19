// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

inline constexpr int kNumOscillators = 2;

// The sounds of a kick that the mixer routes (docs/specs/mixer-and-buses.md): each is a mono stem
// that comes out of the voice already through its own level, the amp envelope and the Punch / Tail
// levels. The order is the order of the mixer's channels.
enum Stem
{
    stemOsc1,
    stemOsc2,
    stemNoise,
    stemTock,
    kNumStems
};

// Where a stem goes. The numbers are the entries of the Destination choice (append-only).
enum class Destination
{
    main = 0,
    busA = 1,
    busB = 2
};
inline constexpr int kNumDestinations = 3;

// How much of a bus's sound is processed by its effects (the entries of Apply To, append-only).
enum class BusRegion
{
    whole = 0,
    punch = 1,
    tail = 2
};

struct MixerSourceParameters
{
    Destination destination = Destination::main;
    bool mute = false;
    bool solo = false;
};

struct BusParameters
{
    float level = 1.0f; // linear gain
    BusRegion region = BusRegion::whole;
};

// The Tock, the click layer (docs/specs/tock.md). Everything but `level` is read at note-on.
struct TockParameters
{
    bool on = false;
    bool sampleMode = false;   // false: Synth, true: Sample
    float level = 0.25118864f; // linear gain, -12 dB
    float tuneSemitones = 0.0f;
    float startMs = 0.0f;
    float decayMs = 25.0f;  // time to fall by 60 dB
    float tone = 0.7f;      // 0 dull .. 1 open
    bool ampEnv = false;    // whether the amp envelope shapes the Tock (docs/specs/tock.md §8)
    float trimStart = 0.0f; // 0..1 of the sample
    float trimEnd = 1.0f;
    float pogoSemitones = 0.0f;
    bool keyTrack = false; // add the played note, relative to C3, to the Tune (docs/specs/tock.md section 9)
};

// The noise oscillator (docs/specs/noise-oscillator.md). Type and One Shot, Phase and Random are read at
// note-on; Level and Pitch move smoothly.
struct NoiseParameters
{
    bool on = false;
    float level = 1.0f; // linear gain
    float pitchSemitones = 0.0f;
    bool keyTrack = false;
    bool oneShot = false;
    float phase = 0.0f;  // 0..1 of the length
    float random = 1.0f; // 0..1
};

struct OscillatorParameters
{
    bool on = true;
    float level = 1.0f; // linear gain, 0..1
    int pitchSemitones = 0;
    int octave = 0;
    float fineCents = 0.0f;
    float position = 0.0f;   // 0..1 across the wavetable frames
    float startPhase = 0.0f; // cycles (0..1), applied at every note-on
};

// Plain values the plugin hands to the engine once per block, already converted to DSP units
// (docs/ARCHITECTURE.md §4.8, ADR 0005). Defaults match the parameter layout defaults.
struct EngineParameters
{
    EngineParameters() { oscillators[1].on = false; }

    std::array<OscillatorParameters, kNumOscillators> oscillators;

    bool keyTrack = true;
    int rootNote = 36;
    bool glideOn = false;
    float glideMs = 100.0f;
    float velocitySensitivity = 0.0f; // 0..1: every kick equally loud by default
    float outputGain = 1.0f;          // linear
    bool limiterOn = true;

    // Envelopes and Punch / Tail (docs/specs/envelope-breakpoint.md, voice-punch-tail.md).
    float ampTimeScale = 1.0f;
    float pitchTimeScale = 1.0f;
    float pitchAmount = 48.0f;  // semitones above the settling pitch at envelope level 1
    float punchSplitMs = 80.0f; // where the punch ends and the tail begins, after note-on
    float punchLevel = 1.0f;    // linear gain of the punch region (before the split)
    float tailLevel = 1.0f;     // linear gain of the tail region (after the split)

    TockParameters tock;
    NoiseParameters noise;

    // The mixer and the two buses (docs/specs/mixer-and-buses.md).
    std::array<MixerSourceParameters, kNumStems> mixer;
    std::array<BusParameters, 2> buses; // Bus A, Bus B

    // The effects (docs/specs/fx-chain.md): the state of every effect instance by pool index
    // (docs/decisions/0010), and the oversampling factor (1, 2, 4 or 8) of the global Quality setting.
    std::array<EffectSlotParameters, kMaxInstances> effects;
    int oversamplingFactor = 2;

    // The tempo that tempo-synced envelopes follow (host tempo, or the Standalone's BPM).
    float bpm = 120.0f;
};

} // namespace fcm::dsp

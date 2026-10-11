// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <array>

namespace fcm::dsp
{

inline constexpr int kNumOscillators = 2;

// The Tock, the click layer (docs/specs/tock.md). Everything but `level` is read at note-on.
struct TockParameters
{
    bool on = false;
    bool sampleMode = false;   // false: Synth, true: Sample
    float level = 0.25118864f; // linear gain, -12 dB
    float tuneSemitones = 0.0f;
    float startMs = 0.0f;
    float decayMs = 25.0f; // time to fall by 60 dB
    float tone = 0.7f;     // 0 dull .. 1 open
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
    float punchBalance = 0.0f;  // -1 (punch only) .. +1 (tail only)
    float punchSplitMs = 80.0f; // where the punch ends and the tail begins, after note-on
    float punchLevel = 1.0f;    // linear gain of the punch region, on top of the balance
    float tailLevel = 1.0f;     // linear gain of the tail region, on top of the balance

    TockParameters tock;

    // The FX chain (docs/specs/fx-chain.md): the state of every registered effect by index, and the
    // oversampling factor (1, 2, 4 or 8) of the global Quality setting.
    std::array<EffectSlotParameters, kMaxEffects> effects;
    int oversamplingFactor = 2;

    // The tempo that tempo-synced envelopes follow (host tempo, or the Standalone's BPM).
    float bpm = 120.0f;
};

} // namespace fcm::dsp

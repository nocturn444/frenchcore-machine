// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>

namespace fcm::dsp
{

inline constexpr int kNumOscillators = 2;

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
};

} // namespace fcm::dsp

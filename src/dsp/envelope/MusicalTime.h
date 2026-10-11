// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <string>
#include <vector>

namespace fcm::dsp
{

// Note values for the tempo-synced envelope knobs (docs/specs/envelope-breakpoint.md, "Tempo
// sync"). Lengths are in quarter-note beats: 1 is a quarter note, 4 a bar of 4/4.
struct MusicalFigure
{
    const char* name;
    double beats;
};

// T is a triplet (2/3 of the straight value), D is dotted (3/2 of it). Sorted by length.
inline constexpr std::array<MusicalFigure, 14> kMusicalFigures{{
    {"1/64", 0.0625},
    {"1/32", 0.125},
    {"1/16T", 1.0 / 6.0},
    {"1/16", 0.25},
    {"1/8T", 1.0 / 3.0},
    {"1/16D", 0.375},
    {"1/8", 0.5},
    {"1/4T", 2.0 / 3.0},
    {"1/8D", 0.75},
    {"1/4", 1.0},
    {"1/2T", 4.0 / 3.0},
    {"1/4D", 1.5},
    {"1/2", 2.0},
    {"1 bar", 4.0},
}};

inline constexpr double kMinTempoBpm = 20.0;
inline constexpr double kMaxTempoBpm = 400.0;

inline double beatsToMs(double beats, double bpm) noexcept
{
    return beats * 60000.0 / bpm;
}

inline double msToBeats(double ms, double bpm) noexcept
{
    return ms * bpm / 60000.0;
}

// One step of a synced time knob.
struct TimeChoice
{
    std::string label;
    float ms;
};

// The steps a knob with the range [minMs, maxMs] offers at a tempo: `0` when it can be zero,
// `Min` (the shortest time) when it cannot, then every figure that fits the range.
std::vector<TimeChoice> timeChoices(float minMs, float maxMs, double bpm);

// Index of the choice closest to `ms` (by absolute difference).
int nearestChoice(const std::vector<TimeChoice>& choices, float ms);

// A position on the time grid as a fraction of a bar: "1/16", "1/8", "3/16", ... "1", "0".
std::string formatBarFraction(double beats);

} // namespace fcm::dsp

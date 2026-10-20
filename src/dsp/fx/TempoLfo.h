// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cmath>
#include <string_view>

namespace fcm::dsp
{

// The note values of the synced Rate of the Phaser, the Flanger and the Volume LFO. It is a frozen
// copy of the envelopes' figures (kMusicalFigures): the saved value of a `division` parameter is the
// entry number, so a later change of the envelopes' list must not change what an effect saved
// (docs/decisions/0012 item 2). Append-only; lengths in quarter-note beats (4 is a bar of 4/4).
struct LfoDivision
{
    std::string_view name;
    double beats;
};

inline constexpr std::array<LfoDivision, 14> kLfoDivisions{{
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

// The names as a choice list for ParameterSpec::choices.
inline constexpr std::array<std::string_view, 14> kLfoDivisionNames{
    kLfoDivisions[0].name,  kLfoDivisions[1].name, kLfoDivisions[2].name,  kLfoDivisions[3].name,
    kLfoDivisions[4].name,  kLfoDivisions[5].name, kLfoDivisions[6].name,  kLfoDivisions[7].name,
    kLfoDivisions[8].name,  kLfoDivisions[9].name, kLfoDivisions[10].name, kLfoDivisions[11].name,
    kLfoDivisions[12].name, kLfoDivisions[13].name};

// An LFO phase (0 to 1) that restarts at every note (docs/decisions/0012 item 2): it runs at `Rate`
// Hz, or at one cycle per musical figure at the tempo when Sync is on, and `restart()` puts it back
// at 0. Rate 0 holds the phase. Plain data, no allocation; the tempo is read once per block by the
// effect (EffectContext::tempoBpm), not the DAW's bar position, so no host transport is needed.
class TempoLfo
{
  public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept { phaseValue = 0.0; }

    // Back to the start of the cycle (the next value() is 0).
    void restart() noexcept { phaseValue = 0.0; }

    void setRateHz(double hz) noexcept
    {
        const double clamped = std::isfinite(hz) ? hz : 0.0;
        increment = (clamped > 0.0 ? clamped : 0.0) / sampleRate;
    }

    // One cycle per `figure` (an index of kLfoDivisions) at `bpm`.
    void setSynced(int figure, double bpm) noexcept
    {
        const int index = figure < 0 ? 0
                                     : (figure >= static_cast<int>(kLfoDivisions.size())
                                            ? static_cast<int>(kLfoDivisions.size()) - 1
                                            : figure);
        const double tempo = std::isfinite(bpm) && bpm > 0.0 ? bpm : 120.0;
        const double seconds = kLfoDivisions[static_cast<size_t>(index)].beats * 60.0 / tempo;
        increment = 1.0 / (seconds * sampleRate);
    }

    // The phase now.
    float value() const noexcept { return static_cast<float>(phaseValue); }

    // Moves on by one sample.
    void advance() noexcept
    {
        phaseValue += increment;
        if (phaseValue >= 1.0)
            phaseValue -= std::floor(phaseValue);
    }

    // The phase now, then one step on: the usual one-call-per-sample use.
    float next() noexcept
    {
        const float current = value();
        advance();
        return current;
    }

  private:
    double sampleRate = 44100.0;
    double phaseValue = 0.0;
    double increment = 0.0;
};

} // namespace fcm::dsp

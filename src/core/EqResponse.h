// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// The EQ's settings as the display needs them, read from the parameters (docs/specs/fx-eq.md §4).
// The curve is the sum, in dB, of the active bands at a nominal 48 kHz sample rate.
struct EqBand
{
    bool on = true;
    int type = 0; // the entry of the Type choice: Bell, Low Shelf, High Shelf, Low Cut, High Cut
    double freq = 1000.0;
    double gain = 0.0;
    double q = 1.0;

    bool isCut() const noexcept { return type == 3 || type == 4; }
    // Whether the band changes the sound at all (same rule as the effect itself).
    bool isActive() const noexcept;
};

inline constexpr int kEqBands = 4;
using EqBands = std::array<EqBand, kEqBands>;

// Reads "eq.b1.*" ... "eq.b4.*". A missing parameter keeps the EqBand default.
EqBands readEqBands(juce::AudioProcessorValueTreeState& state);

// The display's axes: 20 Hz to 20 kHz on a logarithmic scale, +-18 dB, both as a position from 0 to 1
// (frequency from the left, gain from the bottom). Unusable input gives the lowest end.
inline constexpr double kEqMinHz = 20.0;
inline constexpr double kEqMaxHz = 20000.0;
inline constexpr double kEqRangeDb = 18.0;
double eqFrequencyToUnit(double hz) noexcept;
double eqUnitToFrequency(double unit) noexcept;
double eqGainToUnit(double db) noexcept;
double eqUnitToGain(double unit) noexcept;

// The response of one band, or of all of them, at `hz`, in dB.
double eqBandResponseDb(const EqBand& band, double hz) noexcept;
double eqResponseDb(const EqBands& bands, double hz) noexcept;

} // namespace fcm::core

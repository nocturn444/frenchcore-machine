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
    double gain = 0.0; // what the band uses: Gain, or Extreme Gain when Extreme is on
    double q = 1.0;    // likewise
    bool extreme = false;
    double keyTrack = 0.0; // percent

    bool isCut() const noexcept { return type == 3 || type == 4; }
    // Whether the band changes the sound at all (same rule as the effect itself).
    bool isActive() const noexcept;

    // The frequency the band really has for the note the tail is on (semitones from C3), which is
    // `freq` unless Key Track is used.
    double frequencyAt(double noteSemitones) const noexcept;
};

inline constexpr int kEqBands = 4;
using EqBands = std::array<EqBand, kEqBands>;

// Reads "eq.b1.*" ... "eq.b4.*". A missing parameter keeps the EqBand default.
EqBands readEqBands(juce::AudioProcessorValueTreeState& state, const juce::String& instanceId = "eq");

// The display's axes: 20 Hz to 20 kHz on a logarithmic scale, +-18 dB, both as a position from 0 to 1
// (frequency from the left, gain from the bottom). Unusable input gives the lowest end.
inline constexpr double kEqMinHz = 20.0;
inline constexpr double kEqMaxHz = 20000.0;
inline constexpr double kEqRangeDb = 18.0;
inline constexpr double kEqExtremeRangeDb = 36.0; // when any band is Extreme
double eqFrequencyToUnit(double hz) noexcept;
double eqUnitToFrequency(double unit) noexcept;
double eqGainToUnit(double db, double rangeDb = kEqRangeDb) noexcept;
double eqUnitToGain(double unit, double rangeDb = kEqRangeDb) noexcept;

// The half-height of the display for these bands: 36 dB when any band is Extreme, else 18 dB.
double eqDisplayRangeDb(const EqBands& bands) noexcept;

// The response of one band, or of all of them, at `hz`, in dB, with the bands at the frequencies they
// have for the note (semitones from C3; 0 when no note is playing).
double eqBandResponseDb(const EqBand& band, double hz, double noteSemitones = 0.0) noexcept;
double eqResponseDb(const EqBands& bands, double hz, double noteSemitones = 0.0) noexcept;

} // namespace fcm::core

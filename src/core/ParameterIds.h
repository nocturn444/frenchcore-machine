// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <string>

namespace fcm::core
{

// Version of the saved plugin state. Bump it and add a migration in StateMigrations when the
// state format changes (docs/ARCHITECTURE.md §5).
inline constexpr int kStateVersion = 1;

// Parameter IDs are permanent once released: never rename, remove or reuse one.
namespace ParameterIds
{
inline constexpr const char* kOutputGain = "global.outputGain";
inline constexpr const char* kLimiterOn = "global.limiterOn";
inline constexpr const char* kKeyTrack = "global.keyTrack";
inline constexpr const char* kRootNote = "global.rootNote";
inline constexpr const char* kGlideOn = "global.glideOn";
inline constexpr const char* kGlideTime = "global.glideTime";
inline constexpr const char* kVelocitySensitivity = "global.velocitySensitivity";

// Envelope globals and Punch / Tail (docs/specs/envelope-breakpoint.md, voice-punch-tail.md).
// The envelope shapes themselves are state, not parameters.
inline constexpr const char* kAmpEnvTimeScale = "ampEnv.timeScale";
inline constexpr const char* kPitchEnvTimeScale = "pitchEnv.timeScale";
inline constexpr const char* kPitchEnvAmount = "pitchEnv.amount";
inline constexpr const char* kPunchTailBalance = "punchTail.balance";
inline constexpr const char* kPunchTailSplit = "punchTail.split";

// The two oscillators are instances of the same class, so their parameters share these suffixes
// behind the prefix "osc1." / "osc2." (docs/specs/oscillator-wavetable.md §3).
inline constexpr std::array<const char*, 2> kOscillatorPrefixes{"osc1", "osc2"};

namespace Oscillator
{
inline constexpr const char* kOn = "on";
inline constexpr const char* kLevel = "level";
inline constexpr const char* kPitch = "pitch";
inline constexpr const char* kOctave = "octave";
inline constexpr const char* kFine = "fine";
inline constexpr const char* kPosition = "position";
inline constexpr const char* kPhase = "phase";
} // namespace Oscillator

// "osc1.level" for (0, Oscillator::kLevel).
inline std::string oscillator(int index, const char* suffix)
{
    return std::string(kOscillatorPrefixes[static_cast<size_t>(index)]) + "." + suffix;
}
} // namespace ParameterIds

} // namespace fcm::core

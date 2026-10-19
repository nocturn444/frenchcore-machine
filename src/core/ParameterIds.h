// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <string>
#include <string_view>

namespace fcm::core
{

// Version of the saved plugin state. Bump it and add a migration in StateMigrations when the
// state format changes (docs/ARCHITECTURE.md §5).
inline constexpr int kStateVersion = 2;

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
inline constexpr const char* kQuality = "global.quality"; // oversampling: 1x, 2x, 4x, 8x

// Every effect adds "<id>.enabled" and "<id>.mix"; its own parameters are "<id>.<suffix>"
// (docs/specs/fx-chain.md). These are the suffixes of the two common ones.
inline constexpr const char* kEffectEnabled = "enabled";
inline constexpr const char* kEffectMix = "mix";

// Envelope globals and Punch / Tail (docs/specs/envelope-breakpoint.md, voice-punch-tail.md).
// The envelope shapes themselves are state, not parameters.
inline constexpr const char* kAmpEnvTimeScale = "ampEnv.timeScale";
inline constexpr const char* kPitchEnvTimeScale = "pitchEnv.timeScale";
inline constexpr const char* kPitchEnvAmount = "pitchEnv.amount";
inline constexpr const char* kPunchTailSplit = "punchTail.split";
inline constexpr const char* kPunchLevel = "punchTail.punchLevel";
inline constexpr const char* kTailLevel = "punchTail.tailLevel";

// The Tock (docs/specs/tock.md).
inline constexpr const char* kTockOn = "tock.on";
inline constexpr const char* kTockMode = "tock.mode";
inline constexpr const char* kTockSample = "tock.sample";
inline constexpr const char* kTockLevel = "tock.level";
inline constexpr const char* kTockTune = "tock.tune";
inline constexpr const char* kTockStart = "tock.start";
inline constexpr const char* kTockDecay = "tock.decay";
inline constexpr const char* kTockTone = "tock.tone";

// "distortion.enabled" for ("distortion", kEffectEnabled). `effectId` is an instance ID: the effect's
// ID for the first copy ("distortion"), the ID plus the copy number for the others ("distortion2"),
// see EffectRegistry::instanceId and docs/decisions/0010.
inline std::string effect(std::string_view effectId, std::string_view suffix)
{
    return std::string(effectId) + "." + std::string(suffix);
}

// The two oscillators are instances of the same class, so their parameters share these suffixes
// behind the prefix "osc1." / "osc2." (docs/specs/oscillator-wavetable.md §3).
inline constexpr std::array<const char*, 2> kOscillatorPrefixes{"osc1", "osc2"};

namespace Oscillator
{
inline constexpr const char* kOn = "on";
inline constexpr const char* kWavetable = "wavetable";
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

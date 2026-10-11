// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EngineParameterReader.h"

#include "core/ParameterIds.h"

#include <cmath>

namespace fcm::core
{

namespace
{
std::atomic<float>* lookup(juce::AudioProcessorValueTreeState& state, const std::string& parameterId)
{
    auto* pointer = state.getRawParameterValue(parameterId);
    jassert(pointer != nullptr); // layout and reader out of sync
    return pointer;
}

// A value from a parameter, or `fallback` when it is not a finite number: a damaged project must
// never push a NaN or an infinity into the engine (docs/specs/state-safe-loading.md).
float finiteOr(const std::atomic<float>* value, float fallback) noexcept
{
    const float number = value->load();
    return std::isfinite(number) ? number : fallback;
}

bool toBool(const std::atomic<float>* value, bool fallback) noexcept
{
    return finiteOr(value, fallback ? 1.0f : 0.0f) >= 0.5f;
}

int toInt(const std::atomic<float>* value, int fallback) noexcept
{
    // Clamped first: converting a huge float to an integer is undefined.
    const float number = juce::jlimit(-1.0e6f, 1.0e6f, finiteOr(value, static_cast<float>(fallback)));
    return static_cast<int>(std::lround(number));
}
} // namespace

EngineParameterReader::EngineParameterReader(juce::AudioProcessorValueTreeState& state)
    : outputGain(lookup(state, ParameterIds::kOutputGain)),
      limiterOn(lookup(state, ParameterIds::kLimiterOn)), keyTrack(lookup(state, ParameterIds::kKeyTrack)),
      rootNote(lookup(state, ParameterIds::kRootNote)), glideOn(lookup(state, ParameterIds::kGlideOn)),
      glideTime(lookup(state, ParameterIds::kGlideTime)),
      velocitySensitivity(lookup(state, ParameterIds::kVelocitySensitivity)),
      ampTimeScale(lookup(state, ParameterIds::kAmpEnvTimeScale)),
      pitchTimeScale(lookup(state, ParameterIds::kPitchEnvTimeScale)),
      pitchAmount(lookup(state, ParameterIds::kPitchEnvAmount)),
      punchBalance(lookup(state, ParameterIds::kPunchTailBalance)),
      punchSplit(lookup(state, ParameterIds::kPunchTailSplit)),
      punchLevel(lookup(state, ParameterIds::kPunchLevel)), tailLevel(lookup(state, ParameterIds::kTailLevel))
{
    namespace Osc = ParameterIds::Oscillator;
    for (int index = 0; index < dsp::kNumOscillators; ++index)
    {
        oscillators[static_cast<size_t>(index)] = {
            lookup(state, ParameterIds::oscillator(index, Osc::kOn)),
            lookup(state, ParameterIds::oscillator(index, Osc::kLevel)),
            lookup(state, ParameterIds::oscillator(index, Osc::kPitch)),
            lookup(state, ParameterIds::oscillator(index, Osc::kOctave)),
            lookup(state, ParameterIds::oscillator(index, Osc::kFine)),
            lookup(state, ParameterIds::oscillator(index, Osc::kPosition)),
            lookup(state, ParameterIds::oscillator(index, Osc::kPhase)),
        };
    }
}

dsp::EngineParameters EngineParameterReader::read() const noexcept
{
    const dsp::EngineParameters defaults;
    dsp::EngineParameters result;

    // The default gain is 0 dB; the other fields fall back to the engine's own defaults.
    result.outputGain = juce::Decibels::decibelsToGain(finiteOr(outputGain, 0.0f), -48.0f);
    result.limiterOn = toBool(limiterOn, defaults.limiterOn);
    result.keyTrack = toBool(keyTrack, defaults.keyTrack);
    result.rootNote = toInt(rootNote, defaults.rootNote);
    result.glideOn = toBool(glideOn, defaults.glideOn);
    result.glideMs = finiteOr(glideTime, defaults.glideMs);
    result.velocitySensitivity = finiteOr(velocitySensitivity, defaults.velocitySensitivity * 100.0f) * 0.01f;
    result.ampTimeScale = finiteOr(ampTimeScale, defaults.ampTimeScale);
    result.pitchTimeScale = finiteOr(pitchTimeScale, defaults.pitchTimeScale);
    result.pitchAmount = finiteOr(pitchAmount, defaults.pitchAmount);
    result.punchBalance = finiteOr(punchBalance, defaults.punchBalance * 100.0f) * 0.01f;
    result.punchSplitMs = finiteOr(punchSplit, defaults.punchSplitMs);
    result.punchLevel = juce::Decibels::decibelsToGain(finiteOr(punchLevel, 0.0f), -48.0f);
    result.tailLevel = juce::Decibels::decibelsToGain(finiteOr(tailLevel, 0.0f), -48.0f);

    for (size_t i = 0; i < oscillators.size(); ++i)
    {
        const auto& source = oscillators[i];
        const auto& fallback = defaults.oscillators[i];
        auto& target = result.oscillators[i];
        target.on = toBool(source.on, fallback.on);
        target.level = finiteOr(source.level, fallback.level);
        target.pitchSemitones = toInt(source.pitch, fallback.pitchSemitones);
        target.octave = toInt(source.octave, fallback.octave);
        target.fineCents = finiteOr(source.fine, fallback.fineCents);
        target.position = finiteOr(source.position, fallback.position);
        target.startPhase = finiteOr(source.phase, fallback.startPhase * 360.0f) / 360.0f;
    }

    return result;
}

} // namespace fcm::core

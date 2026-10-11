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

bool toBool(const std::atomic<float>* value) noexcept
{
    return value->load() >= 0.5f;
}

int toInt(const std::atomic<float>* value) noexcept
{
    return static_cast<int>(std::lround(value->load()));
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
      punchSplit(lookup(state, ParameterIds::kPunchTailSplit))
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
    dsp::EngineParameters result;

    result.outputGain = juce::Decibels::decibelsToGain(outputGain->load(), -48.0f);
    result.limiterOn = toBool(limiterOn);
    result.keyTrack = toBool(keyTrack);
    result.rootNote = toInt(rootNote);
    result.glideOn = toBool(glideOn);
    result.glideMs = glideTime->load();
    result.velocitySensitivity = velocitySensitivity->load() * 0.01f;
    result.ampTimeScale = ampTimeScale->load();
    result.pitchTimeScale = pitchTimeScale->load();
    result.pitchAmount = pitchAmount->load();
    result.punchBalance = punchBalance->load() * 0.01f;
    result.punchSplitMs = punchSplit->load();

    for (size_t i = 0; i < oscillators.size(); ++i)
    {
        const auto& source = oscillators[i];
        auto& target = result.oscillators[i];
        target.on = toBool(source.on);
        target.level = source.level->load();
        target.pitchSemitones = toInt(source.pitch);
        target.octave = toInt(source.octave);
        target.fineCents = source.fine->load();
        target.position = source.position->load();
        target.startPhase = source.phase->load() / 360.0f;
    }

    return result;
}

} // namespace fcm::core

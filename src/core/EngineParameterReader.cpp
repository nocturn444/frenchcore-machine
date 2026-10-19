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

EngineParameterReader::EngineParameterReader(juce::AudioProcessorValueTreeState& state,
                                             const dsp::EffectRegistry& registry)
    : quality(lookup(state, ParameterIds::kQuality)), outputGain(lookup(state, ParameterIds::kOutputGain)),
      limiterOn(lookup(state, ParameterIds::kLimiterOn)), keyTrack(lookup(state, ParameterIds::kKeyTrack)),
      rootNote(lookup(state, ParameterIds::kRootNote)), glideOn(lookup(state, ParameterIds::kGlideOn)),
      glideTime(lookup(state, ParameterIds::kGlideTime)),
      velocitySensitivity(lookup(state, ParameterIds::kVelocitySensitivity)),
      noise{lookup(state, ParameterIds::kNoiseOn),      lookup(state, ParameterIds::kNoiseLevel),
            lookup(state, ParameterIds::kNoisePitch),   lookup(state, ParameterIds::kNoiseKeyTrack),
            lookup(state, ParameterIds::kNoiseOneShot), lookup(state, ParameterIds::kNoisePhase),
            lookup(state, ParameterIds::kNoiseRandom)},
      tockOn(lookup(state, ParameterIds::kTockOn)), tockMode(lookup(state, ParameterIds::kTockMode)),
      tockLevel(lookup(state, ParameterIds::kTockLevel)), tockTune(lookup(state, ParameterIds::kTockTune)),
      tockStart(lookup(state, ParameterIds::kTockStart)), tockDecay(lookup(state, ParameterIds::kTockDecay)),
      tockTone(lookup(state, ParameterIds::kTockTone)),
      ampTimeScale(lookup(state, ParameterIds::kAmpEnvTimeScale)),
      pitchTimeScale(lookup(state, ParameterIds::kPitchEnvTimeScale)),
      pitchAmount(lookup(state, ParameterIds::kPitchEnvAmount)),
      punchSplit(lookup(state, ParameterIds::kPunchTailSplit)),
      punchLevel(lookup(state, ParameterIds::kPunchLevel)), tailLevel(lookup(state, ParameterIds::kTailLevel))
{
    // One entry per pool instance, in pool order (docs/decisions/0010).
    for (int instance = 0; instance < registry.instanceCount(); ++instance)
    {
        const std::string instanceId = registry.instanceId(instance);
        const int index = dsp::EffectRegistry::effectOf(instance);
        EffectPointers pointers;
        pointers.enabled = lookup(state, ParameterIds::effect(instanceId, ParameterIds::kEffectEnabled));
        pointers.mix = lookup(state, ParameterIds::effect(instanceId, ParameterIds::kEffectMix));
        pointers.defaultMixPercent = registry.descriptor(index)->defaultMix * 100.0f;
        for (const auto& spec : registry.parameters(index))
        {
            pointers.values.push_back(lookup(state, ParameterIds::effect(instanceId, spec.idSuffix)));
            pointers.defaults.push_back(spec.defaultValue);
        }
        effects.push_back(std::move(pointers));
    }

    for (size_t i = 0; i < ParameterIds::kMixerSources.size(); ++i)
    {
        const auto& source = ParameterIds::kMixerSources[i];
        mixer[i] = {lookup(state, ParameterIds::mixerDestination(source)),
                    lookup(state, ParameterIds::mixerMute(source)),
                    lookup(state, ParameterIds::mixerSolo(source))};
    }
    for (int bus = 0; bus < 2; ++bus)
        buses[static_cast<size_t>(bus)] = {lookup(state, ParameterIds::busLevel(bus)),
                                           lookup(state, ParameterIds::busRegion(bus))};

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
    result.punchSplitMs = finiteOr(punchSplit, defaults.punchSplitMs);
    result.punchLevel = juce::Decibels::decibelsToGain(finiteOr(punchLevel, 0.0f), -48.0f);
    result.tailLevel = juce::Decibels::decibelsToGain(finiteOr(tailLevel, 0.0f), -48.0f);

    // The noise: its Level is in dB, the rest is plain; Phase and Random are percentages.
    result.noise.on = toBool(noise.on, defaults.noise.on);
    result.noise.level = juce::Decibels::decibelsToGain(finiteOr(noise.level, 0.0f), -48.0f);
    result.noise.pitchSemitones = finiteOr(noise.pitch, defaults.noise.pitchSemitones);
    result.noise.keyTrack = toBool(noise.keyTrack, defaults.noise.keyTrack);
    result.noise.oneShot = toBool(noise.oneShot, defaults.noise.oneShot);
    result.noise.phase = juce::jlimit(0.0f, 1.0f, finiteOr(noise.phase, 0.0f) * 0.01f);
    result.noise.random = juce::jlimit(0.0f, 1.0f, finiteOr(noise.random, 100.0f) * 0.01f);

    // The Tock's level is in dB (-12 by default), like the other levels.
    result.tock.on = toBool(tockOn, defaults.tock.on);
    result.tock.sampleMode = toInt(tockMode, 0) >= 1;
    result.tock.level = juce::Decibels::decibelsToGain(finiteOr(tockLevel, -12.0f), -48.0f);
    result.tock.tuneSemitones = static_cast<float>(toInt(tockTune, 0));
    result.tock.startMs = finiteOr(tockStart, defaults.tock.startMs);
    result.tock.decayMs = finiteOr(tockDecay, defaults.tock.decayMs);
    result.tock.tone = finiteOr(tockTone, defaults.tock.tone);

    // The mixer: a destination or region that is not one of the entries goes to the first (Main, Whole).
    for (size_t i = 0; i < mixer.size(); ++i)
    {
        auto& target = result.mixer[static_cast<size_t>(ParameterIds::kMixerSources[i].stem)];
        const int destination = toInt(mixer[i].destination, 0);
        target.destination = destination >= 0 && destination < dsp::kNumDestinations
                                 ? static_cast<dsp::Destination>(destination)
                                 : dsp::Destination::main;
        target.mute = toBool(mixer[i].mute, false);
        target.solo = toBool(mixer[i].solo, false);
    }
    for (size_t b = 0; b < buses.size(); ++b)
    {
        result.buses[b].level = juce::Decibels::decibelsToGain(finiteOr(buses[b].level, 0.0f), -48.0f);
        const int region = toInt(buses[b].region, 0);
        result.buses[b].region =
            region >= 0 && region <= 2 ? static_cast<dsp::BusRegion>(region) : dsp::BusRegion::whole;
    }

    // The effect instances: a value that is not a finite number becomes the effect's default.
    for (size_t i = 0; i < effects.size() && i < result.effects.size(); ++i)
    {
        const auto& source = effects[i];
        auto& target = result.effects[i];
        target.enabled = toBool(source.enabled, false);
        target.mix = juce::jlimit(0.0f, 1.0f, finiteOr(source.mix, source.defaultMixPercent) * 0.01f);
        for (size_t p = 0; p < source.values.size() && p < target.values.size(); ++p)
            target.values[p] = finiteOr(source.values[p], source.defaults[p]);
    }
    result.oversamplingFactor = 1 << juce::jlimit(0, 3, toInt(quality, 1));

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

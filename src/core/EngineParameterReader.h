// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"
#include "dsp/fx/EffectRegistry.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace fcm::core
{

// Turns the APVTS parameters into the plain struct the engine consumes (ADR 0005). The raw
// parameter pointers are looked up once at construction, so read() is real-time safe.
class EngineParameterReader
{
  public:
    explicit EngineParameterReader(juce::AudioProcessorValueTreeState& state,
                                   const dsp::EffectRegistry& registry = dsp::defaultEffectRegistry());

    dsp::EngineParameters read() const noexcept;

  private:
    struct OscillatorPointers
    {
        std::atomic<float>* on;
        std::atomic<float>* level;
        std::atomic<float>* pitch;
        std::atomic<float>* octave;
        std::atomic<float>* fine;
        std::atomic<float>* position;
        std::atomic<float>* phase;
    };

    // One per registered effect: its on switch, its mix and the values of its own parameters.
    struct EffectPointers
    {
        std::atomic<float>* enabled;
        std::atomic<float>* mix;
        std::vector<std::atomic<float>*> values;
        std::vector<float> defaults;
        float defaultMixPercent = 100.0f;
    };

    std::vector<EffectPointers> effects;
    std::atomic<float>* quality;
    std::atomic<float>* outputGain;
    std::atomic<float>* limiterOn;
    std::atomic<float>* keyTrack;
    std::atomic<float>* rootNote;
    std::atomic<float>* glideOn;
    std::atomic<float>* glideTime;
    std::atomic<float>* velocitySensitivity;
    std::atomic<float>* tockOn;
    std::atomic<float>* tockMode;
    std::atomic<float>* tockLevel;
    std::atomic<float>* tockTune;
    std::atomic<float>* tockStart;
    std::atomic<float>* tockDecay;
    std::atomic<float>* tockTone;
    std::atomic<float>* ampTimeScale;
    std::atomic<float>* pitchTimeScale;
    std::atomic<float>* pitchAmount;
    std::atomic<float>* punchSplit;
    std::atomic<float>* punchLevel;
    std::atomic<float>* tailLevel;
    std::array<OscillatorPointers, dsp::kNumOscillators> oscillators;
};

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/engine/EngineParameters.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::core
{

// Turns the APVTS parameters into the plain struct the engine consumes (ADR 0005). The raw
// parameter pointers are looked up once at construction, so read() is real-time safe.
class EngineParameterReader
{
  public:
    explicit EngineParameterReader(juce::AudioProcessorValueTreeState& state);

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

    std::atomic<float>* outputGain;
    std::atomic<float>* limiterOn;
    std::atomic<float>* keyTrack;
    std::atomic<float>* rootNote;
    std::atomic<float>* glideOn;
    std::atomic<float>* glideTime;
    std::atomic<float>* velocitySensitivity;
    std::atomic<float>* ampTimeScale;
    std::atomic<float>* pitchTimeScale;
    std::atomic<float>* pitchAmount;
    std::atomic<float>* punchBalance;
    std::atomic<float>* punchSplit;
    std::atomic<float>* punchLevel;
    std::atomic<float>* tailLevel;
    std::array<OscillatorPointers, dsp::kNumOscillators> oscillators;
};

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/tock/TockSample.h"

#include <cstdint>

namespace fcm::dsp
{

// What a Tock needs at note-on (docs/specs/tock.md §1: these are read when the note starts).
struct TockSettings
{
    bool sampleMode = false;
    float tuneSemitones = 0.0f;
    float startMs = 0.0f;
    float decayMs = 25.0f; // time to fall by 60 dB
    float tone = 0.7f;     // 0 dull .. 1 open
    // docs/specs/tock.md §8: the part of the sample that plays (fractions of its length) and the Pogo
    // pitch bend over that part. The defaults change nothing.
    float trimStart = 0.0f;
    float trimEnd = 1.0f;
    float pogoSemitones = 0.0f;
    // The played note relative to C3 when Key Track is on, else 0: added to Tune (docs/specs/tock.md §9).
    float keyTrackSemitones = 0.0f;
};

// The click layer of the kick (docs/specs/tock.md §4). One sample at a time, real-time safe, plain
// data (copied into the fading copy of the previous kick). Costs nothing when idle.
class Tock
{
  public:
    void prepare(double newSampleRate) noexcept;

    // The sample used in Sample mode, or nullptr for silence in that mode. Must outlive the block.
    void setSample(const TockSample* newSample) noexcept { sample = newSample; }

    // Begins a new Tock. Settings that are not finite or out of range are brought into range.
    void start(const TockSettings& settings) noexcept;

    // The next output sample; exactly 0 when the Tock is not running.
    float process() noexcept;

    void reset() noexcept;
    bool isRunning() const noexcept { return running; }

  private:
    float nextNoise() noexcept;
    // Multiplier of the pitch for the Pogo bend `age` samples into the played part.
    double pogoRatio() const noexcept;

    double sampleRate = 44100.0;
    const TockSample* sample = nullptr;

    bool running = false;
    bool sampleMode = false;
    double delaySamples = 0.0;
    double age = 0.0;            // samples since the Tock itself began
    double decayPerSample = 0.0; // ln(1000) / decay samples
    double phase = 0.0;          // synth tone phase, in cycles
    double phaseStep = 0.0;
    double readPosition = 0.0; // sample mode, in source samples
    double readStep = 0.0;
    double readBegin = 0.0;     // where the played part starts, in source samples
    double readEnd = 0.0;       // where the played part ends, in source samples
    double fadeInSamples = 0.0; // source samples; 0 when the trim starts at the beginning
    double fadeOutStart = 0.0;  // source position where the fade-out begins
    double fadeOutSamples = 0.0;
    double pogoSemitones = 0.0;
    double pogoLength = 1.0; // output samples the bend lasts
    float filterState = 0.0f;
    float filterCoefficient = 1.0f;
    std::uint32_t noiseState = 0x9e3779b9u;
};

} // namespace fcm::dsp

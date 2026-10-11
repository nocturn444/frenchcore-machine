// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/Voice.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kLevelSmoothSeconds = 0.010;
constexpr double kPitchSmoothSeconds = 0.020;
constexpr double kPositionSmoothSeconds = 0.010;

float totalSemitones(const OscillatorParameters& osc) noexcept
{
    return static_cast<float>(osc.pitchSemitones + 12 * osc.octave) + osc.fineCents * 0.01f;
}
} // namespace

void Voice::prepare(double newSampleRate, const WavetableData* wavetable)
{
    sampleRate = newSampleRate;

    auto prepareCore = [&](Core& voiceCore)
    {
        voiceCore.amp.prepare(sampleRate);
        voiceCore.pitch.prepare(sampleRate);
        for (auto& osc : voiceCore.oscillators)
        {
            osc.prepare(sampleRate);
            osc.setWavetable(wavetable);
        }
    };
    prepareCore(core);
    for (auto& ghost : ghosts)
        prepareCore(ghost.core);

    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        levels[i].reset(sampleRate, kLevelSmoothSeconds);
        semitones[i].reset(sampleRate, kPitchSmoothSeconds);
        positions[i].reset(sampleRate, kPositionSmoothSeconds);
    }

    reset();
}

void Voice::reset() noexcept
{
    auto resetCore = [](Core& voiceCore)
    {
        voiceCore.amp.reset();
        voiceCore.pitch.reset();
        for (auto& osc : voiceCore.oscillators)
            osc.reset();
        voiceCore.active = false;
        voiceCore.hasRendered = false;
        voiceCore.velocityGain = 1.0f;
    };
    resetCore(core);
    for (auto& ghost : ghosts)
    {
        resetCore(ghost.core);
        ghost.gain = 0.0f;
    }
    ghostStep = 0.0f;

    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        levels[i].setCurrentAndTargetValue(levels[i].getTargetValue());
        semitones[i].setCurrentAndTargetValue(semitones[i].getTargetValue());
        positions[i].setCurrentAndTargetValue(positions[i].getTargetValue());
    }
}

void Voice::setParameters(const EngineParameters& parameters, bool snap) noexcept
{
    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        const auto& osc = parameters.oscillators[i];
        const float level = osc.on ? osc.level : 0.0f;

        if (snap)
        {
            levels[i].setCurrentAndTargetValue(level);
            semitones[i].setCurrentAndTargetValue(totalSemitones(osc));
            positions[i].setCurrentAndTargetValue(osc.position);
        }
        else
        {
            levels[i].setTargetValue(level);
            semitones[i].setTargetValue(totalSemitones(osc));
            positions[i].setTargetValue(osc.position);
        }
        startPhases[i] = osc.startPhase;
    }
}

void Voice::trigger(float velocityGain) noexcept
{
    // A kick that is still sounding keeps rendering for a moment with a fade-out, so replacing it
    // with the new kick does not make a step in the waveform. A kick that has not output a sample
    // yet (several notes at the same instant) is simply replaced. If every ghost slot is busy the
    // one that has faded the most is reused.
    if (core.active && core.hasRendered)
    {
        auto quietest = std::min_element(ghosts.begin(), ghosts.end(),
                                         [](const Ghost& a, const Ghost& b) { return a.gain < b.gain; });
        quietest->core = core;
        quietest->gain = 1.0f;
        ghostStep = static_cast<float>(1.0 / (kRetriggerFadeMs * 0.001 * sampleRate));
    }

    for (size_t i = 0; i < kNumOscillators; ++i)
        core.oscillators[i].reset(static_cast<double>(startPhases[i]));

    core.amp.start();
    core.pitch.start();
    core.velocityGain = velocityGain;
    core.active = true;
    core.hasRendered = false;
}

bool Voice::isActive() const noexcept
{
    return core.active ||
           std::any_of(ghosts.begin(), ghosts.end(), [](const Ghost& g) { return g.gain > 0.0f; });
}

float Voice::render(Core& voiceCore, float settleNote, const Frame& frame) noexcept
{
    voiceCore.hasRendered = true;
    const float pitchOffset = voiceCore.pitch.next();
    const float amp = voiceCore.amp.next();
    if (!voiceCore.amp.isActive())
        voiceCore.active = false;

    // Hz of the settling pitch plus the envelope's offset, then each oscillator's own tuning.
    const float baseNote = settleNote + pitchOffset;
    float sum = 0.0f;
    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        const auto frequency =
            440.0 * std::exp2((static_cast<double>(baseNote + frame.semitones[i]) - 69.0) / 12.0);
        sum += voiceCore.oscillators[i].process(frequency) * frame.level[i];
    }
    return sum * amp * voiceCore.velocityGain;
}

float Voice::process(float settleNote) noexcept
{
    // Smoothers always advance so a parameter change is never left half-done while the voice is idle.
    Frame frame;
    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        frame.level[i] = levels[i].getNextValue();
        frame.semitones[i] = semitones[i].getNextValue();
        const float position = positions[i].getNextValue();
        core.oscillators[i].setPosition(position);
        for (auto& ghost : ghosts)
            ghost.core.oscillators[i].setPosition(position);
    }

    float output = 0.0f;
    if (core.active)
        output += render(core, settleNote, frame);

    for (auto& ghost : ghosts)
    {
        if (ghost.gain <= 0.0f)
            continue;

        if (ghost.core.active)
            output += render(ghost.core, settleNote, frame) * ghost.gain;
        ghost.gain = std::max(0.0f, ghost.gain - ghostStep);
    }

    return output;
}

} // namespace fcm::dsp

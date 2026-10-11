// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/Voice.h"

#include "dsp/envelope/BreakpointEnvelope.h"
#include "dsp/envelope/MusicalTime.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kLevelSmoothSeconds = 0.010;
constexpr double kPitchSmoothSeconds = 0.020;
constexpr double kPositionSmoothSeconds = 0.010;
constexpr double kPitchAmountSmoothSeconds = 0.020;
constexpr double kPunchTailSmoothSeconds = 0.010;

float totalSemitones(const OscillatorParameters& osc) noexcept
{
    return static_cast<float>(osc.pitchSemitones + 12 * osc.octave) + osc.fineCents * 0.01f;
}
} // namespace

void Voice::prepare(double newSampleRate, const WavetableData* wavetable)
{
    sampleRate = newSampleRate;
    fallbackTable = wavetable;

    defaultAmpShape = BreakpointEnvelope(EnvelopeKind::amp).toShape();
    defaultPitchShape = BreakpointEnvelope(EnvelopeKind::pitch).toShape();

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

    pitchAmount.reset(sampleRate, kPitchAmountSmoothSeconds);
    punchBalance.reset(sampleRate, kPunchTailSmoothSeconds);
    punchSplitMs.reset(sampleRate, kPunchTailSmoothSeconds);
    punchLevel.reset(sampleRate, kPunchTailSmoothSeconds);
    tailLevel.reset(sampleRate, kPunchTailSmoothSeconds);

    setShapes(nullptr, nullptr);
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
        voiceCore.samplesSinceNoteOn = 0.0;
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
    pitchAmount.setCurrentAndTargetValue(pitchAmount.getTargetValue());
    punchBalance.setCurrentAndTargetValue(punchBalance.getTargetValue());
    punchSplitMs.setCurrentAndTargetValue(punchSplitMs.getTargetValue());
    punchLevel.setCurrentAndTargetValue(punchLevel.getTargetValue());
    tailLevel.setCurrentAndTargetValue(tailLevel.getTargetValue());
}

void Voice::setShapes(const EnvelopeShape* ampShape, const EnvelopeShape* pitchShape) noexcept
{
    const EnvelopeShape* amp = ampShape != nullptr ? ampShape : defaultAmpShape.get();
    const EnvelopeShape* pitch = pitchShape != nullptr ? pitchShape : defaultPitchShape.get();

    ampShapeInUse = amp;
    pitchShapeInUse = pitch;
    core.amp.setShape(amp);
    core.pitch.setShape(pitch);
    for (auto& ghost : ghosts)
    {
        ghost.core.amp.setShape(amp);
        ghost.core.pitch.setShape(pitch);
    }
}

void Voice::setWavetables(const std::array<const WavetableData*, kNumOscillators>& tables) noexcept
{
    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        const WavetableData* table = tables[i] != nullptr ? tables[i] : fallbackTable;
        core.oscillators[i].setWavetable(table);
        for (auto& ghost : ghosts)
            ghost.core.oscillators[i].setWavetable(table);
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

    // std::clamp does not stop a NaN, so a tempo or scale that is not a finite number is ignored
    // (the last good value stays) instead of silencing the kick for good.
    if (std::isfinite(parameters.bpm) && parameters.bpm > 0.0f)
        bpm = std::clamp(parameters.bpm, static_cast<float>(kMinTempoBpm), static_cast<float>(kMaxTempoBpm));
    ampTimeScale = std::isfinite(parameters.ampTimeScale) ? parameters.ampTimeScale : 1.0f;
    pitchTimeScale = std::isfinite(parameters.pitchTimeScale) ? parameters.pitchTimeScale : 1.0f;

    if (snap)
    {
        pitchAmount.setCurrentAndTargetValue(parameters.pitchAmount);
        punchBalance.setCurrentAndTargetValue(parameters.punchBalance);
        punchSplitMs.setCurrentAndTargetValue(parameters.punchSplitMs);
        punchLevel.setCurrentAndTargetValue(parameters.punchLevel);
        tailLevel.setCurrentAndTargetValue(parameters.tailLevel);
    }
    else
    {
        pitchAmount.setTargetValue(parameters.pitchAmount);
        punchBalance.setTargetValue(parameters.punchBalance);
        punchSplitMs.setTargetValue(parameters.punchSplitMs);
        punchLevel.setTargetValue(parameters.punchLevel);
        tailLevel.setTargetValue(parameters.tailLevel);
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

    // A tempo-synced shape plays scaled by reference tempo / current tempo (docs/decisions/0007), on
    // top of the time scale. Both are fixed for the whole kick.
    const auto syncScale = [this](const EnvelopeShape* shape)
    { return shape != nullptr && shape->syncRefBpm > 0.0f ? shape->syncRefBpm / bpm : 1.0f; };
    core.amp.start(ampTimeScale * syncScale(ampShapeInUse));
    core.pitch.start(pitchTimeScale * syncScale(pitchShapeInUse));
    core.samplesSinceNoteOn = 0.0;
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
    const float pitchOffset = voiceCore.pitch.next() * frame.pitchAmount;
    const float amp = voiceCore.amp.next();
    if (!voiceCore.amp.isActive())
        voiceCore.active = false;

    // Punch / tail: the gain moves from the punch gain to the tail gain around the split time,
    // counted from this kick's own note-on, with a smoothstep over a few milliseconds.
    const double crossfadeSamples = kPunchTailCrossfadeMs * 0.001 * sampleRate;
    const double splitSamples = static_cast<double>(frame.punchSplitMs) * 0.001 * sampleRate;
    const double progress = std::clamp(
        (voiceCore.samplesSinceNoteOn - (splitSamples - 0.5 * crossfadeSamples)) / crossfadeSamples, 0.0,
        1.0);
    const auto s = static_cast<float>(progress * progress * (3.0 - 2.0 * progress));
    const float punchTailGain = frame.punchGain + (frame.tailGain - frame.punchGain) * s;
    voiceCore.samplesSinceNoteOn += 1.0;

    // Hz of the settling pitch plus the envelope's offset, then each oscillator's own tuning.
    const float baseNote = settleNote + pitchOffset;
    float sum = 0.0f;
    for (size_t i = 0; i < kNumOscillators; ++i)
    {
        const auto frequency =
            440.0 * std::exp2((static_cast<double>(baseNote + frame.semitones[i]) - 69.0) / 12.0);
        sum += voiceCore.oscillators[i].process(frequency) * frame.level[i];
    }
    return sum * amp * voiceCore.velocityGain * punchTailGain;
}

float Voice::process(float settleNote) noexcept
{
    // Smoothers always advance so a parameter change is never left half-done while the voice is idle.
    Frame frame;
    frame.pitchAmount = pitchAmount.getNextValue();
    const float balance = punchBalance.getNextValue();
    frame.punchSplitMs = punchSplitMs.getNextValue();
    frame.punchGain = (balance > 0.0f ? 1.0f - balance : 1.0f) * punchLevel.getNextValue();
    frame.tailGain = (balance < 0.0f ? 1.0f + balance : 1.0f) * tailLevel.getNextValue();
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

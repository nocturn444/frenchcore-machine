// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/transient/TransientEffect.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.010;
constexpr double kGainSmoothSeconds = 0.001;
constexpr double kFloor = 1.0e-6; // -120 dB: where the level counts as silence
// Speed 0 to 1 sets the fast follower's attack from 0.1 to 5 ms and the slow one's from 5 to 50 ms.
constexpr double kFastAttackMs[2] = {0.1, 5.0};
constexpr double kSlowAttackMs[2] = {5.0, 50.0};
// The releases are long enough that the followers do not ripple with a low note (a 50 Hz kick has a
// 20 ms cycle), which would show as distortion; the slow one is slower still so the sustain part
// lasts (docs/specs/fx-transient.md section 4).
// How many dB of gain one dB of difference gives at 100 %: the spec's first formula (0.24) would have
// made 100 % nearly inaudible on a kick, whose fast and slow followers differ by a few dB only.
constexpr double kAmountPerDb = 2.0;
constexpr double kFastReleaseMs = 30.0;
constexpr double kSlowReleaseMs = 300.0;

float finiteOrZero(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}

double coefficient(double milliseconds, double rate) noexcept
{
    return 1.0 - std::exp(-1.0 / (milliseconds * 0.001 * rate));
}

double lerpLog(const double (&range)[2], double t) noexcept
{
    return range[0] * std::pow(range[1] / range[0], t);
}

enum Param
{
    pAttack,
    pSustain,
    pSpeed,
    pOutput
};
} // namespace

const EffectDescriptor& TransientEffect::descriptor() const
{
    static const EffectDescriptor d{"transient", "Transient", 1};
    return d;
}

std::span<const ParameterSpec> TransientEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"attack", "Attack", ParameterKind::number, -100.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
        {"sustain", "Sustain", ParameterKind::number, -100.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
        {"speed", "Speed", ParameterKind::number, 0.0f, 100.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"output", "Output", ParameterKind::number, -24.0f, 12.0f, 0.1f, 0.0f, 0.0f, "dB", {}},
    };
    return specs;
}

void TransientEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);
    gainSmoothing = coefficient(kGainSmoothSeconds * 1000.0, sampleRate);
    attackAmount.reset(sampleRate, kSmoothSeconds);
    sustainAmount.reset(sampleRate, kSmoothSeconds);
    outputGain.reset(sampleRate, kSmoothSeconds);
    fast.release = coefficient(kFastReleaseMs, sampleRate);
    slow.release = coefficient(kSlowReleaseMs, sampleRate);
    reset();
}

void TransientEffect::reset() noexcept
{
    fast.value = 0.0;
    slow.value = 0.0;
    gainDb = 0.0;
    snapSmoothers = true;
}

void TransientEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 4)
        return;

    const float attack = std::clamp(finiteOrZero(values[pAttack]), -100.0f, 100.0f) * 0.01f;
    const float sustain = std::clamp(finiteOrZero(values[pSustain]), -100.0f, 100.0f) * 0.01f;
    const double speed = std::clamp(finiteOrZero(values[pSpeed]), 0.0f, 100.0f) * 0.01;
    const float output =
        juce::Decibels::decibelsToGain(std::clamp(finiteOrZero(values[pOutput]), -24.0f, 12.0f), -48.0f);

    fast.attack = coefficient(lerpLog(kFastAttackMs, speed), sampleRate);
    slow.attack = coefficient(lerpLog(kSlowAttackMs, speed), sampleRate);

    if (snapSmoothers)
    {
        attackAmount.setCurrentAndTargetValue(attack);
        sustainAmount.setCurrentAndTargetValue(sustain);
        outputGain.setCurrentAndTargetValue(output);
        snapSmoothers = false;
    }
    else
    {
        attackAmount.setTargetValue(attack);
        sustainAmount.setTargetValue(sustain);
        outputGain.setTargetValue(output);
    }
}

void TransientEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    std::array<float*, kMaxChannels> data{};
    for (int c = 0; c < numChannels; ++c)
        data[static_cast<size_t>(c)] = buffer.getWritePointer(c);

    for (int i = 0; i < numSamples; ++i)
    {
        // The linked level of the channels.
        double level = 0.0;
        for (int c = 0; c < numChannels; ++c)
        {
            float& sample = data[static_cast<size_t>(c)][i];
            if (!std::isfinite(sample))
                sample = 0.0f;
            level = std::max(level, static_cast<double>(std::abs(sample)));
        }

        // The followers always run, so they are in step when Attack or Sustain are turned up.
        const double quick = fast.run(level);
        const double steady = slow.run(level);

        const double attack = static_cast<double>(attackAmount.getNextValue());
        const double sustain = static_cast<double>(sustainAmount.getNextValue());
        const double output = static_cast<double>(outputGain.getNextValue());

        const bool shaping = attack != 0.0 || sustain != 0.0 || attackAmount.isSmoothing() ||
                             sustainAmount.isSmoothing() || std::abs(gainDb) > 1.0e-6;
        double gain = output;
        if (shaping)
        {
            // How far the fast follower is above (a hit starting) or below (ringing out) the slow one.
            const double difference =
                20.0 * (std::log10(std::max(quick, kFloor)) - std::log10(std::max(steady, kFloor)));
            const double attackSignal = std::max(0.0, difference);
            const double sustainSignal = std::max(0.0, -difference);
            const double wanted = std::clamp(kAmountPerDb * (attack * attackSignal + sustain * sustainSignal),
                                             -kMaxGainDb, kMaxGainDb);

            // 1 ms of smoothing, so the gain does not follow the cycles of a low note.
            gainDb += gainSmoothing * (wanted - gainDb);
            gain *= std::exp(gainDb * 0.1151292546497023); // ln(10) / 20
        }

        if (shaping || output != 1.0)
            for (int c = 0; c < numChannels; ++c)
                data[static_cast<size_t>(c)][i] = static_cast<float>(data[static_cast<size_t>(c)][i] * gain);
    }
}

} // namespace fcm::dsp

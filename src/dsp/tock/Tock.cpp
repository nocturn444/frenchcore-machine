// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/tock/Tock.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kBaseToneHz = 1200.0;
constexpr double kSilence = 1.0e-4; // -80 dB: the Tock stops here
constexpr double kMinDecayMs = 2.0;
constexpr double kMaxDecayMs = 200.0;
constexpr double kMaxStartMs = 20.0;
constexpr float kMaxTuneSemitones = 24.0f;
constexpr double kMaxPogoSemitones = 24.0;
constexpr double kMaxKeyTrackSemitones = 72.0; // the whole MIDI range around C3
constexpr double kMinPlayedMs = 1.0;           // a trim shorter than this plays this much
constexpr double kTrimFadeInMs = 0.5;          // only when the trim starts after the sample's beginning
constexpr double kTrimFadeOutMs = 2.0;         // only when the trim ends before the sample's end
constexpr double kPogoCurve = 4.0;             // the bend is exponential: fast at first

double finiteClamp(double value, double low, double high, double fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
} // namespace

void Tock::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void Tock::reset() noexcept
{
    running = false;
    delaySamples = 0.0;
    age = 0.0;
    phase = 0.0;
    readPosition = 0.0;
    filterState = 0.0f;
    noiseState = 0x9e3779b9u;
}

void Tock::start(const TockSettings& settings) noexcept
{
    sampleMode = settings.sampleMode;

    const double tune =
        finiteClamp(settings.tuneSemitones, -kMaxTuneSemitones, kMaxTuneSemitones, 0.0) +
        finiteClamp(settings.keyTrackSemitones, -kMaxKeyTrackSemitones, kMaxKeyTrackSemitones, 0.0);
    const double startMs = finiteClamp(settings.startMs, 0.0, kMaxStartMs, 0.0);
    const double decayMs = finiteClamp(settings.decayMs, kMinDecayMs, kMaxDecayMs, 25.0);
    const double tone = finiteClamp(settings.tone, 0.0, 1.0, 0.7);

    delaySamples = std::floor(startMs * 0.001 * sampleRate);
    age = 0.0;
    decayPerSample = std::log(1000.0) / (decayMs * 0.001 * sampleRate);

    // Same seed at every note, so every kick starts with the same noise.
    noiseState = 0x9e3779b9u;
    phase = 0.0;
    phaseStep = std::min(kBaseToneHz * std::exp2(tune / 12.0), 0.45 * sampleRate) / sampleRate;

    pogoSemitones = finiteClamp(settings.pogoSemitones, -kMaxPogoSemitones, kMaxPogoSemitones, 0.0);
    const double noteStep = std::exp2(tune / 12.0);

    readPosition = readBegin = 0.0;
    readEnd = 0.0;
    fadeInSamples = fadeOutSamples = 0.0;
    fadeOutStart = 0.0;
    readStep = (sample != nullptr ? sample->sampleRate / sampleRate : 1.0) * noteStep;
    if (sample != nullptr && sample->samples.size() >= 2)
    {
        // docs/specs/tock.md §8.4: if the end is not 1 ms after the start, 1 ms plays from the start.
        const auto length = static_cast<double>(sample->samples.size());
        const double last = length - 1.0;
        const double trimStart = finiteClamp(settings.trimStart, 0.0, 1.0, 0.0);
        const double trimEnd = finiteClamp(settings.trimEnd, 0.0, 1.0, 1.0);
        const double minPlayed = kMinPlayedMs * 0.001 * sample->sampleRate;
        const double begin = std::min(trimStart * length, std::max(last - minPlayed, 0.0));
        double end = trimEnd * length;
        if (end - begin < minPlayed)
            end = begin + minPlayed;
        readPosition = readBegin = begin;
        readEnd = std::min(end, last);
        if (begin > 0.0)
            fadeInSamples = kTrimFadeInMs * 0.001 * sample->sampleRate;
        if (trimEnd < 1.0)
        {
            fadeOutSamples = std::min(kTrimFadeOutMs * 0.001 * sample->sampleRate, readEnd - begin);
            fadeOutStart = readEnd - fadeOutSamples;
        }
        pogoLength = std::max((readEnd - begin) / readStep, 1.0);
    }
    // In Synth mode the bend lasts as long as the tone does.
    if (!sampleMode || pogoLength <= 1.0)
        pogoLength = std::max(decayMs * 0.001 * sampleRate, 1.0);

    const double cutoff = std::min(200.0 * std::pow(100.0, tone), 0.45 * sampleRate);
    filterCoefficient = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * cutoff / sampleRate));
    filterState = 0.0f;

    running = true;
}

float Tock::nextNoise() noexcept
{
    // xorshift32 (Marsaglia 2003), mapped to -1..1.
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return static_cast<float>(noiseState) * (2.0f / 4294967296.0f) - 1.0f;
}

double Tock::pogoRatio() const noexcept
{
    if (pogoSemitones == 0.0)
        return 1.0;
    // Exponential approach, normalised to reach the full bend at the end of the part.
    const double t = std::min(age / pogoLength, 1.0);
    const double bend = (1.0 - std::exp(-kPogoCurve * t)) / (1.0 - std::exp(-kPogoCurve));
    return std::exp2(pogoSemitones * bend / 12.0);
}

float Tock::process() noexcept
{
    if (!running)
        return 0.0f;

    if (delaySamples > 0.0)
    {
        delaySamples -= 1.0;
        return 0.0f;
    }

    const double envelope = std::exp(-age * decayPerSample);
    const double pogo = pogoRatio();
    age += 1.0;
    if (envelope < kSilence)
    {
        running = false;
        return 0.0f;
    }

    float raw = 0.0f;
    if (sampleMode)
    {
        if (sample == nullptr || sample->samples.size() < 2)
        {
            running = false;
            return 0.0f;
        }

        const double last = readEnd;
        if (readPosition >= last)
        {
            running = false;
            return 0.0f;
        }
        const auto index = static_cast<size_t>(readPosition);
        const auto fraction = static_cast<float>(readPosition - static_cast<double>(index));
        raw = (sample->samples[index] * (1.0f - fraction) + sample->samples[index + 1] * fraction) *
              static_cast<float>(envelope);

        // The cuts of a trimmed sample are faded so they never click (docs/specs/tock.md §8.2).
        if (fadeInSamples > 0.0)
            raw *= static_cast<float>(std::min((readPosition - readBegin) / fadeInSamples, 1.0));
        if (fadeOutSamples > 0.0 && readPosition > fadeOutStart)
            raw *= static_cast<float>(std::max((readEnd - readPosition) / fadeOutSamples, 0.0));
        readPosition += readStep * pogo;
    }
    else
    {
        const float blip = static_cast<float>(std::sin(2.0 * std::numbers::pi * phase));
        phase += phaseStep * pogo;
        phase -= std::floor(phase);

        const auto e = static_cast<float>(envelope);
        raw = 0.8f * (0.7f * blip * e + 0.5f * nextNoise() * e * e * e);
    }

    filterState += filterCoefficient * (raw - filterState);
    return filterState;
}

} // namespace fcm::dsp

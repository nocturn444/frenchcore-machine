// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/formant/FormantEffect.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace fcm::dsp
{

namespace
{
constexpr double kVowelSeconds = 0.020;
constexpr double kSmoothSeconds = 0.010;
constexpr double kMaxFrequencyOfRate = 0.45;
constexpr double kMaxDamping = 2.0; // k = bandwidth / frequency, so the widest band is still a band
constexpr double kDriveShape = 15.0;
constexpr double kMakeupTargetPower = 0.25; // pink noise comes out 6 dB below its input

// Bandwidths in Hz at Resonance 50 %; Resonance scales them by 2^(1 - 2 r).
constexpr std::array<double, FormantEffect::kFormants> kBandwidthHz{80.0, 100.0, 120.0};

// Frequencies in Hz and gains in dB; the gains are a first voicing, for the product owner's ears.
constexpr std::array<FormantEffect::Vowel, FormantEffect::kVowelCount> kVowels{{
    {{730.0, 1090.0, 2440.0}, {0.0, -5.0, -15.0}},  // A
    {{530.0, 1840.0, 2480.0}, {0.0, -8.0, -14.0}},  // E
    {{270.0, 2290.0, 3010.0}, {0.0, -12.0, -16.0}}, // I
    {{570.0, 840.0, 2410.0}, {0.0, -2.0, -18.0}},   // O
    {{300.0, 870.0, 2240.0}, {0.0, -10.0, -22.0}},  // U
}};

enum Param
{
    pVowel,
    pMorph,
    pShift,
    pResonance,
    pDrive
};

float finiteOrZero(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}

double bandwidthScale(double resonanceUnit) noexcept
{
    return std::pow(2.0, 1.0 - 2.0 * resonanceUnit);
}

// The pink-noise power of one vowel through the analog prototype of the three band-passes:
// |sum of g_i H_i|^2, averaged over the logarithm of the frequency (equal power per octave is what
// makes noise pink) between 20 Hz and 20 kHz.
double pinkPower(const FormantEffect::Vowel& vowel) noexcept
{
    constexpr int kPoints = 600;
    const double low = std::log(20.0), high = std::log(20000.0);
    double total = 0.0;
    for (int n = 0; n < kPoints; ++n)
    {
        const double f = std::exp(low + (high - low) * (n + 0.5) / kPoints);
        std::complex<double> sum = 0.0;
        for (int i = 0; i < FormantEffect::kFormants; ++i)
        {
            const double k = std::min(kBandwidthHz[static_cast<size_t>(i)] / vowel.hz[static_cast<size_t>(i)],
                                      kMaxDamping);
            const double w = f / vowel.hz[static_cast<size_t>(i)];
            const std::complex<double> s(0.0, w);
            const double g = std::pow(10.0, vowel.gainDb[static_cast<size_t>(i)] / 20.0);
            sum += g * (k * s) / (1.0 + k * s + s * s);
        }
        total += std::norm(sum);
    }
    return total / kPoints;
}

const std::array<double, FormantEffect::kVowelCount>& makeupTable()
{
    static const std::array<double, FormantEffect::kVowelCount> table = []
    {
        std::array<double, FormantEffect::kVowelCount> made{};
        for (size_t v = 0; v < made.size(); ++v)
            made[v] = 10.0 * std::log10(kMakeupTargetPower / pinkPower(kVowels[v]));
        return made;
    }();
    return table;
}
} // namespace

const EffectDescriptor& FormantEffect::descriptor() const
{
    static const EffectDescriptor d{"formant", "Formant", 1, 1.0f, false};
    return d;
}

std::span<const ParameterSpec> FormantEffect::parameters() const
{
    static constexpr std::string_view kVowelNames[] = {"A", "E", "I", "O", "U"};
    static const ParameterSpec specs[] = {
        {"vowel", "Vowel", ParameterKind::choice, 0.0f, 4.0f, 1.0f, 0.0f, 0.0f, "", kVowelNames},
        {"morph", "Morph", ParameterKind::number, 0.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
        {"shift", "Shift", ParameterKind::number, -12.0f, 12.0f, 0.01f, 0.0f, 0.0f, "st", {}},
        {"resonance", "Resonance", ParameterKind::number, 0.0f, 100.0f, 0.1f, 50.0f, 0.0f, "%", {}},
        {"drive", "Drive", ParameterKind::number, 0.0f, 100.0f, 0.1f, 0.0f, 0.0f, "%", {}},
    };
    return specs;
}

const FormantEffect::Vowel& FormantEffect::vowel(int index) noexcept
{
    return kVowels[static_cast<size_t>(std::clamp(index, 0, kVowelCount - 1))];
}

double FormantEffect::makeupDb(int index) noexcept
{
    return makeupTable()[static_cast<size_t>(std::clamp(index, 0, kVowelCount - 1))];
}

double FormantEffect::saturate(double x, double d) noexcept
{
    d = std::clamp(std::isfinite(d) ? d : 0.0, 0.0, 1.0);
    const double a = kDriveShape * d * d;
    if (a < 1.0e-6)
        return x;
    return std::tanh(a * x) / std::tanh(a);
}

void FormantEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);
    makeupTable(); // built here, never on the audio thread
    for (auto& smoothed : frequency)
        smoothed.reset(sampleRate, kVowelSeconds);
    for (auto& smoothed : gain)
        smoothed.reset(sampleRate, kVowelSeconds);
    resonance.reset(sampleRate, kSmoothSeconds);
    drive.reset(sampleRate, kSmoothSeconds);
    reset();
}

void FormantEffect::reset() noexcept
{
    for (auto& channel : bands)
        for (auto& band : channel)
            band = {};
    snapSmoothers = true;
}

void FormantEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < 5)
        return;

    const int chosen =
        std::clamp(static_cast<int>(std::lround(finiteOrZero(values[pVowel]))), 0, kVowelCount - 1);
    const int next = (chosen + 1) % kVowelCount;
    const double morph = std::clamp(static_cast<double>(finiteOrZero(values[pMorph])) * 0.01, 0.0, 1.0);
    const double shift = std::clamp(static_cast<double>(finiteOrZero(values[pShift])), -12.0, 12.0);
    const double resonanceTarget =
        std::clamp(static_cast<double>(finiteOrZero(values[pResonance])) * 0.01, 0.0, 1.0);
    const double driveTarget = std::clamp(static_cast<double>(finiteOrZero(values[pDrive])) * 0.01, 0.0, 1.0);

    // Frequencies are interpolated in the logarithmic domain, gains (and the make-up) in dB.
    const double shiftRatio = std::pow(2.0, shift / 12.0);
    const double maxHz = kMaxFrequencyOfRate * sampleRate;
    const double makeup = makeupDb(chosen) * (1.0 - morph) + makeupDb(next) * morph;
    std::array<float, kFormants> hz{}, linear{};
    for (size_t i = 0; i < kFormants; ++i)
    {
        const double a = kVowels[static_cast<size_t>(chosen)].hz[i],
                     b = kVowels[static_cast<size_t>(next)].hz[i];
        hz[i] = static_cast<float>(std::clamp(
            std::exp(std::log(a) * (1.0 - morph) + std::log(b) * morph) * shiftRatio, 20.0, maxHz));
        const double db = kVowels[static_cast<size_t>(chosen)].gainDb[i] * (1.0 - morph) +
                          kVowels[static_cast<size_t>(next)].gainDb[i] * morph + makeup;
        linear[i] = static_cast<float>(std::pow(10.0, db / 20.0));
    }

    if (snapSmoothers)
    {
        for (size_t i = 0; i < kFormants; ++i)
        {
            frequency[i].setCurrentAndTargetValue(hz[i]);
            gain[i].setCurrentAndTargetValue(linear[i]);
        }
        resonance.setCurrentAndTargetValue(static_cast<float>(resonanceTarget));
        drive.setCurrentAndTargetValue(static_cast<float>(driveTarget));
        snapSmoothers = false;
    }
    else
    {
        for (size_t i = 0; i < kFormants; ++i)
        {
            frequency[i].setTargetValue(hz[i]);
            gain[i].setTargetValue(linear[i]);
        }
        resonance.setTargetValue(static_cast<float>(resonanceTarget));
        drive.setTargetValue(static_cast<float>(driveTarget));
    }
}

void FormantEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    std::array<float*, kMaxChannels> data{};
    for (int c = 0; c < numChannels; ++c)
        data[static_cast<size_t>(c)] = buffer.getWritePointer(c);

    for (int i = 0; i < numSamples; ++i)
    {
        const double scale = bandwidthScale(static_cast<double>(resonance.getNextValue()));
        const double d = static_cast<double>(drive.getNextValue());

        std::array<double, kFormants> g{}, k{}, a1{}, a2{}, a3{}, level{};
        for (size_t f = 0; f < kFormants; ++f)
        {
            const double fc = static_cast<double>(frequency[f].getNextValue());
            level[f] = static_cast<double>(gain[f].getNextValue());
            g[f] = std::tan(std::numbers::pi * fc / sampleRate);
            k[f] = std::min(kBandwidthHz[f] * scale / fc, kMaxDamping);
            a1[f] = 1.0 / (1.0 + g[f] * (g[f] + k[f]));
            a2[f] = g[f] * a1[f];
            a3[f] = g[f] * a2[f];
        }

        for (int c = 0; c < numChannels; ++c)
        {
            float& sample = data[static_cast<size_t>(c)][i];
            auto& channel = bands[static_cast<size_t>(c)];
            const double x = saturate(std::isfinite(sample) ? static_cast<double>(sample) : 0.0, d);

            double y = 0.0;
            for (size_t f = 0; f < kFormants; ++f)
            {
                auto& band = channel[f];
                const double v3 = x - band.ic2;
                const double v1 = a1[f] * band.ic1 + a2[f] * v3;
                const double v2 = band.ic2 + a2[f] * band.ic1 + a3[f] * v3;
                band.ic1 = 2.0 * v1 - band.ic1;
                band.ic2 = 2.0 * v2 - band.ic2;
                y += level[f] * k[f] * v1; // the band-pass has a peak gain of 1
            }
            sample = std::isfinite(y) ? static_cast<float>(y) : 0.0f;
        }
    }
}

} // namespace fcm::dsp

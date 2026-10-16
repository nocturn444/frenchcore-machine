// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/compressor/CompressorEffect.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kSmoothSeconds = 0.010;
constexpr double kFloorDb = -120.0;
constexpr double kMinGainDb = -60.0;
constexpr double kMaxGainDb = 24.0;
constexpr double kDbToGain = 0.1151292546497023; // ln(10) / 20
constexpr double kDetectorHoldMs = 20.0;

// The multiband numbers, in one table (docs/specs/fx-compressor.md section 4): the downward threshold
// and the attack and release (ms) of each band, low to high. The upward threshold is the same for all.
struct BandSettings
{
    double downwardThresholdDb;
    double attackMs;
    double releaseMs;
};
constexpr std::array<BandSettings, 3> kBandTable{
    {{-14.0, 30.0, 300.0}, {-20.0, 10.0, 150.0}, {-20.0, 3.0, 80.0}}};
constexpr double kUpwardThresholdDb = -40.0;
constexpr double kDownwardRatioAtFull = 66.0;
constexpr double kUpwardRatioAtFull = 4.0;

enum Param
{
    pMultiband,
    pThreshold,
    pRatio,
    pAttack,
    pRelease,
    pKnee,
    pMakeup,
    pDepth,
    pTime,
    pUpward,
    pDownward,
    pInGain,
    pOutGain,
    pLow,
    pMid,
    pHigh,
    kNumParams
};

// Bit 0: Multiband off, bit 1: on (ParameterSpec::shownWhen).
constexpr unsigned kSingleOnly = 1u << 0;
constexpr unsigned kMultibandOnly = 1u << 1;

float finiteOr(float x, float fallback) noexcept
{
    return std::isfinite(x) ? x : fallback;
}

double toDb(double level) noexcept
{
    return level > 1.0e-6 ? 20.0 * std::log10(level) : kFloorDb;
}

double coefficient(double milliseconds, double rate) noexcept
{
    return std::exp(-1.0 / (std::max(milliseconds, 0.01) * 0.001 * rate));
}

double dbToGain(double db) noexcept
{
    return std::exp(db * kDbToGain);
}
} // namespace

const EffectDescriptor& CompressorEffect::descriptor() const
{
    static const EffectDescriptor d{"compressor", "Compressor", 1};
    return d;
}

std::span<const ParameterSpec> CompressorEffect::parameters() const
{
    static const ParameterSpec specs[] = {
        {"multiband", "Multiband", ParameterKind::toggle, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "", {}},
        {"threshold",
         "Threshold",
         ParameterKind::number,
         -60.0f,
         0.0f,
         0.1f,
         -18.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kSingleOnly},
        {"ratio",
         "Ratio",
         ParameterKind::number,
         1.0f,
         20.0f,
         0.1f,
         4.0f,
         4.0f,
         ":1",
         {},
         "multiband",
         kSingleOnly},
        {"attack",
         "Attack",
         ParameterKind::number,
         0.1f,
         100.0f,
         0.1f,
         5.0f,
         10.0f,
         "ms",
         {},
         "multiband",
         kSingleOnly},
        {"release",
         "Release",
         ParameterKind::number,
         10.0f,
         1000.0f,
         1.0f,
         100.0f,
         150.0f,
         "ms",
         {},
         "multiband",
         kSingleOnly},
        {"knee",
         "Knee",
         ParameterKind::number,
         0.0f,
         12.0f,
         0.1f,
         6.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kSingleOnly},
        {"makeup",
         "Makeup",
         ParameterKind::number,
         -12.0f,
         24.0f,
         0.1f,
         0.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kSingleOnly},
        {"depth",
         "Depth",
         ParameterKind::number,
         0.0f,
         100.0f,
         0.1f,
         100.0f,
         0.0f,
         "%",
         {},
         "multiband",
         kMultibandOnly},
        {"time",
         "Time",
         ParameterKind::number,
         0.0f,
         100.0f,
         0.1f,
         50.0f,
         0.0f,
         "%",
         {},
         "multiband",
         kMultibandOnly},
        {"upward",
         "Upward",
         ParameterKind::number,
         0.0f,
         100.0f,
         0.1f,
         100.0f,
         0.0f,
         "%",
         {},
         "multiband",
         kMultibandOnly},
        {"downward",
         "Downward",
         ParameterKind::number,
         0.0f,
         100.0f,
         0.1f,
         100.0f,
         0.0f,
         "%",
         {},
         "multiband",
         kMultibandOnly},
        {"ingain",
         "In Gain",
         ParameterKind::number,
         -24.0f,
         24.0f,
         0.1f,
         0.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kMultibandOnly},
        {"outgain",
         "Out Gain",
         ParameterKind::number,
         -24.0f,
         24.0f,
         0.1f,
         0.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kMultibandOnly},
        {"low",
         "Low",
         ParameterKind::number,
         -24.0f,
         24.0f,
         0.1f,
         0.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kMultibandOnly},
        {"mid",
         "Mid",
         ParameterKind::number,
         -24.0f,
         24.0f,
         0.1f,
         0.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kMultibandOnly},
        {"high",
         "High",
         ParameterKind::number,
         -24.0f,
         24.0f,
         0.1f,
         0.0f,
         0.0f,
         "dB",
         {},
         "multiband",
         kMultibandOnly},
    };
    return specs;
}

double CompressorEffect::staticReduction(double levelDb, double thresholdDb, double ratio,
                                         double kneeDb) noexcept
{
    ratio = std::clamp(std::isfinite(ratio) ? ratio : 1.0, 1.0, 100.0);
    kneeDb = std::max(0.0, std::isfinite(kneeDb) ? kneeDb : 0.0);
    const double over = levelDb - thresholdDb;

    double out = levelDb;
    if (kneeDb > 0.0 && 2.0 * std::abs(over) <= kneeDb)
    {
        const double inside = over + 0.5 * kneeDb;
        out = levelDb + (1.0 / ratio - 1.0) * inside * inside / (2.0 * kneeDb);
    }
    else if (2.0 * over > kneeDb)
    {
        out = thresholdDb + over / ratio;
    }
    return std::max(0.0, levelDb - out);
}

double CompressorEffect::multibandGainDb(int band, double levelDb, double downwardAmount, double upwardAmount,
                                         double depthAmount) noexcept
{
    const auto& settings = kBandTable[static_cast<size_t>(std::clamp(band, 0, kBands - 1))];
    downwardAmount = std::clamp(std::isfinite(downwardAmount) ? downwardAmount : 0.0, 0.0, 1.0);
    upwardAmount = std::clamp(std::isfinite(upwardAmount) ? upwardAmount : 0.0, 0.0, 1.0);
    depthAmount = std::clamp(std::isfinite(depthAmount) ? depthAmount : 0.0, 0.0, 1.0);

    // Each amount scales the ratio between 1:1 and the full one, so 0 % does nothing of that kind.
    double gain = 0.0;
    if (levelDb > settings.downwardThresholdDb)
    {
        const double ratio = 1.0 + (kDownwardRatioAtFull - 1.0) * downwardAmount;
        gain -= (levelDb - settings.downwardThresholdDb) * (1.0 - 1.0 / ratio);
    }
    if (levelDb < kUpwardThresholdDb)
    {
        const double ratio = 1.0 + (kUpwardRatioAtFull - 1.0) * upwardAmount;
        gain += std::min(kMaxGainDb, (kUpwardThresholdDb - levelDb) * (1.0 - 1.0 / ratio));
    }
    return std::clamp(gain * depthAmount, kMinGainDb, kMaxGainDb);
}

void CompressorEffect::prepare(const EffectSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    channels = std::clamp(spec.numChannels, 1, kMaxChannels);

    // The crossovers sit below Nyquist whatever the sample rate.
    const double low = std::min(kCrossoverLowHz, 0.2 * sampleRate);
    const double high = std::min(kCrossoverHighHz, 0.4 * sampleRate);
    lowPass1 = StereoEffect::lowPass(sampleRate, low);
    highPass1 = StereoEffect::highPass(sampleRate, low);
    lowPass2 = StereoEffect::lowPass(sampleRate, high);
    highPass2 = StereoEffect::highPass(sampleRate, high);

    for (auto* smoothed :
         {&multibandMix, &threshold, &ratio, &makeup, &depth, &upward, &downward, &inGain, &outGain})
        smoothed->reset(sampleRate, kSmoothSeconds);
    for (auto& level : bandLevel)
        level.reset(sampleRate, kSmoothSeconds);
    reset();
}

void CompressorEffect::reset() noexcept
{
    splits = {};
    reduction = 0.0;
    detector = 0.0;
    bandGainDb.fill(0.0);
    singleRan = multiRan = false;
    snapSmoothers = true;
}

void CompressorEffect::setParameters(std::span<const float> values) noexcept
{
    if (values.size() < static_cast<size_t>(kNumParams))
        return;

    const auto value = [&](Param p, float fallback)
    { return finiteOr(values[static_cast<size_t>(p)], fallback); };

    block.multiband = value(pMultiband, 0.0f) >= 0.5f;
    block.knee = std::clamp(static_cast<double>(value(pKnee, 6.0f)), 0.0, 12.0);
    block.attack = coefficient(std::clamp(static_cast<double>(value(pAttack, 5.0f)), 0.1, 100.0), sampleRate);
    block.release =
        coefficient(std::clamp(static_cast<double>(value(pRelease, 100.0f)), 10.0, 1000.0), sampleRate);
    block.detectorDecay = coefficient(kDetectorHoldMs, sampleRate);

    // Time scales the per-band times from 0.1 to 2 (50 % is a little over 1).
    const double timeScale =
        0.1 + 1.9 * std::clamp(static_cast<double>(value(pTime, 50.0f)), 0.0, 100.0) / 100.0;
    for (size_t b = 0; b < kBands; ++b)
    {
        block.bandAttack[b] = coefficient(kBandTable[b].attackMs * timeScale, sampleRate);
        block.bandRelease[b] = coefficient(kBandTable[b].releaseMs * timeScale, sampleRate);
    }

    const auto clampTo = [&](Param p, float low, float high, float fallback)
    { return std::clamp(value(p, fallback), low, high); };
    const float mixTarget = block.multiband ? 1.0f : 0.0f;
    const float thresholdTarget = clampTo(pThreshold, -60.0f, 0.0f, -18.0f);
    const float ratioTarget = clampTo(pRatio, 1.0f, 20.0f, 4.0f);
    const float makeupTarget = clampTo(pMakeup, -12.0f, 24.0f, 0.0f);
    const float depthTarget = clampTo(pDepth, 0.0f, 100.0f, 100.0f) * 0.01f;
    const float upwardTarget = clampTo(pUpward, 0.0f, 100.0f, 100.0f) * 0.01f;
    const float downwardTarget = clampTo(pDownward, 0.0f, 100.0f, 100.0f) * 0.01f;
    const float inTarget = juce::Decibels::decibelsToGain(clampTo(pInGain, -24.0f, 24.0f, 0.0f), -60.0f);
    const float outTarget = juce::Decibels::decibelsToGain(clampTo(pOutGain, -24.0f, 24.0f, 0.0f), -60.0f);
    const std::array<float, kBands> bandTargets{
        juce::Decibels::decibelsToGain(clampTo(pLow, -24.0f, 24.0f, 0.0f), -60.0f),
        juce::Decibels::decibelsToGain(clampTo(pMid, -24.0f, 24.0f, 0.0f), -60.0f),
        juce::Decibels::decibelsToGain(clampTo(pHigh, -24.0f, 24.0f, 0.0f), -60.0f)};

    const auto set = [&](auto& smoothed, float target)
    {
        if (snapSmoothers)
            smoothed.setCurrentAndTargetValue(target);
        else
            smoothed.setTargetValue(target);
    };
    set(multibandMix, mixTarget);
    set(threshold, thresholdTarget);
    set(ratio, ratioTarget);
    set(makeup, makeupTarget);
    set(depth, depthTarget);
    set(upward, upwardTarget);
    set(downward, downwardTarget);
    set(inGain, inTarget);
    set(outGain, outTarget);
    for (size_t b = 0; b < kBands; ++b)
        set(bandLevel[b], bandTargets[b]);
    snapSmoothers = false;
}

double CompressorEffect::singleBand(double left, double right, double& outLeft, double& outRight) noexcept
{
    // The detector sees the louder channel, so the stereo image does not move. It holds the peak and lets
    // go over 20 ms, so a steady tone is seen at its peak and not between its cycles.
    detector = std::max(std::max(std::abs(left), std::abs(right)), detector * block.detectorDecay);
    const double level = toDb(detector);
    const double wanted = staticReduction(level, static_cast<double>(threshold.getCurrentValue()),
                                          static_cast<double>(ratio.getCurrentValue()), block.knee);

    // Attack when the reduction is growing, release when it is falling (a smooth branching detector).
    const double coefficientNow = wanted > reduction ? block.attack : block.release;
    reduction = coefficientNow * reduction + (1.0 - coefficientNow) * wanted;

    const double gain = dbToGain(
        std::clamp(static_cast<double>(makeup.getCurrentValue()) * 1.0 - reduction, kMinGainDb, kMaxGainDb));
    outLeft = left * gain;
    outRight = right * gain;
    return gain;
}

void CompressorEffect::multiBand(double left, double right, double& outLeft, double& outRight) noexcept
{
    const double inputGain = static_cast<double>(inGain.getCurrentValue());
    const std::array<double, 2> input{left * inputGain, right * inputGain};
    std::array<std::array<double, kBands>, 2> bands{};

    for (int c = 0; c < 2; ++c)
    {
        auto& split = splits[static_cast<size_t>(c)];
        const double x = input[static_cast<size_t>(c)];
        const double lowBand = split.low1.run(lowPass1, x);
        const double rest = split.high1.run(highPass1, x);
        const double mid = split.low2.run(lowPass2, rest);
        const double high = split.high2.run(highPass2, rest);

        // The low band gets the second crossover's all-pass, so the three stay in phase (as in Stereo).
        bands[static_cast<size_t>(c)] = {
            split.alignLow.run(lowPass2, lowBand) + split.alignHigh.run(highPass2, lowBand), mid, high};
    }

    double sumLeft = 0.0, sumRight = 0.0;
    for (size_t b = 0; b < kBands; ++b)
    {
        const double level = toDb(std::max(std::abs(bands[0][b]), std::abs(bands[1][b])));
        const double wanted = multibandGainDb(
            static_cast<int>(b), level, static_cast<double>(downward.getCurrentValue()),
            static_cast<double>(upward.getCurrentValue()), static_cast<double>(depth.getCurrentValue()));

        // Attack when the gain has to fall, release when it rises.
        const double coefficientNow = wanted < bandGainDb[b] ? block.bandAttack[b] : block.bandRelease[b];
        bandGainDb[b] = coefficientNow * bandGainDb[b] + (1.0 - coefficientNow) * wanted;

        const double gain = dbToGain(bandGainDb[b]) * static_cast<double>(bandLevel[b].getCurrentValue());
        sumLeft += bands[0][b] * gain;
        sumRight += bands[1][b] * gain;
    }

    const double outputGain = static_cast<double>(outGain.getCurrentValue());
    outLeft = sumLeft * outputGain;
    outRight = sumRight * outputGain;
}

void CompressorEffect::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numChannels = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();
    if (numChannels < 1)
        return;

    float* left = buffer.getWritePointer(0);
    float* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Advance the smoothers once per sample; the paths read their current values.
        const double mix = static_cast<double>(multibandMix.getNextValue());
        for (auto* smoothed : {&threshold, &ratio, &makeup, &depth, &upward, &downward, &inGain, &outGain})
            smoothed->getNextValue();
        for (auto& level : bandLevel)
            level.getNextValue();

        const double inLeft = std::isfinite(left[i]) ? static_cast<double>(left[i]) : 0.0;
        const double inRight =
            right != nullptr ? (std::isfinite(right[i]) ? static_cast<double>(right[i]) : 0.0) : inLeft;

        double singleLeft = 0.0, singleRight = 0.0, multiLeft = 0.0, multiRight = 0.0;
        // The path that is not heard is not run, except during the crossfade, and starts again from
        // silence when it is needed.
        if (mix < 1.0)
        {
            singleBand(inLeft, inRight, singleLeft, singleRight);
            singleRan = true;
        }
        else if (singleRan)
        {
            reduction = 0.0;
            detector = 0.0;
            singleRan = false;
        }

        if (mix > 0.0)
        {
            multiBand(inLeft, inRight, multiLeft, multiRight);
            multiRan = true;
        }
        else if (multiRan)
        {
            splits = {};
            bandGainDb.fill(0.0);
            multiRan = false;
        }

        const double outLeft = singleLeft * (1.0 - mix) + multiLeft * mix;
        const double outRight = singleRight * (1.0 - mix) + multiRight * mix;
        left[i] = std::isfinite(outLeft) ? static_cast<float>(outLeft) : 0.0f;
        if (right != nullptr)
            right[i] = std::isfinite(outRight) ? static_cast<float>(outRight) : 0.0f;
    }
}

} // namespace fcm::dsp

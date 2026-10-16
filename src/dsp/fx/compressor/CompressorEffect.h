// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/stereo/StereoEffect.h"

#include <array>

namespace fcm::dsp
{

// A compressor with a Multiband switch (docs/specs/fx-compressor.md). Off: a feed-forward compressor
// with a log-domain gain computer and soft knee, after Giannoulis, Massberg and Reiss, "Digital
// Dynamic Range Compressor Design: A Tutorial and Analysis", JAES 60(6), 2012. On: an OTT-style
// three-band compressor that turns loud parts down and quiet parts up in every band.
class CompressorEffect : public Effect
{
  public:
    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The gain reduction in dB (>= 0) the single-band compressor applies to a level of `levelDb`, for a
    // threshold, ratio and knee width in dB (0 for a hard knee). Exposed for tests.
    static double staticReduction(double levelDb, double thresholdDb, double ratio, double kneeDb) noexcept;

    // The three bands of the multiband mode: split points and per-band settings (the numbers are a
    // starting point to be voiced by ear, docs/specs/fx-compressor.md section 4).
    static constexpr int kBands = 3;
    static constexpr double kCrossoverLowHz = 88.0;
    static constexpr double kCrossoverHighHz = 2500.0;

    // The gain in dB the multiband mode applies to a band at `levelDb` (before smoothing): downward
    // above the band's threshold, upward below -40 dB, each scaled by its amount (0 to 1) and by
    // depth (0 to 1), limited to -60..+24 dB.
    static double multibandGainDb(int band, double levelDb, double downward, double upward,
                                  double depth) noexcept;

  private:
    static constexpr int kMaxChannels = 2;

    // The values of one block, in the units the process loop uses.
    struct Block
    {
        bool multiband = false;
        double thresholdDb = -18.0;
        double knee = 6.0;
        double attack = 0.0, release = 0.0;                     // single band, one-pole coefficients
        double detectorDecay = 0.0;                             // how fast the peak detector lets go
        std::array<double, kBands> bandAttack{}, bandRelease{}; // multiband, one-pole coefficients
    };

    // One pass of the two second-order sections of a Linkwitz-Riley filter of the fourth order.
    struct State
    {
        double s1 = 0.0, s2 = 0.0;

        double run(const StereoEffect::Biquad& k, double x) noexcept
        {
            const double y = k.b0 * x + s1;
            s1 = k.b1 * x - k.a1 * y + s2;
            s2 = k.b2 * x - k.a2 * y;
            return y;
        }
    };
    struct LR4
    {
        State first, second;
        double run(const StereoEffect::Biquad& k, double x) noexcept
        {
            return second.run(k, first.run(k, x));
        }
    };
    struct ChannelSplit
    {
        LR4 low1, high1, low2, high2, alignLow, alignHigh;
    };

    double singleBand(double left, double right, double& outLeft, double& outRight) noexcept;
    void multiBand(double left, double right, double& outLeft, double& outRight) noexcept;

    double sampleRate = 44100.0;
    int channels = 2;
    Block block;

    StereoEffect::Biquad lowPass1, highPass1, lowPass2, highPass2;
    std::array<ChannelSplit, kMaxChannels> splits;

    double detector = 0.0;                   // single band: the held peak of the input
    double reduction = 0.0;                  // single band: the smoothed gain reduction in dB
    std::array<double, kBands> bandGainDb{}; // multiband: the smoothed gain of each band in dB

    juce::SmoothedValue<float> multibandMix{0.0f}; // 0 single band, 1 multiband (10 ms crossfade)
    juce::SmoothedValue<float> threshold{-18.0f};
    juce::SmoothedValue<float> ratio{4.0f};
    juce::SmoothedValue<float> makeup{0.0f};
    juce::SmoothedValue<float> depth{1.0f};
    juce::SmoothedValue<float> upward{1.0f};
    juce::SmoothedValue<float> downward{1.0f};
    juce::SmoothedValue<float> inGain{1.0f};
    juce::SmoothedValue<float> outGain{1.0f};
    std::array<juce::SmoothedValue<float>, kBands> bandLevel;
    bool snapSmoothers = true;
    bool singleRan = false; // the path was run in the last sample, so its state is not at rest
    bool multiRan = false;
};

} // namespace fcm::dsp

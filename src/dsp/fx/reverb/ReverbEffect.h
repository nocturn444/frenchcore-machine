// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/stereo/StereoEffect.h"

#include <array>
#include <vector>

namespace fcm::dsp
{

// A simple room reverb (docs/specs/fx-reverb.md): an eight-line feedback delay network with a
// Householder feedback matrix (Jot and Chaigne, "Digital delay networks for designing artificial
// reverberators", AES 90th Convention, 1991; Julius O. Smith, "Physical Audio Signal Processing", FDN
// reverberation), a few allpass diffusers before it, a one-pole low-pass per line for Damping and a
// little modulation on two lines. The output is the reverb only; the rack's Mix adds the dry sound.
class ReverbEffect : public Effect
{
  public:
    static constexpr int kLines = 8;

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The gain of one pass through a line of `delaySeconds` so that the tail falls 60 dB in `decaySeconds`.
    static double lineGain(double delaySeconds, double decaySeconds) noexcept;

  private:
    static constexpr int kMaxChannels = 2;
    static constexpr int kDiffusers = 4;

    // A delay line read at a fractional position with linear interpolation.
    struct Line
    {
        std::vector<float> data;
        int write = 0;

        void prepare(int length)
        {
            data.assign(static_cast<size_t>(std::max(2, length)), 0.0f);
            write = 0;
        }
        void clear() noexcept
        {
            std::fill(data.begin(), data.end(), 0.0f);
            write = 0;
        }
        void push(float x) noexcept
        {
            data[static_cast<size_t>(write)] = x;
            write = write + 1 == static_cast<int>(data.size()) ? 0 : write + 1;
        }
        // `delay` samples back from the newest sample written (delay >= 1).
        float read(double delay) const noexcept
        {
            const auto size = static_cast<int>(data.size());
            const double clamped = delay < 1.0 ? 1.0 : (delay > size - 2 ? size - 2 : delay);
            const int whole = static_cast<int>(clamped);
            const auto fraction = static_cast<float>(clamped - whole);
            const int newer = (write - whole + size) % size;
            const int older = (newer - 1 + size) % size;
            return data[static_cast<size_t>(newer)] +
                   fraction * (data[static_cast<size_t>(older)] - data[static_cast<size_t>(newer)]);
        }
    };

    struct Allpass
    {
        std::vector<float> data;
        int write = 0;

        void prepare(int length)
        {
            data.assign(static_cast<size_t>(std::max(1, length)), 0.0f);
            write = 0;
        }
        void clear() noexcept
        {
            std::fill(data.begin(), data.end(), 0.0f);
            write = 0;
        }
        // A Schroeder allpass with the delay of its buffer: y = -g x + d; d' = x + g y.
        float run(float x, float g) noexcept
        {
            const float delayed = data[static_cast<size_t>(write)];
            const float y = -g * x + delayed;
            data[static_cast<size_t>(write)] = x + g * y;
            write = write + 1 == static_cast<int>(data.size()) ? 0 : write + 1;
            return y;
        }
    };

    struct Biquad
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

    double sampleRate = 44100.0;
    int channels = 2;

    // The values of the block.
    double sizeScale = 0.8;
    double decaySeconds = 1.2;
    double damping = 0.0; // the one-pole coefficient
    double lowCutHz = 200.0;
    StereoEffect::Biquad lowCut;

    std::array<Line, kLines> lines;
    std::array<double, kLines> baseDelays{};                // in samples at the current rate, at scale 1
    std::array<juce::SmoothedValue<double>, kLines> delays; // the smoothed length of each line, in samples
    std::array<float, kLines> dampState{};
    std::array<Allpass, kDiffusers> diffusers;
    Line predelay;
    juce::SmoothedValue<double> predelaySamples{0.0};
    Biquad lowCutState;
    double lfoPhase[2] = {0.0, 0.25};
    double lfoStep[2] = {0.0, 0.0};
    double modulationDepth = 0.0;
    bool snapSmoothers = true;
};

} // namespace fcm::dsp

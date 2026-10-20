// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"
#include "dsp/fx/NoteTriggers.h"
#include "dsp/fx/TempoLfo.h"

#include <array>

namespace fcm::dsp
{

// A phaser (docs/specs/fx-phaser.md): a chain of first-order all-pass filters tuned to one frequency,
// with feedback, added to the dry sound so that a notch falls every two stages (Julius O. Smith,
// "Physical Audio Signal Processing", the phasing chapter). The all-pass is the topology-preserving
// transform one-pole of Vadim Zavalishin, "The Art of Virtual Analog Filter Design" (2012). The sweep
// is a TempoLfo that restarts at every note.
class PhaserEffect : public Effect
{
  public:
    static constexpr int kMaxStages = 12;

    const EffectDescriptor& descriptor() const override;
    std::span<const ParameterSpec> parameters() const override;
    void prepare(const EffectSpec& spec) override;
    void reset() noexcept override;
    void setParameters(std::span<const float> values) noexcept override;
    void setContext(const EffectContext& newContext) noexcept override { context = newContext; }
    void noteOn(int sampleOffset) noexcept override { pending.add(sampleOffset); }
    void process(juce::AudioBuffer<float>& buffer) noexcept override;

    // The number of all-pass stages of the Stages choice (0, 1, 2 -> 4, 8, 12).
    static int stagesOf(int choice) noexcept;

    // The corner frequency of the all-pass stages that puts the first notch of a chain of `stages`
    // stages at `notchHz`: the notch is where the chain's phase is -180 degrees,
    // 2 * stages * atan(f / corner) = pi.
    static double cornerForFirstNotch(double notchHz, int stages) noexcept;

    // The gain that keeps the peaks between the notches within +6 dB: the chain with feedback peaks
    // at (1 + 1 / (1 - feedback)) / 2 of the dry level, and above 2 the output is scaled back.
    static double levelFor(double feedback) noexcept;

  private:
    static constexpr int kMaxChannels = 2;
    static constexpr int kControlInterval = 8; // samples between coefficient updates

    // One chain of all-pass stages for both channels, with the last output for the feedback.
    struct Chain
    {
        std::array<std::array<double, kMaxStages>, kMaxChannels> state{};
        std::array<double, kMaxChannels> last{};
        void clear() noexcept
        {
            for (auto& channel : state)
                channel.fill(0.0);
            last.fill(0.0);
        }
    };

    double sampleRate = 44100.0;
    int channels = 2;

    bool sync = false;
    int division = 9;
    float rateHz = 0.5f;
    float keyTrack = 0.0f;
    EffectContext context;
    TempoLfo lfo;
    PendingNoteOns pending;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> frequency{300.0f};
    juce::SmoothedValue<float> depth{0.5f};
    juce::SmoothedValue<float> feedback{0.4f};
    bool snapSmoothers = true;

    // Two chains: when Stages changes, the one that was playing fades out while the new one fades in.
    std::array<Chain, 2> chains;
    std::array<int, 2> chainStages{8, 8};
    int current = 0;
    int fadeLeft = 0;
    int fadeTotal = 1;

    // The coefficient of the all-pass stages, valid for the next kControlInterval samples.
    std::array<double, 2> allPassG{};
    int untilUpdate = 0;
};

} // namespace fcm::dsp

// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cmath>

namespace fcm::dsp
{

// Phase 2 placeholder for the amp envelope: a fixed kick shape that Phase 3 replaces with the
// editable BreakpointEnvelope (docs/specs/voice-note-handling.md §4.4). Linear attack to 1, then an
// exponential decay (constant dB per second). It runs to silence on its own and ignores note-off,
// so a short MIDI note still plays the whole kick.
class FixedAmpEnvelope
{
  public:
    static constexpr double kAttackMs = 1.0;
    static constexpr double kDecaySeconds = 0.5;    // time for -60 dB, measured from the end of the attack
    static constexpr float kSilenceLevel = 1.0e-4f; // -80 dB: below this the envelope snaps to 0
    // Upper bound on the time from note-on until the envelope goes idle, for the host's tail length.
    static constexpr double kMaxLengthSeconds = kAttackMs / 1000.0 + kDecaySeconds * 80.0 / 60.0;

    void prepare(double sampleRate) noexcept
    {
        attackStep = static_cast<float>(1.0 / (kAttackMs * 0.001 * sampleRate));
        // 10^(-60/20) = 10^-3 over kDecaySeconds.
        decayCoefficient = static_cast<float>(std::pow(10.0, -3.0 / (kDecaySeconds * sampleRate)));
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
    }

    void start() noexcept
    {
        stage = Stage::attack;
        level = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }

    float next() noexcept
    {
        switch (stage)
        {
        case Stage::attack:
            level += attackStep;
            if (level >= 1.0f)
            {
                level = 1.0f;
                stage = Stage::decay;
            }
            break;
        case Stage::decay:
            level *= decayCoefficient;
            if (level < kSilenceLevel)
            {
                level = 0.0f;
                stage = Stage::idle;
            }
            break;
        case Stage::idle:
            break;
        }
        return level;
    }

  private:
    enum class Stage
    {
        idle,
        attack,
        decay
    };

    Stage stage = Stage::idle;
    float level = 0.0f;
    float attackStep = 1.0f;
    float decayCoefficient = 0.999f;
};

} // namespace fcm::dsp

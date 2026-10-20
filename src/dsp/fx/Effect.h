// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectCurve.h"

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <span>
#include <string_view>

namespace fcm::dsp
{

inline constexpr int kMaxEffects = 16;
inline constexpr int kMaxEffectParameters = 48; // docs/decisions/0011

// Every effect type exists as a fixed pool of copies (docs/decisions/0010). A pool index is
// `effect index * kInstancesPerEffect + (copy - 1)`, so it fits the 6 bits a rack uses per entry.
inline constexpr int kInstancesPerEffect = 4;
inline constexpr int kMaxInstances = kMaxEffects * kInstancesPerEffect;
inline constexpr int kMaxRackEffects = 8;

// How an effect describes one of its parameters, as plain data (docs/decisions/0009). fcm_core turns
// the specs of all registered effects into APVTS parameters; fcm_dsp never includes APVTS.
enum class ParameterKind
{
    toggle,  // off / on, value 0 or 1
    choice,  // one of `choices`, value is the entry number
    integer, // a whole number from minimum to maximum
    number   // a float from minimum to maximum
};

struct ParameterSpec
{
    std::string_view idSuffix; // the full ID is "<effect id>.<idSuffix>"
    std::string_view name;
    ParameterKind kind = ParameterKind::number;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float step = 0.0f; // 0: continuous
    float defaultValue = 0.0f;
    float skewCentre = 0.0f; // 0: linear; otherwise the value that sits at the middle of the control
    std::string_view unit;   // "dB", "ms", ...
    std::span<const std::string_view> choices; // for ParameterKind::choice

    // Only the UI reads these: the control is shown only while the toggle or choice parameter
    // `shownWhenSuffix` of the same effect has a value whose bit is set in `shownWhenMask`
    // (docs/decisions/0011). Empty: always shown. The parameter exists and automates either way.
    std::string_view shownWhenSuffix = {};
    unsigned shownWhenMask = 0;
};

// Stable identity of an effect. `id` never changes after release; `version` bumps when its parameters
// change, with a state migration.
struct EffectDescriptor
{
    std::string_view id; // lowercase, no spaces: "distortion", "eq", "stereo"
    std::string_view displayName;
    int version = 1;
    float defaultMix = 1.0f;    // 0..1, the default of "<id>.mix" (docs/decisions/0011)
    bool inDefaultMain = false; // part of the rack a new project starts with (docs/decisions/0010 item 9)
};

// What the engine hands the chain once per block for one effect: whether it is on, its dry/wet mix
// and the values of its own parameters in the order of Effect::parameters().
struct EffectSlotParameters
{
    bool enabled = false;
    float mix = 1.0f; // 0..1
    std::array<float, kMaxEffectParameters> values{};
};

// What an effect may want to know about the sound it is processing, set once per block before
// process() (docs/decisions/0011). Effects never see MIDI.
struct EffectContext
{
    float noteSemitones = 0.0f; // the pitch the tail settles on, relative to MIDI note 60, with glide
    float tempoBpm = 120.0f;
};

struct EffectSpec
{
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    int numChannels = 2;
};

// An audio effect in the FX chain (docs/specs/fx-chain.md). All allocation happens in prepare();
// reset(), setParameters() and process() are real-time safe. An effect does not know its position
// in the chain and never looks at other effects.
class Effect
{
  public:
    virtual ~Effect() = default;

    virtual const EffectDescriptor& descriptor() const = 0;

    // The effect's own parameters. The array, and every string in it, must be static data of the
    // effect type: the registry keeps views of it after the instance it asked is gone.
    virtual std::span<const ParameterSpec> parameters() const = 0;

    virtual void prepare(const EffectSpec& spec) = 0;
    virtual void reset() noexcept = 0;

    // Once per block, before process(). `values` has one entry per parameter, in the order of
    // parameters(), in the units of the spec (a choice is its entry number, a toggle 0 or 1).
    virtual void setParameters(std::span<const float> values) noexcept = 0;

    // Once per block, before process(), for effects that follow the note or the tempo.
    virtual void setContext(const EffectContext& context) noexcept { juce::ignoreUnused(context); }

    // (from 0.3, docs/decisions/0012 item 1) Told before process(), once for every note-on whose sound
    // reaches this effect inside the coming buffer, in time order. `sampleOffset` counts from the
    // buffer's first sample and is already corrected for the latency of what is in front of the
    // effect, so it is the sample where the note's sound is in the input. Effects that restart
    // something at a note (an LFO) keep the offsets, e.g. in a PendingNoteOns, and cut process() there.
    // Not called while the effect is off or asleep.
    virtual void noteOn(int sampleOffset) noexcept { juce::ignoreUnused(sampleOffset); }

    // (from 0.3, docs/decisions/0012 item 3) An effect with a drawn curve returns its default one
    // (static data of the effect type; nullptr for effects without a curve), the ready-made shapes of
    // its editor menu, and reads the current curve in setCurve() once per block before process()
    // (nullptr means the default). Curves are not parameters: they are saved with the project and
    // published like the envelopes.
    virtual const EffectCurve* defaultCurve() const noexcept { return nullptr; }
    virtual std::span<const NamedCurve> curvePresets() const noexcept { return {}; }
    virtual void setCurve(const EffectCurve* curve) noexcept { juce::ignoreUnused(curve); }

    // The oversampling factor (1, 2, 4 or 8) of the global Quality setting, before process() whenever
    // it changes and once after prepare(). Effects that do not oversample ignore it.
    virtual void setOversampling(int factor) noexcept { juce::ignoreUnused(factor); }

    // Processes the block in place (1 or 2 channels).
    virtual void process(juce::AudioBuffer<float>& buffer) noexcept = 0;

    // Delay of the wet signal in samples at the given oversampling factor (1, 2, 4 or 8). Constant
    // for a given factor, whether the effect is on or not (docs/decisions/0009).
    virtual int latencySamples(int oversamplingFactor) const noexcept
    {
        juce::ignoreUnused(oversamplingFactor);
        return 0;
    }
};

} // namespace fcm::dsp

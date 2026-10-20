// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fcm::dsp
{

// The list of effects the plugin knows, in registration order. That order is the default chain
// order and the index of an effect everywhere (docs/specs/fx-chain.md §4). Only RegisterEffects.cpp
// names concrete effects; nothing else lists them.
class EffectRegistry
{
  public:
    using Factory = std::unique_ptr<Effect> (*)();

    // Appends an effect type. Returns false (and adds nothing) if the registry is full, the ID is
    // empty or already taken, or the effect has more parameters than the limit.
    bool add(Factory factory);

    template <typename EffectType> bool add()
    {
        return add([]() -> std::unique_ptr<Effect> { return std::make_unique<EffectType>(); });
    }

    int count() const noexcept { return static_cast<int>(entries.size()); }

    // Nullptr when the index is out of range.
    const EffectDescriptor* descriptor(int index) const noexcept;
    std::span<const ParameterSpec> parameters(int index) const noexcept;

    // (from 0.3) The effect's default drawn curve (nullptr when it has none) and its ready-made
    // shapes. Static data of the effect type, like the parameter specs.
    const EffectCurve* defaultCurve(int index) const noexcept;
    std::span<const NamedCurve> curvePresets(int index) const noexcept;

    // Index of the effect with this ID, or -1.
    int indexOf(std::string_view id) const noexcept;

    // The pool of copies (docs/decisions/0010): every effect has kInstancesPerEffect instances.
    // Instance 1 of an effect keeps its plain ID ("distortion"), the others add the copy number
    // ("distortion2"). A pool index is `effect * kInstancesPerEffect + (copy - 1)`.
    int instanceCount() const noexcept { return count() * kInstancesPerEffect; }
    static constexpr int instanceIndex(int effectIndex, int copy) noexcept
    {
        return effectIndex * kInstancesPerEffect + (copy - 1);
    }
    static constexpr int effectOf(int instance) noexcept { return instance / kInstancesPerEffect; }
    static constexpr int copyOf(int instance) noexcept { return instance % kInstancesPerEffect + 1; }

    // "distortion", "distortion2", ... Empty when the instance does not exist.
    std::string instanceId(int instance) const;
    // The pool index of an instance ID, or -1.
    int indexOfInstance(std::string_view instanceId) const noexcept;

    // A new instance of effect `index`.
    std::unique_ptr<Effect> create(int index) const;

  private:
    struct Entry
    {
        Factory factory;
        EffectDescriptor descriptor; // copied from a throw-away instance (the views are static data)
        std::span<const ParameterSpec> parameters;
        const EffectCurve* defaultCurve = nullptr;
        std::span<const NamedCurve> curvePresets;
    };

    std::vector<Entry> entries;
};

// Registers every effect of the plugin (RegisterEffects.cpp).
void registerEffects(EffectRegistry& registry);

// A registry with every effect of the plugin, built once.
const EffectRegistry& defaultEffectRegistry();

} // namespace fcm::dsp

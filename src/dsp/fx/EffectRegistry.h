// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/Effect.h"

#include <memory>
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

    // Index of the effect with this ID, or -1.
    int indexOf(std::string_view id) const noexcept;

    // A new instance of effect `index`.
    std::unique_ptr<Effect> create(int index) const;

  private:
    struct Entry
    {
        Factory factory;
        EffectDescriptor descriptor; // copied from a throw-away instance (the views are static data)
        std::span<const ParameterSpec> parameters;
    };

    std::vector<Entry> entries;
};

// Registers every effect of the plugin (RegisterEffects.cpp).
void registerEffects(EffectRegistry& registry);

// A registry with every effect of the plugin, built once.
const EffectRegistry& defaultEffectRegistry();

} // namespace fcm::dsp

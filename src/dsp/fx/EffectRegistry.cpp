// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/EffectRegistry.h"

namespace fcm::dsp
{

bool EffectRegistry::add(Factory factory)
{
    if (factory == nullptr || count() >= kMaxEffects)
        return false;

    const auto probe = factory();
    if (probe == nullptr)
        return false;

    const EffectDescriptor& descriptor = probe->descriptor();
    const auto parameters = probe->parameters();
    if (descriptor.id.empty() || indexOf(descriptor.id) >= 0 ||
        parameters.size() > static_cast<size_t>(kMaxEffectParameters))
        return false;

    // The id, the name and the parameter specs are static data of the effect type, so the views stay
    // valid after the probe is gone (docs/specs/fx-chain.md §4).
    entries.push_back({factory, descriptor, parameters});
    return true;
}

const EffectDescriptor* EffectRegistry::descriptor(int index) const noexcept
{
    return index >= 0 && index < count() ? &entries[static_cast<size_t>(index)].descriptor : nullptr;
}

std::span<const ParameterSpec> EffectRegistry::parameters(int index) const noexcept
{
    return index >= 0 && index < count() ? entries[static_cast<size_t>(index)].parameters
                                         : std::span<const ParameterSpec>{};
}

int EffectRegistry::indexOf(std::string_view id) const noexcept
{
    for (int i = 0; i < count(); ++i)
        if (entries[static_cast<size_t>(i)].descriptor.id == id)
            return i;
    return -1;
}

std::unique_ptr<Effect> EffectRegistry::create(int index) const
{
    return index >= 0 && index < count() ? entries[static_cast<size_t>(index)].factory() : nullptr;
}

const EffectRegistry& defaultEffectRegistry()
{
    static const EffectRegistry registry = []
    {
        EffectRegistry built;
        registerEffects(built);
        return built;
    }();
    return registry;
}

} // namespace fcm::dsp

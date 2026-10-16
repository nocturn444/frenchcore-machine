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

    // An instance ID is the effect ID plus a copy number, so no effect ID may be another one with
    // a copy number appended: the IDs must read back unambiguously.
    for (const auto& entry : entries)
        for (int copy = 2; copy <= kInstancesPerEffect; ++copy)
        {
            const std::string suffix = std::to_string(copy);
            if (std::string(entry.descriptor.id) + suffix == descriptor.id ||
                std::string(descriptor.id) + suffix == entry.descriptor.id)
                return false;
        }

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

std::string EffectRegistry::instanceId(int instance) const
{
    if (instance < 0 || instance >= instanceCount())
        return {};

    std::string id(entries[static_cast<size_t>(effectOf(instance))].descriptor.id);
    if (copyOf(instance) > 1)
        id += std::to_string(copyOf(instance));
    return id;
}

int EffectRegistry::indexOfInstance(std::string_view instanceId) const noexcept
{
    for (int effect = 0; effect < count(); ++effect)
    {
        const std::string_view id = entries[static_cast<size_t>(effect)].descriptor.id;
        if (instanceId == id)
            return instanceIndex(effect, 1);
        if (instanceId.size() == id.size() + 1 && instanceId.substr(0, id.size()) == id)
        {
            const char digit = instanceId.back();
            if (digit >= '2' && digit < static_cast<char>('1' + kInstancesPerEffect))
                return instanceIndex(effect, digit - '0');
        }
    }
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

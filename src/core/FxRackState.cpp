// SPDX-License-Identifier: AGPL-3.0-only
#include "core/FxRackState.h"

#include "core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace fcm::core
{

namespace
{
constexpr int kMaxSavedCharacters = 2048;
constexpr int kMaxSavedRackNodes = 16;

const char* const kRackNames[dsp::kNumRacks] = {"busA", "busB", "main"};

size_t slot(dsp::RackId rack) noexcept
{
    return static_cast<size_t>(rack);
}
} // namespace

FxRackState::FxRackState(juce::AudioProcessorValueTreeState& parametersIn,
                         const dsp::EffectRegistry& registryIn,
                         std::array<std::atomic<std::uint64_t>*, dsp::kNumRacks> targetsIn)
    : parameters(parametersIn), registry(registryIn), targets(targetsIn)
{
    setDefault();
}

const juce::Identifier& FxRackState::nodeType()
{
    static const juce::Identifier type{"FxRacks"};
    return type;
}

const char* FxRackState::rackName(dsp::RackId rack) noexcept
{
    return kRackNames[slot(rack)];
}

int FxRackState::positionAt(double x, double slotWidth, int count) noexcept
{
    if (count <= 1 || !std::isfinite(x) || !std::isfinite(slotWidth) || slotWidth <= 0.0)
        return 0;
    return std::clamp(static_cast<int>(std::floor(x / slotWidth)), 0, count - 1);
}

int FxRackState::insertionIndex(const std::vector<double>& otherSizes, double offset) noexcept
{
    if (!std::isfinite(offset))
        return 0;

    int index = 0;
    double start = 0.0;
    for (const double size : otherSizes)
    {
        const double usable = std::isfinite(size) && size > 0.0 ? size : 0.0;
        if (offset < start + 0.5 * usable)
            break;
        start += usable;
        ++index;
    }
    return index;
}

const std::vector<std::string>& FxRackState::ids(dsp::RackId rack) const noexcept
{
    return racks[slot(rack)];
}

void FxRackState::publish()
{
    for (size_t r = 0; r < racks.size(); ++r)
    {
        std::vector<int> indices;
        for (const auto& id : racks[r])
            indices.push_back(registry.indexOfInstance(id));
        if (targets[r] != nullptr)
            targets[r]->store(dsp::EffectRack::pack(indices));
    }
}

bool FxRackState::isUsed(const std::string& instanceId) const noexcept
{
    for (const auto& rack : racks)
        if (std::find(rack.begin(), rack.end(), instanceId) != rack.end())
            return true;
    return false;
}

int FxRackState::lowestFreeInstance(int effectIndex) const noexcept
{
    for (int copy = 1; copy <= dsp::kInstancesPerEffect; ++copy)
    {
        const int instance = dsp::EffectRegistry::instanceIndex(effectIndex, copy);
        if (!isUsed(registry.instanceId(instance)))
            return instance;
    }
    return -1;
}

int FxRackState::freeCopies(int effectIndex) const noexcept
{
    if (effectIndex < 0 || effectIndex >= registry.count())
        return 0;

    int free = 0;
    for (int copy = 1; copy <= dsp::kInstancesPerEffect; ++copy)
        if (!isUsed(registry.instanceId(dsp::EffectRegistry::instanceIndex(effectIndex, copy))))
            ++free;
    return free;
}

bool FxRackState::canAdd(dsp::RackId rack, int effectIndex) const noexcept
{
    return static_cast<int>(racks[slot(rack)].size()) < dsp::kMaxRackEffects && freeCopies(effectIndex) > 0;
}

bool FxRackState::canDuplicate(dsp::RackId rack, const std::string& instanceId) const noexcept
{
    const auto& list = racks[slot(rack)];
    const int instance = registry.indexOfInstance(instanceId);
    return instance >= 0 && std::find(list.begin(), list.end(), instanceId) != list.end() &&
           canAdd(rack, dsp::EffectRegistry::effectOf(instance));
}

void FxRackState::setInstanceToDefaults(int instance)
{
    const std::string id = registry.instanceId(instance);
    const int effectIndex = dsp::EffectRegistry::effectOf(instance);

    const auto setDefault = [this](const std::string& parameterId)
    {
        if (auto* parameter = parameters.getParameter(parameterId))
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
    };

    setDefault(ParameterIds::effect(id, ParameterIds::kEffectEnabled));
    setDefault(ParameterIds::effect(id, ParameterIds::kEffectMix));
    for (const auto& spec : registry.parameters(effectIndex))
        setDefault(ParameterIds::effect(id, spec.idSuffix));
}

void FxRackState::copyInstance(int from, int to)
{
    const std::string fromId = registry.instanceId(from);
    const std::string toId = registry.instanceId(to);
    const int effectIndex = dsp::EffectRegistry::effectOf(from);

    const auto copy = [this](const std::string& source, const std::string& target)
    {
        auto* sourceParameter = parameters.getParameter(source);
        auto* targetParameter = parameters.getParameter(target);
        if (sourceParameter != nullptr && targetParameter != nullptr)
            targetParameter->setValueNotifyingHost(sourceParameter->getValue());
    };

    copy(ParameterIds::effect(fromId, ParameterIds::kEffectEnabled),
         ParameterIds::effect(toId, ParameterIds::kEffectEnabled));
    copy(ParameterIds::effect(fromId, ParameterIds::kEffectMix),
         ParameterIds::effect(toId, ParameterIds::kEffectMix));
    for (const auto& spec : registry.parameters(effectIndex))
        copy(ParameterIds::effect(fromId, spec.idSuffix), ParameterIds::effect(toId, spec.idSuffix));
}

std::string FxRackState::add(dsp::RackId rack, int effectIndex)
{
    if (!canAdd(rack, effectIndex))
        return {};

    const int instance = lowestFreeInstance(effectIndex);
    if (instance < 0)
        return {};

    setInstanceToDefaults(instance);
    if (auto* enabled = parameters.getParameter(
            ParameterIds::effect(registry.instanceId(instance), ParameterIds::kEffectEnabled)))
        enabled->setValueNotifyingHost(1.0f);

    racks[slot(rack)].push_back(registry.instanceId(instance));
    publish();
    sendChangeMessage();
    return registry.instanceId(instance);
}

std::string FxRackState::duplicate(dsp::RackId rack, const std::string& instanceId)
{
    if (!canDuplicate(rack, instanceId))
        return {};

    const int source = registry.indexOfInstance(instanceId);
    const int instance = lowestFreeInstance(dsp::EffectRegistry::effectOf(source));
    if (instance < 0)
        return {};

    copyInstance(source, instance);

    auto& list = racks[slot(rack)];
    const auto position = std::find(list.begin(), list.end(), instanceId);
    list.insert(position + 1, registry.instanceId(instance));
    publish();
    sendChangeMessage();
    return registry.instanceId(instance);
}

bool FxRackState::remove(dsp::RackId rack, const std::string& instanceId)
{
    auto& list = racks[slot(rack)];
    const auto position = std::find(list.begin(), list.end(), instanceId);
    if (position == list.end())
        return false;

    list.erase(position);
    if (auto* enabled =
            parameters.getParameter(ParameterIds::effect(instanceId, ParameterIds::kEffectEnabled)))
        enabled->setValueNotifyingHost(0.0f);

    publish();
    sendChangeMessage();
    return true;
}

void FxRackState::move(dsp::RackId rack, int from, int to)
{
    auto& list = racks[slot(rack)];
    const int last = static_cast<int>(list.size()) - 1;
    if (last < 0)
        return;

    from = std::clamp(from, 0, last);
    to = std::clamp(to, 0, last);
    if (from == to)
        return;

    const auto id = list[static_cast<size_t>(from)];
    list.erase(list.begin() + from);
    list.insert(list.begin() + to, id);
    publish();
    sendChangeMessage();
}

void FxRackState::setDefault()
{
    for (auto& rack : racks)
        rack.clear();

    for (int index = 0; index < registry.count(); ++index)
        if (registry.descriptor(index)->inDefaultMain)
            racks[slot(dsp::RackId::main)].push_back(
                registry.instanceId(dsp::EffectRegistry::instanceIndex(index, 1)));

    publish();
    sendChangeMessage();
}

juce::ValueTree FxRackState::toValueTree() const
{
    juce::ValueTree node(nodeType());
    for (size_t r = 0; r < racks.size(); ++r)
    {
        juce::StringArray names;
        for (const auto& id : racks[r])
            names.add(juce::String(id));

        juce::ValueTree rack("Rack");
        rack.setProperty("id", kRackNames[r], nullptr);
        rack.setProperty("effects", names.joinIntoString(","), nullptr);
        node.appendChild(rack, nullptr);
    }
    return node;
}

bool FxRackState::restoreFrom(const juce::ValueTree& node)
{
    if (!node.isValid() || node.getType() != nodeType())
    {
        setDefault();
        return false;
    }

    std::array<std::vector<std::string>, dsp::kNumRacks> loaded;
    std::vector<std::string> used;

    const int nodes = std::min(node.getNumChildren(), kMaxSavedRackNodes);
    for (int n = 0; n < nodes; ++n)
    {
        const auto child = node.getChild(n);
        if (child.getType().toString() != "Rack")
            continue;

        const juce::String name = child["id"].toString();
        int rackIndex = -1;
        for (int r = 0; r < dsp::kNumRacks; ++r)
            if (name == kRackNames[r])
                rackIndex = r;
        if (rackIndex < 0 || !loaded[static_cast<size_t>(rackIndex)].empty())
            continue;

        const juce::String text = child["effects"].toString();
        if (text.length() > kMaxSavedCharacters)
            continue;

        for (const auto& token : juce::StringArray::fromTokens(text, ",", ""))
        {
            auto& rack = loaded[static_cast<size_t>(rackIndex)];
            if (static_cast<int>(rack.size()) >= dsp::kMaxRackEffects)
                break;

            const std::string id = token.trim().toStdString();
            if (registry.indexOfInstance(id) < 0 || std::find(used.begin(), used.end(), id) != used.end())
                continue;
            used.push_back(id);
            rack.push_back(id);
        }
    }

    racks = std::move(loaded);
    publish();
    sendChangeMessage();
    return true;
}

} // namespace fcm::core

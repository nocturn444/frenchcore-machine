// SPDX-License-Identifier: AGPL-3.0-only
#include "core/FxOrderState.h"

#include "dsp/fx/EffectChain.h"

#include <algorithm>
#include <cmath>

namespace fcm::core
{

namespace
{
constexpr int kMaxSavedCharacters = 2048;

std::vector<std::string> registrationOrder(const dsp::EffectRegistry& registry)
{
    std::vector<std::string> ids;
    for (int i = 0; i < registry.count(); ++i)
        ids.emplace_back(registry.descriptor(i)->id);
    return ids;
}
} // namespace

FxOrderState::FxOrderState(const dsp::EffectRegistry& registryIn, std::atomic<std::uint64_t>& targetIn)
    : registry(registryIn), target(targetIn), order(registrationOrder(registryIn))
{
    publish();
}

const juce::Identifier& FxOrderState::nodeType()
{
    static const juce::Identifier type{"FxChain"};
    return type;
}

int FxOrderState::positionAt(double x, double slotWidth, int count) noexcept
{
    if (count <= 1 || !std::isfinite(x) || !std::isfinite(slotWidth) || slotWidth <= 0.0)
        return 0;
    return std::clamp(static_cast<int>(std::floor(x / slotWidth)), 0, count - 1);
}

void FxOrderState::publish()
{
    std::vector<int> indices;
    for (const auto& id : order)
        indices.push_back(registry.indexOf(id));
    target.store(dsp::EffectChain::packOrder(indices));
}

void FxOrderState::set(const std::vector<std::string>& wanted)
{
    std::vector<std::string> clean;
    for (const auto& id : wanted)
        if (registry.indexOf(id) >= 0 && std::find(clean.begin(), clean.end(), id) == clean.end())
            clean.push_back(id);

    // Registered effects that were not named keep their registration order, at the end.
    for (const auto& id : registrationOrder(registry))
        if (std::find(clean.begin(), clean.end(), id) == clean.end())
            clean.push_back(id);

    if (clean == order)
        return;

    order = std::move(clean);
    publish();
    sendChangeMessage();
}

void FxOrderState::move(int from, int to)
{
    const int last = static_cast<int>(order.size()) - 1;
    if (last < 0)
        return;

    from = std::clamp(from, 0, last);
    to = std::clamp(to, 0, last);
    if (from == to)
        return;

    auto changed = order;
    const auto id = changed[static_cast<size_t>(from)];
    changed.erase(changed.begin() + from);
    changed.insert(changed.begin() + to, id);
    set(changed);
}

juce::ValueTree FxOrderState::toValueTree() const
{
    juce::StringArray names;
    for (const auto& id : order)
        names.add(juce::String(id));

    juce::ValueTree node(nodeType());
    node.setProperty("order", names.joinIntoString(","), nullptr);
    return node;
}

bool FxOrderState::restoreFrom(const juce::ValueTree& node)
{
    if (!node.isValid() || node.getType() != nodeType())
    {
        set({});
        return false;
    }

    const juce::String text = node["order"].toString();
    if (text.isEmpty() || text.length() > kMaxSavedCharacters)
    {
        set({});
        return false;
    }

    std::vector<std::string> wanted;
    for (const auto& token : juce::StringArray::fromTokens(text, ",", ""))
        wanted.push_back(token.trim().toStdString());
    set(wanted);
    return true;
}

} // namespace fcm::core

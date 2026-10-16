// SPDX-License-Identifier: AGPL-3.0-only
#include "core/StateMigrations.h"

#include "core/ParameterIds.h"

#include <cmath>
#include <limits>

namespace fcm::core
{

namespace
{
const juce::Identifier kStateVersionProperty{"stateVersion"};
constexpr int kLargestVersionRead = 1'000'000;
} // namespace

int readStateVersion(const juce::ValueTree& state)
{
    const juce::var stored = state.getProperty(kStateVersionProperty);

    double number = 1.0;
    if (stored.isInt() || stored.isInt64() || stored.isDouble())
    {
        number = static_cast<double>(stored);
    }
    else if (stored.isString())
    {
        // Only plain digits: anything else (text, "nan", signs, decimals) is not a version.
        const juce::String text = stored.toString().trim();
        if (text.isNotEmpty() && text.containsOnly("0123456789"))
            number = text.getDoubleValue();
    }

    if (!std::isfinite(number) || number < 1.0)
        return 1;
    return number > kLargestVersionRead ? kLargestVersionRead : static_cast<int>(number);
}

bool applyMigrations(juce::ValueTree& state, int fromVersion, int toVersion, std::span<const Migration> steps)
{
    for (int version = fromVersion; version < toVersion; ++version)
    {
        const Migration* step = nullptr;
        for (const auto& candidate : steps)
            if (candidate.fromVersion == version)
                step = &candidate;

        if (step == nullptr || step->apply == nullptr)
            return false;

        step->apply(state);
        state.setProperty(kStateVersionProperty, version + 1, nullptr);
    }
    return true;
}

namespace
{
// Version 1 to 2 (docs/decisions/0010 item 8): the single 0.1 effect chain becomes the Main rack.
// The 0.1 chain always held its three effects, in the saved order; the buses start empty. Whatever
// the saved order lacks is appended in the 0.1 default order, as FxOrderState did.
void migrateFrom1To2(juce::ValueTree& state)
{
    static const char* const kEffectsOf01[] = {"distortion", "eq", "stereo"};

    juce::StringArray order;
    const auto chain = state.getChildWithName("FxChain");
    if (chain.isValid())
    {
        for (const auto& token : juce::StringArray::fromTokens(chain["order"].toString(), ",", ""))
        {
            const auto id = token.trim();
            for (const char* known : kEffectsOf01)
                if (id == known && !order.contains(id))
                    order.add(id);
        }
        state.removeChild(chain, nullptr);
    }
    for (const char* known : kEffectsOf01)
        if (!order.contains(known))
            order.add(known);

    const auto rack = [](const char* id, const juce::String& effects)
    {
        juce::ValueTree node("Rack");
        node.setProperty("id", id, nullptr);
        node.setProperty("effects", effects, nullptr);
        return node;
    };

    juce::ValueTree racks("FxRacks");
    racks.appendChild(rack("busA", {}), nullptr);
    racks.appendChild(rack("busB", {}), nullptr);
    racks.appendChild(rack("main", order.joinIntoString(",")), nullptr);

    state.removeChild(state.getChildWithName("FxRacks"), nullptr);
    state.appendChild(racks, nullptr);

    // The Tock's Amp Env switch (docs/specs/tock.md §8) is off for new sounds, but a 0.1 Tock was always
    // shaped by the amp envelope, so a migrated state turns it on.
    for (const auto& child : state)
        if (child.hasType("PARAM") && child["id"].toString() == ParameterIds::kTockAmpEnv)
            return;
    juce::ValueTree ampEnv("PARAM");
    ampEnv.setProperty("id", juce::String(ParameterIds::kTockAmpEnv), nullptr);
    ampEnv.setProperty("value", 1, nullptr);
    state.appendChild(ampEnv, nullptr);
}
} // namespace

std::span<const Migration> stateMigrations() noexcept
{
    // Add one entry here, and bump kStateVersion, whenever the state format changes.
    static_assert(kStateVersion == 2, "kStateVersion changed: add the migration from the previous version");
    static const Migration steps[] = {{1, &migrateFrom1To2}};
    return steps;
}

} // namespace fcm::core

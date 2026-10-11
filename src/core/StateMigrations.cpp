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

std::span<const Migration> stateMigrations() noexcept
{
    // Add one entry here, and bump kStateVersion, whenever the state format changes.
    static_assert(kStateVersion == 1, "kStateVersion changed: add the migration from the previous version");
    return {};
}

} // namespace fcm::core

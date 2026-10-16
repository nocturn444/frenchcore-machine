// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EffectDecorations.h"

#include "ui/EqDisplay.h"

namespace fcm::ui
{

const EffectDecoration* decorationFor(std::string_view effectId)
{
    static const EffectDecoration eq{
        96,
        [](juce::AudioProcessorValueTreeState& state, const juce::String& instanceId,
           const std::function<float()>& note) -> std::unique_ptr<juce::Component>
        { return std::make_unique<EqDisplay>(state, instanceId, note); }};

    return effectId == "eq" ? &eq : nullptr;
}

const std::vector<CopyOnSwitch>& copyOnSwitchRules(std::string_view effectId)
{
    static const std::vector<CopyOnSwitch> eq = []
    {
        std::vector<CopyOnSwitch> rules;
        for (int band = 1; band <= 4; ++band)
        {
            const std::string prefix = "b" + std::to_string(band) + ".";
            rules.push_back({prefix + "extreme", prefix + "gain", prefix + "xgain"});
            rules.push_back({prefix + "extreme", prefix + "q", prefix + "xq"});
        }
        return rules;
    }();
    static const std::vector<CopyOnSwitch> none;

    return effectId == "eq" ? eq : none;
}

} // namespace fcm::ui

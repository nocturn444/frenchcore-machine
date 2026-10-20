// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EffectDecorations.h"

#include "ui/CurveThumbnail.h"
#include "ui/EqDisplay.h"
#include "ui/PeakPad.h"

namespace fcm::ui
{

const EffectDecoration* decorationFor(std::string_view effectId)
{
    static const EffectDecoration eq{
        96, [](const DecorationContext& context) -> std::unique_ptr<juce::Component>
        { return std::make_unique<EqDisplay>(context.state, context.instanceId, context.environment.note); }};

    // The Peak's XY pad (docs/specs/fx-peak.md section 2).
    static const EffectDecoration peak{
        92, [](const DecorationContext& context) -> std::unique_ptr<juce::Component>
        { return std::make_unique<PeakPad>(context.state, context.instanceId); }};

    // The Volume LFO's shape picture, which opens the shape editor (docs/specs/fx-volumelfo.md section 2).
    static const EffectDecoration volumeLfo{
        60, [](const DecorationContext& context) -> std::unique_ptr<juce::Component>
        { return std::make_unique<CurveThumbnail>(context); }};

    if (effectId == "peak")
        return &peak;
    if (effectId == "volumelfo")
        return &volumeLfo;
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

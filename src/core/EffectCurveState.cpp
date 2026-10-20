// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EffectCurveState.h"

#include <cmath>

namespace fcm::core
{

namespace
{
const juce::Identifier kCurveType{"Curve"};
const juce::Identifier kPointType{"Point"};
const juce::Identifier kInstance{"instance"};
const juce::Identifier kX{"x"};
const juce::Identifier kY{"y"};
const juce::Identifier kBend{"curve"};

// A hostile file may hold any number of nodes; look at no more than every instance could use.
constexpr int kMaxCurveNodes = 4 * dsp::kMaxEffects * dsp::kInstancesPerEffect;
} // namespace

EffectCurveState::EffectCurveState(const dsp::EffectRegistry& source) : registry(source)
{
    entries.resize(static_cast<size_t>(registry.instanceCount()));
    for (int i = 0; i < registry.instanceCount(); ++i)
    {
        const auto* defaultCurve = registry.defaultCurve(dsp::EffectRegistry::effectOf(i));
        if (defaultCurve == nullptr)
            continue;
        auto& entry = entries[static_cast<size_t>(i)];
        entry.has = true;
        entry.curve = *defaultCurve;
        entry.published = std::make_unique<dsp::PublishedPointer<dsp::EffectCurve>>();
        publish(entry);
    }
}

EffectCurveState::Entry* EffectCurveState::entryFor(int instance) noexcept
{
    if (instance < 0 || instance >= static_cast<int>(entries.size()) ||
        !entries[static_cast<size_t>(instance)].has)
        return nullptr;
    return &entries[static_cast<size_t>(instance)];
}

const EffectCurveState::Entry* EffectCurveState::entryFor(int instance) const noexcept
{
    return const_cast<EffectCurveState*>(this)->entryFor(instance);
}

void EffectCurveState::publish(Entry& entry)
{
    entry.published->publish(std::make_unique<const dsp::EffectCurve>(entry.curve));
}

bool EffectCurveState::hasCurve(int instance) const noexcept
{
    return entryFor(instance) != nullptr;
}

const dsp::EffectCurve& EffectCurveState::get(int instance) const noexcept
{
    const auto* entry = entryFor(instance);
    return entry != nullptr ? entry->curve : none;
}

dsp::PublishedPointer<dsp::EffectCurve>* EffectCurveState::source(int instance) noexcept
{
    auto* entry = entryFor(instance);
    return entry != nullptr ? entry->published.get() : nullptr;
}

bool EffectCurveState::set(int instance, const dsp::EffectCurve& curve)
{
    auto* entry = entryFor(instance);
    if (entry == nullptr || !curve.isValid())
        return false;

    entry->curve = curve;
    publish(*entry);
    sendChangeMessage();
    return true;
}

void EffectCurveState::reset(int instance)
{
    auto* entry = entryFor(instance);
    const auto* defaultCurve = registry.defaultCurve(dsp::EffectRegistry::effectOf(instance));
    if (entry == nullptr || defaultCurve == nullptr)
        return;

    entry->curve = *defaultCurve;
    publish(*entry);
    sendChangeMessage();
}

void EffectCurveState::copy(int from, int to)
{
    auto* source = entryFor(from);
    auto* target = entryFor(to);
    if (source == nullptr || target == nullptr || from == to)
        return;

    target->curve = source->curve;
    publish(*target);
    sendChangeMessage();
}

const juce::Identifier& EffectCurveState::nodeType()
{
    static const juce::Identifier type{"EffectCurves"};
    return type;
}

juce::ValueTree EffectCurveState::toValueTree() const
{
    juce::ValueTree node(nodeType());
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        const auto& entry = entries[static_cast<size_t>(i)];
        if (!entry.has)
            continue;

        juce::ValueTree curve(kCurveType);
        curve.setProperty(kInstance, juce::String(registry.instanceId(i)), nullptr);
        for (int p = 0; p < entry.curve.count; ++p)
        {
            const auto& point = entry.curve.points[static_cast<size_t>(p)];
            juce::ValueTree child(kPointType);
            child.setProperty(kX, static_cast<double>(point.x), nullptr);
            child.setProperty(kY, static_cast<double>(point.y), nullptr);
            child.setProperty(kBend, static_cast<double>(point.curve), nullptr);
            curve.appendChild(child, nullptr);
        }
        node.appendChild(curve, nullptr);
    }
    return node;
}

void EffectCurveState::restoreFrom(const juce::ValueTree& node)
{
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        reset(i);

    if (!node.isValid() || node.getType() != nodeType())
        return;

    std::vector<bool> seen(entries.size(), false);
    const int nodes = std::min(node.getNumChildren(), kMaxCurveNodes);
    for (int n = 0; n < nodes; ++n)
    {
        const auto child = node.getChild(n);
        if (child.getType() != kCurveType || !child.hasProperty(kInstance))
            continue;

        const int instance = registry.indexOfInstance(child[kInstance].toString().toStdString());
        if (instance < 0 || seen[static_cast<size_t>(instance)] || !hasCurve(instance))
            continue;
        seen[static_cast<size_t>(instance)] = true;

        if (child.getNumChildren() > dsp::kMaxCurvePoints)
            continue;

        dsp::EffectCurve curve;
        bool usable = true;
        for (const auto& pointNode : child)
        {
            if (pointNode.getType() != kPointType || !pointNode.hasProperty(kX) ||
                !pointNode.hasProperty(kY) || !pointNode.hasProperty(kBend) ||
                curve.count >= dsp::kMaxCurvePoints)
            {
                usable = false;
                break;
            }
            curve.points[static_cast<size_t>(curve.count++)] = {
                static_cast<float>(static_cast<double>(pointNode[kX])),
                static_cast<float>(static_cast<double>(pointNode[kY])),
                static_cast<float>(static_cast<double>(pointNode[kBend]))};
        }

        // An invalid curve leaves the instance at its default (set() refuses it).
        if (usable)
            set(instance, curve);
    }
}

} // namespace fcm::core

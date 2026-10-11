// SPDX-License-Identifier: AGPL-3.0-only
#include "core/StateLoader.h"

#include "core/ParameterIds.h"
#include "core/StateMigrations.h"

#include <cmath>
#include <map>
#include <optional>
#include <set>

namespace fcm::core
{

namespace
{
const juce::Identifier kParamType{"PARAM"};
const juce::Identifier kIdProperty{"id"};
const juce::Identifier kValueProperty{"value"};
const juce::Identifier kStateVersionProperty{"stateVersion"};

// True for plain decimal numbers only: optional sign, digits with at most one '.', optional
// exponent. JUCE's own parser is lenient ("--1" gives 0, "nan" gives NaN), so check first.
bool looksLikeNumber(const juce::String& text)
{
    const auto length = text.length();
    int i = 0;
    if (i < length && (text[i] == '+' || text[i] == '-'))
        ++i;

    int digits = 0;
    while (i < length && text[i] >= '0' && text[i] <= '9')
        ++i, ++digits;
    if (i < length && text[i] == '.')
    {
        ++i;
        while (i < length && text[i] >= '0' && text[i] <= '9')
            ++i, ++digits;
    }
    if (digits == 0)
        return false;

    if (i < length && (text[i] == 'e' || text[i] == 'E'))
    {
        ++i;
        if (i < length && (text[i] == '+' || text[i] == '-'))
            ++i;
        int exponentDigits = 0;
        while (i < length && text[i] >= '0' && text[i] <= '9')
            ++i, ++exponentDigits;
        if (exponentDigits == 0)
            return false;
    }
    return i == length;
}

// A saved value as a finite number, or nothing. Text must look like a plain number: JUCE would
// otherwise turn "nan" or "inf" into a real NaN or infinity.
std::optional<double> readFiniteNumber(const juce::var& stored)
{
    double number = 0.0;
    if (stored.isInt() || stored.isInt64() || stored.isDouble() || stored.isBool())
    {
        number = static_cast<double>(stored);
    }
    else if (stored.isString())
    {
        const juce::String text = stored.toString().trim();
        if (!looksLikeNumber(text))
            return std::nullopt;
        number = text.getDoubleValue();
    }
    else
    {
        return std::nullopt;
    }

    if (!std::isfinite(number))
        return std::nullopt;
    return number;
}

double defaultValue(const juce::RangedAudioParameter& parameter)
{
    return static_cast<double>(parameter.convertFrom0to1(parameter.getDefaultValue()));
}

double clampToRange(const juce::RangedAudioParameter& parameter, double value)
{
    const auto& range = parameter.getNormalisableRange();
    const auto clipped =
        juce::jlimit(static_cast<double>(range.start), static_cast<double>(range.end), value);
    return static_cast<double>(range.snapToLegalValue(static_cast<float>(clipped)));
}

std::map<juce::String, const juce::RangedAudioParameter*>
parametersById(const juce::AudioProcessorValueTreeState& state)
{
    std::map<juce::String, const juce::RangedAudioParameter*> result;
    for (auto* parameter : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter))
            result[ranged->paramID] = ranged;
    return result;
}
} // namespace

void sanitizeSavedParameters(juce::ValueTree& saved, const juce::AudioProcessorValueTreeState& state)
{
    const auto parameters = parametersById(state);

    for (int i = saved.getNumChildren(); --i >= 0;)
    {
        auto child = saved.getChild(i);
        if (child.getType() != kParamType)
            continue;

        const auto found = parameters.find(child[kIdProperty].toString());
        if (found == parameters.end() || !child.hasProperty(kValueProperty))
        {
            // Unknown control (maybe from a newer version) or no value to use: ignore the entry.
            saved.removeChild(i, nullptr);
            continue;
        }

        const auto& parameter = *found->second;
        const auto number = readFiniteNumber(child[kValueProperty]);
        child.setProperty(kValueProperty,
                          number.has_value() ? clampToRange(parameter, *number) : defaultValue(parameter),
                          nullptr);
    }
}

void addMissingParameters(juce::ValueTree& saved, const juce::AudioProcessorValueTreeState& state)
{
    std::set<juce::String> present;
    for (int i = 0; i < saved.getNumChildren(); ++i)
        if (const auto child = saved.getChild(i); child.getType() == kParamType)
            present.insert(child[kIdProperty].toString());

    // A saved state describes everything: a control it does not mention takes its default value
    // instead of keeping what it had (docs/specs/project-state.md).
    for (const auto& [id, parameter] : parametersById(state))
    {
        if (present.count(id) > 0)
            continue;

        juce::ValueTree entry(kParamType);
        entry.setProperty(kIdProperty, id, nullptr);
        entry.setProperty(kValueProperty, defaultValue(*parameter), nullptr);
        saved.appendChild(entry, nullptr);
    }
}

void sanitizeLiveParameters(juce::AudioProcessorValueTreeState& state)
{
    for (auto* parameter : state.processor.getParameters())
    {
        const float normalised = parameter->getValue();
        if (!std::isfinite(normalised) || normalised < 0.0f || normalised > 1.0f)
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }
}

StateLoadResult loadState(juce::AudioProcessorValueTreeState& state, juce::ValueTree saved,
                          const juce::Identifier& expectedType)
{
    if (!saved.isValid() || saved.getType() != expectedType)
        return StateLoadResult::rejected;

    // A newer version is loaded as far as this version understands it; an older one is brought
    // up to date first.
    const int version = readStateVersion(saved);
    const bool fromNewer = version > kStateVersion;
    if (!fromNewer && !applyMigrations(saved, version, kStateVersion, stateMigrations()))
        return StateLoadResult::rejected; // a missing migration step: do not guess

    sanitizeSavedParameters(saved, state);
    addMissingParameters(saved, state);
    state.replaceState(saved);
    state.state.setProperty(kStateVersionProperty, kStateVersion, nullptr);
    sanitizeLiveParameters(state);

    return fromNewer ? StateLoadResult::loadedFromNewerVersion : StateLoadResult::loaded;
}

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PresetFile.h"

#include <algorithm>
#include <array>
#include <string>

namespace fcm::core
{

namespace
{
const juce::String kRootTag{"FrenchcorePreset"};
const juce::String kFormatAttribute{"presetFormat"};
const juce::String kNameAttribute{"name"};

bool isAllowedCharacter(juce::juce_wchar c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
           c == '-' || c == '_' || c == '.' || c == '(' || c == ')';
}

// Device names that Windows refuses as file names, with or without an extension.
bool isReservedDeviceName(const juce::String& name)
{
    const auto base = name.upToFirstOccurrenceOf(".", false, false).trim().toUpperCase();
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL")
        return true;
    return base.length() == 4 && (base.startsWith("COM") || base.startsWith("LPT")) && base[3] >= '1' &&
           base[3] <= '9';
}

// True if the elements nest deeper than `maxDepth`. JUCE's XML parser recurses once per level, so a
// file made of thousands of nested tags could overflow the stack; this scan runs first. It counts
// quoted attribute values and comments as opaque so they cannot hide a tag.
bool nestsTooDeeply(const std::string& s, int maxDepth)
{
    const auto n = s.size();
    size_t i = 0;
    int depth = 0;
    while ((i = s.find('<', i)) != std::string::npos)
    {
        ++i;
        if (i >= n)
            break;

        const auto skipTo = [&](const char* end)
        {
            const auto found = s.find(end, i);
            i = found == std::string::npos ? n : found + std::string(end).size();
        };
        if (s.compare(i, 3, "!--") == 0)
        {
            skipTo("-->");
            continue;
        }
        if (s.compare(i, 8, "![CDATA[") == 0)
        {
            skipTo("]]>");
            continue;
        }
        if (s[i] == '?' || s[i] == '!')
        {
            skipTo(">");
            continue;
        }

        const bool closing = s[i] == '/';
        bool selfClosing = false;
        char quote = 0;
        for (; i < n; ++i)
        {
            const char c = s[i];
            if (quote != 0)
            {
                if (c == quote)
                    quote = 0;
            }
            else if (c == '"' || c == '\'')
            {
                quote = c;
            }
            else if (c == '>')
            {
                selfClosing = s[i - 1] == '/';
                ++i;
                break;
            }
        }

        if (closing)
            --depth;
        else if (!selfClosing && ++depth > maxDepth)
            return true;
    }
    return false;
}

PresetParseResult refused(const juce::String& message)
{
    PresetParseResult result;
    result.error = message;
    return result;
}
} // namespace

juce::String sanitizePresetName(const juce::String& name)
{
    juce::String kept;
    bool previousWasSpace = false;
    for (auto c : name)
    {
        if (!isAllowedCharacter(c))
            continue;
        if (c == ' ')
        {
            if (previousWasSpace)
                continue;
            previousWasSpace = true;
        }
        else
        {
            previousWasSpace = false;
        }
        kept += juce::String::charToString(c);
        if (kept.length() >= kMaxPresetNameLength)
            break;
    }

    // A name may not start or end with a space or a dot (Windows drops them silently).
    kept = kept.trimCharactersAtStart(" .").trimCharactersAtEnd(" .");
    if (kept.isEmpty())
        return {};
    if (isReservedDeviceName(kept))
        kept += "_";
    return kept;
}

juce::String encodePreset(const juce::String& name, const juce::ValueTree& state)
{
    juce::XmlElement root(kRootTag);
    root.setAttribute(kFormatAttribute, kPresetFormatVersion);
    root.setAttribute(kNameAttribute, sanitizePresetName(name));
    if (auto child = state.createXml())
        root.addChildElement(child.release());
    return root.toString();
}

PresetParseResult parsePreset(const juce::String& text)
{
    const std::string utf8 = text.toStdString();
    if (static_cast<juce::int64>(utf8.size()) > kMaxPresetBytes)
        return refused("That preset file is too big.");
    if (nestsTooDeeply(utf8, kMaxPresetXmlDepth))
        return refused("That file is not a Frenchcore Machine preset.");

    const auto root = juce::XmlDocument::parse(text);
    if (root == nullptr || !root->hasTagName(kRootTag))
        return refused("That file is not a Frenchcore Machine preset.");

    const int format = root->getIntAttribute(kFormatAttribute, 0);
    if (format < 1)
        return refused("That file is not a Frenchcore Machine preset.");
    if (format > kPresetFormatVersion)
        return refused("That preset was made by a newer version of Frenchcore Machine.");

    const auto* stateElement = root->getFirstChildElement();
    if (stateElement == nullptr)
        return refused("That preset is empty.");

    PresetParseResult result;
    result.state = juce::ValueTree::fromXml(*stateElement);
    if (!result.state.isValid())
        return refused("That preset is damaged.");

    result.name = sanitizePresetName(root->getStringAttribute(kNameAttribute));
    if (result.name.isEmpty())
        result.name = "Untitled";
    result.ok = true;
    return result;
}

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#include "core/UntrustedXml.h"

#include <cctype>
#include <cstring>
#include <limits>

namespace fcm::core
{

namespace
{
// juce::AudioProcessor::copyXmlToBinary writes this number, then the text length, then the text.
constexpr juce::uint32 kJuceXmlMagic = 0x21324356;
constexpr size_t kHeaderBytes = 8;
} // namespace

bool nestsTooDeeply(std::string_view s, int maxDepth)
{
    const auto n = s.size();
    size_t i = 0;
    int depth = 0;
    while ((i = s.find('<', i)) != std::string_view::npos)
    {
        ++i;
        if (i >= n)
            break;

        const auto skipTo = [&](std::string_view end)
        {
            const auto found = s.find(end, i);
            i = found == std::string_view::npos ? n : found + end.size();
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

bool declaresDoctypeOrEntity(std::string_view text)
{
    // Markup declarations are case-sensitive in XML, but JUCE compares DOCTYPE without regard to
    // case, so both spellings are refused.
    for (size_t i = text.find("<!"); i != std::string_view::npos; i = text.find("<!", i + 2))
    {
        const auto rest = text.substr(i + 2);
        const auto startsWithNoCase = [&rest](std::string_view word)
        {
            if (rest.size() < word.size())
                return false;
            for (size_t k = 0; k < word.size(); ++k)
                if (std::toupper(static_cast<unsigned char>(rest[k])) != word[k])
                    return false;
            return true;
        };
        if (startsWithNoCase("DOCTYPE") || startsWithNoCase("ENTITY"))
            return true;
    }
    return false;
}

bool hasSafeXmlShape(std::string_view text)
{
    return !declaresDoctypeOrEntity(text) && !nestsTooDeeply(text, kMaxUntrustedXmlDepth);
}

std::unique_ptr<juce::XmlElement> parseHostStateBlob(const void* data, size_t sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= kHeaderBytes ||
        sizeInBytes > static_cast<size_t>(std::numeric_limits<int>::max()))
        return nullptr;

    const auto* bytes = static_cast<const char*>(data);
    if (juce::ByteOrder::littleEndianInt(bytes) != kJuceXmlMagic)
        return nullptr;

    const auto declaredLength = static_cast<size_t>(juce::ByteOrder::littleEndianInt(bytes + 4));
    if (declaredLength == 0 || declaredLength > sizeInBytes - kHeaderBytes)
        return nullptr;

    // The text ends at the declared length or at the first terminating zero, whichever comes
    // first; only that span is checked and turned into a string.
    const char* text = bytes + kHeaderBytes;
    const auto* zero = static_cast<const char*>(std::memchr(text, 0, declaredLength));
    const auto length = zero != nullptr ? static_cast<size_t>(zero - text) : declaredLength;
    if (length == 0)
        return nullptr;

    if (!juce::CharPointer_UTF8::isValidString(text, static_cast<int>(length)))
        return nullptr;

    const std::string_view view(text, length);
    if (!hasSafeXmlShape(view))
        return nullptr;

    return juce::parseXML(juce::String::fromUTF8(text, static_cast<int>(length)));
}

} // namespace fcm::core

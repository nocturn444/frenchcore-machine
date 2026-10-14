// SPDX-License-Identifier: AGPL-3.0-only
#include "core/StoredAudio.h"
#include "core/UntrustedXml.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>
#include <string>
#include <vector>

using fcm::core::decodeStoredFloats;
using fcm::core::parseHostStateBlob;

namespace
{
juce::MemoryBlock blobFor(const juce::String& xmlText)
{
    const auto xml = juce::parseXML(xmlText);
    REQUIRE(xml != nullptr);
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary(*xml, blob);
    return blob;
}

// A blob with JUCE's header around raw bytes, so the text can be anything at all.
juce::MemoryBlock rawBlob(const std::string& text, juce::uint32 declaredLength)
{
    juce::MemoryOutputStream out;
    out.writeInt(0x21324356);
    out.writeInt(static_cast<int>(declaredLength));
    out.write(text.data(), text.size());
    out.writeByte(0);
    return out.getMemoryBlock();
}

juce::MemoryBlock rawBlob(const std::string& text)
{
    return rawBlob(text, static_cast<juce::uint32>(text.size()));
}

std::unique_ptr<juce::XmlElement> parse(const juce::MemoryBlock& blob)
{
    return parseHostStateBlob(blob.getData(), blob.getSize());
}

juce::String encode(const std::vector<float>& values)
{
    return juce::MemoryBlock(values.data(), values.size() * sizeof(float)).toBase64Encoding();
}
} // namespace

TEST_CASE("A host state blob written by JUCE parses back", "[core][state][untrusted]")
{
    const auto blob = blobFor("<FrenchcoreMachineState stateVersion=\"1\"><Child a=\"\xc3\xa9\"/>"
                              "</FrenchcoreMachineState>");
    const auto xml = parse(blob);
    REQUIRE(xml != nullptr);
    REQUIRE(xml->hasTagName("FrenchcoreMachineState"));
    REQUIRE(xml->getChildByName("Child") != nullptr);

    // Extra bytes after the text, as some hosts keep, are ignored.
    juce::MemoryBlock padded(blob);
    padded.append("\0\0\0\0", 4);
    REQUIRE(parse(padded) != nullptr);
}

TEST_CASE("A malformed host state blob is refused before any string is built",
          "[core][state][untrusted][security]")
{
    const std::string good = "<FrenchcoreMachineState/>";
    REQUIRE(parse(rawBlob(good)) != nullptr);

    REQUIRE(parseHostStateBlob(nullptr, 100) == nullptr);
    REQUIRE(parse(juce::MemoryBlock()) == nullptr);
    REQUIRE(parse(juce::MemoryBlock("\x56\x43\x32\x21", 4)) == nullptr); // header only

    // Wrong magic number.
    auto wrongMagic = rawBlob(good);
    static_cast<char*>(wrongMagic.getData())[0] = 'x';
    REQUIRE(parse(wrongMagic) == nullptr);

    // A declared length of zero, or longer than the bytes that follow.
    REQUIRE(parse(rawBlob(good, 0)) == nullptr);
    REQUIRE(parse(rawBlob(good, static_cast<juce::uint32>(good.size() + 2))) == nullptr);
    REQUIRE(parse(rawBlob(good, 0xffffffffu)) == nullptr);

    // Invalid UTF-8: overlong four-byte forms, lone continuation bytes, a truncated sequence,
    // a surrogate.
    std::string overlong = "<FrenchcoreMachineState a=\"";
    for (int i = 0; i < 64; ++i)
        overlong += "\xf0\x80\x80\x80";
    overlong += "\"/>";
    REQUIRE(parse(rawBlob(overlong)) == nullptr);
    REQUIRE(parse(rawBlob("<FrenchcoreMachineState a=\"\x80\x80\x80\"/>")) == nullptr);
    REQUIRE(parse(rawBlob("<FrenchcoreMachineState a=\"\xe2\x82\"/>")) == nullptr);
    REQUIRE(parse(rawBlob("<FrenchcoreMachineState a=\"\xed\xa0\x80\"/>")) == nullptr);
    REQUIRE(parse(rawBlob(std::string("<FrenchcoreMachineState/>\xc3"))) == nullptr);
}

TEST_CASE("A host state with a DTD or deep nesting is refused", "[core][state][untrusted][security]")
{
    // DTDs and entities, whatever their case.
    REQUIRE(parse(rawBlob("<!DOCTYPE x [<!ENTITY a \"aaaa\">]><FrenchcoreMachineState b=\"&a;\"/>")) ==
            nullptr);
    REQUIRE(parse(rawBlob("<!doctype x><FrenchcoreMachineState/>")) == nullptr);
    REQUIRE(parse(rawBlob("<FrenchcoreMachineState><!ENTITY a \"x\"></FrenchcoreMachineState>")) == nullptr);

    // Nesting far deeper than any real state, and just past the limit.
    std::string deep = "<FrenchcoreMachineState>";
    for (int i = 0; i < 200000; ++i)
        deep += "<a>";
    REQUIRE(parse(rawBlob(deep)) == nullptr);

    const auto nested = [](int levels)
    {
        std::string text;
        for (int i = 0; i < levels; ++i)
            text += "<a>";
        for (int i = 0; i < levels; ++i)
            text += "</a>";
        return text;
    };
    REQUIRE(parse(rawBlob(nested(fcm::core::kMaxUntrustedXmlDepth))) != nullptr);
    REQUIRE(parse(rawBlob(nested(fcm::core::kMaxUntrustedXmlDepth + 1))) == nullptr);

    // Quotes, comments and CDATA cannot hide the nesting, and do not count as levels.
    std::string sneaky = "<FrenchcoreMachineState>";
    for (int i = 0; i < 200; ++i)
        sneaky += "<a x=\"</</\"><!-- </a> --><![CDATA[</a>]]>";
    REQUIRE(fcm::core::nestsTooDeeply(sneaky, fcm::core::kMaxUntrustedXmlDepth));
    REQUIRE_FALSE(fcm::core::nestsTooDeeply("<a><!-- <b><c><d> --><e/></a>", 1));
    REQUIRE_FALSE(fcm::core::declaresDoctypeOrEntity("<a text=\"DOCTYPE ENTITY\"/>"));
}

TEST_CASE("Stored float audio decodes only when its size prefix is honest", "[core][untrusted][security]")
{
    const std::vector<float> good{0.1f, -0.2f, 0.3f, 0.4f, 0.5f};
    const auto decoded = decodeStoredFloats(encode(good), 16);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == good);

    // Exactly at the limit is allowed; one more float is not.
    REQUIRE(decodeStoredFloats(encode(good), good.size()).has_value());
    REQUIRE_FALSE(decodeStoredFloats(encode(good), good.size() - 1).has_value());

    const auto payload = encode(good).fromFirstOccurrenceOf(".", false, false);
    const std::vector<juce::String> bad{
        "",
        "no dot at all",
        ".AAAA",                               // no count
        "-1.A",                                // negative: JUCE would resize to SIZE_MAX
        "0.",                                  // empty
        "2000000000.A",                        // gigabytes
        "99999999999999999999.A",              // 20 digits
        "1e3." + payload,                      // not plain digits
        " 20." + payload,                      // leading space
        "+20." + payload,                      // sign
        "21." + payload,                       // not whole floats
        "16." + payload,                       // says less than the text holds
        "24." + payload,                       // says more than the text holds
        "20." + payload + "A",                 // text longer than the count
        "20." + payload.dropLastCharacters(1), // text shorter than the count
    };
    for (const auto& text : bad)
    {
        INFO(text.substring(0, 40));
        REQUIRE_FALSE(decodeStoredFloats(text, 1'000'000).has_value());
    }
}

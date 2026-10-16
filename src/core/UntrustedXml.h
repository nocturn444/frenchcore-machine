// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <string_view>

namespace fcm::core
{

// Shape limits for XML that comes from outside the plugin: the host's state blob and preset files
// (docs/ARCHITECTURE.md §12, docs/specs/state-safe-loading.md). Our own state nests a handful of
// levels; the limit is generous so a future format still fits.
inline constexpr int kMaxUntrustedXmlDepth = 64;

// True if the elements nest deeper than `maxDepth`. JUCE's XML parser and ValueTree::fromXml
// recurse once per level, so text made of thousands of nested tags could overflow the stack; this
// scan runs before either. Quoted attribute values, comments and CDATA are opaque so they cannot
// hide a tag.
bool nestsTooDeeply(std::string_view text, int maxDepth);

// True if the text declares a DTD or an entity. JUCE's parser expands internal entities, including
// entities inside entities; the plugin never writes either, so any input with one is refused.
bool declaresDoctypeOrEntity(std::string_view text);

// True if the text passes both checks above with kMaxUntrustedXmlDepth.
bool hasSafeXmlShape(std::string_view text);

// The XML element stored in a host state blob written by juce::AudioProcessor::copyXmlToBinary,
// or nullptr when the blob is not one: wrong magic number, a declared length that does not fit,
// invalid UTF-8, a DTD or entity, or nesting deeper than kMaxUntrustedXmlDepth. The UTF-8 check
// runs before any juce::String is built, as defence in depth against JUCE's malformed-string
// overrun fixed in 9.0.4.
std::unique_ptr<juce::XmlElement> parseHostStateBlob(const void* data, size_t sizeInBytes);

} // namespace fcm::core

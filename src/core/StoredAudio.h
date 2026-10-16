// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace fcm::core
{

// Decodes float32 audio saved with juce::MemoryBlock::toBase64Encoding ("<byte count>.<text>") in
// a project or preset. JUCE's decoder trusts the byte count and resizes to it before reading the
// text, so a damaged count could throw std::bad_alloc or allocate gigabytes. Here the count must
// be plain digits, between 1 and `maxSamples` floats, a whole number of floats, and exactly what
// the encoded text holds; otherwise nothing is decoded. The values are not checked (callers
// validate them as for a loaded file).
std::optional<std::vector<float>> decodeStoredFloats(const juce::String& encoded, size_t maxSamples);

} // namespace fcm::core

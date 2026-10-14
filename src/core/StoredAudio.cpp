// SPDX-License-Identifier: AGPL-3.0-only
#include "core/StoredAudio.h"

#include <cstring>

namespace fcm::core
{

namespace
{
// More digits than this cannot describe an allowed size and could overflow while parsing.
constexpr int kMaxCountDigits = 12;

// toBase64Encoding writes one character per six bits.
constexpr size_t encodedLengthFor(size_t byteCount)
{
    return (byteCount * 8 + 5) / 6;
}
} // namespace

std::optional<std::vector<float>> decodeStoredFloats(const juce::String& encoded, size_t maxSamples)
{
    const int dot = encoded.indexOfChar('.');
    if (dot < 1 || dot > kMaxCountDigits)
        return std::nullopt;

    size_t byteCount = 0;
    for (int i = 0; i < dot; ++i)
    {
        const auto c = encoded[i];
        if (c < '0' || c > '9')
            return std::nullopt;
        byteCount = byteCount * 10 + static_cast<size_t>(c - '0');
    }

    if (byteCount == 0 || byteCount % sizeof(float) != 0 || byteCount / sizeof(float) > maxSamples)
        return std::nullopt;

    // The count is now small; the text must be exactly what the encoder writes for it.
    if (static_cast<size_t>(encoded.length() - dot - 1) != encodedLengthFor(byteCount))
        return std::nullopt;

    juce::MemoryBlock bytes;
    if (!bytes.fromBase64Encoding(encoded) || bytes.getSize() != byteCount)
        return std::nullopt;

    std::vector<float> values(byteCount / sizeof(float));
    std::memcpy(values.data(), bytes.getData(), byteCount);
    return values;
}

} // namespace fcm::core

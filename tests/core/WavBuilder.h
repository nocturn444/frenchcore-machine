// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fcm::test
{

// A minimal WAV file written by hand, so the tests control every byte (including lying headers).
struct WavSpec
{
    int channels = 1;
    uint32_t sampleRate = 48000;
    bool isFloat = true;
    uint32_t declaredDataBytes = 0xffffffff; // 0xffffffff: the real size
    bool truncateBy = false;
};

inline void put32(std::vector<uint8_t>& out, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

inline void put16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

inline void putTag(std::vector<uint8_t>& out, const char* tag)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(tag[i]));
}

// `samples` are interleaved if channels > 1.
inline std::vector<uint8_t> makeWav(const std::vector<float>& samples, const WavSpec& spec = {})
{
    const int bytesPerSample = spec.isFloat ? 4 : 2;
    const auto realBytes = static_cast<uint32_t>(samples.size() * static_cast<size_t>(bytesPerSample));
    const uint32_t dataBytes = spec.declaredDataBytes == 0xffffffff ? realBytes : spec.declaredDataBytes;

    std::vector<uint8_t> out;
    putTag(out, "RIFF");
    put32(out, 36 + dataBytes);
    putTag(out, "WAVE");
    putTag(out, "fmt ");
    put32(out, 16);
    put16(out, spec.isFloat ? 3 : 1);
    put16(out, static_cast<uint16_t>(spec.channels));
    put32(out, spec.sampleRate);
    put32(out, spec.sampleRate * static_cast<uint32_t>(spec.channels * bytesPerSample));
    put16(out, static_cast<uint16_t>(spec.channels * bytesPerSample));
    put16(out, static_cast<uint16_t>(bytesPerSample * 8));
    putTag(out, "data");
    put32(out, dataBytes);

    for (float s : samples)
    {
        if (spec.isFloat)
        {
            uint32_t bits;
            std::memcpy(&bits, &s, 4);
            put32(out, bits);
        }
        else
        {
            put16(out, static_cast<uint16_t>(
                           static_cast<int16_t>(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767.0f))));
        }
    }
    if (spec.truncateBy)
        out.resize(out.size() / 2);
    return out;
}

} // namespace fcm::test

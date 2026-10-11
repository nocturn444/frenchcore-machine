// SPDX-License-Identifier: AGPL-3.0-only
#include "core/WavetableImport.h"

#include "dsp/oscillator/WavetableData.h"

#include <algorithm>
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace fcm::core
{

namespace
{
constexpr int kFrame = dsp::WavetableData::kFrameSize;

WavetableImportResult fail(const juce::String& message)
{
    WavetableImportResult result;
    result.error = message;
    return result;
}

// One periodic cycle stretched to kFrame samples with cubic (Catmull-Rom) interpolation, the cycle
// wrapping around at both ends.
std::vector<float> resampleCycle(const std::vector<float>& cycle)
{
    const auto n = static_cast<int>(cycle.size());
    const auto at = [&](int i) { return cycle[static_cast<size_t>(((i % n) + n) % n)]; };

    std::vector<float> out(static_cast<size_t>(kFrame));
    for (int i = 0; i < kFrame; ++i)
    {
        const double position = static_cast<double>(i) * n / kFrame;
        const int base = static_cast<int>(std::floor(position));
        const double t = position - base;
        const double p0 = at(base - 1), p1 = at(base), p2 = at(base + 1), p3 = at(base + 2);
        out[static_cast<size_t>(i)] = static_cast<float>(
            p1 + 0.5 * t *
                     (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0))));
    }
    return out;
}
} // namespace

WavetableImportResult importWavetable(const void* data, size_t numBytes)
{
    if (data == nullptr || numBytes == 0)
        return fail("The file is empty");
    if (numBytes > kMaxWavetableFileBytes)
        return fail("The file is too big (at most 32 MB)");

    juce::WavAudioFormat wav;
    // The reader owns the stream once it opens it, and deletes it if it does not.
    std::unique_ptr<juce::AudioFormatReader> reader(
        wav.createReaderFor(new juce::MemoryInputStream(data, numBytes, false), true));
    if (reader == nullptr)
        return fail("Not a WAV file");

    const auto length = reader->lengthInSamples;
    const auto channels = static_cast<int>(reader->numChannels);
    if (channels < 1 || channels > kMaxWavetableChannels)
        return fail("Unsupported number of channels");
    if (length < kMinCycleSamples)
        return fail("Too short: at least 64 samples");
    if (length > kMaxWavetableSamples)
        return fail("Too long: at most 256 frames of 2048 samples");

    const auto samples = static_cast<int>(length);
    const bool wholeFrames = samples % kFrame == 0;
    if (!wholeFrames && samples > kMaxCycleSamples)
        return fail("The length must be a whole number of 2048-sample frames, or a single cycle of up to "
                    "16384 samples");

    // Only the first channel is read: the other channels are not allocated.
    juce::AudioBuffer<float> buffer(1, samples);
    if (!reader->read(&buffer, 0, samples, 0, true, false))
        return fail("The file could not be read");

    std::vector<float> source(buffer.getReadPointer(0), buffer.getReadPointer(0) + samples);
    for (float s : source)
        if (!std::isfinite(s))
            return fail("The file contains invalid samples");

    std::vector<float> frames = wholeFrames ? std::move(source) : resampleCycle(source);

    // Centre every frame and scale the whole table so its loudest sample is 1.
    float peak = 0.0f;
    for (size_t frame = 0; frame < frames.size() / kFrame; ++frame)
    {
        float* f = frames.data() + frame * kFrame;
        double mean = 0.0;
        for (int i = 0; i < kFrame; ++i)
            mean += f[i];
        mean /= kFrame;
        for (int i = 0; i < kFrame; ++i)
        {
            f[i] = static_cast<float>(f[i] - mean);
            peak = std::max(peak, std::abs(f[i]));
        }
    }
    if (!(peak > 1.0e-6f))
        return fail("The file is silent");

    for (float& s : frames)
        s /= peak;

    WavetableImportResult result;
    result.frames = std::move(frames);
    return result;
}

bool isValidStoredWavetable(const std::vector<float>& frames) noexcept
{
    if (frames.empty() || frames.size() % kFrame != 0 ||
        frames.size() > static_cast<size_t>(kMaxWavetableSamples))
        return false;
    return std::all_of(frames.begin(), frames.end(),
                       [](float s) { return std::isfinite(s) && std::abs(s) <= 4.0f; });
}

juce::String sanitiseWavetableName(const juce::String& name)
{
    juce::String clean;
    for (auto c : name.trim())
        if (c >= 32 && c != 127 && clean.length() < 64)
            clean += c;
    return clean.trim();
}

} // namespace fcm::core

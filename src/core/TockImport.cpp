// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TockImport.h"

#include "core/WavetableImport.h"

#include <algorithm>
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace fcm::core
{

namespace
{
TockImportResult fail(const juce::String& message)
{
    TockImportResult result;
    result.error = message;
    return result;
}
} // namespace

TockImportResult importTockSample(const void* data, size_t numBytes)
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

    const auto channels = static_cast<int>(reader->numChannels);
    if (channels < 1 || channels > kMaxWavetableChannels)
        return fail("Unsupported number of channels");

    const double rate = reader->sampleRate;
    if (!std::isfinite(rate) || rate < kMinTockSampleRate || rate > kMaxTockSampleRate)
        return fail("Unsupported sample rate (8 kHz to 192 kHz)");

    const auto length = reader->lengthInSamples;
    if (length < kMinTockSamples)
        return fail("Too short: at least 16 samples");
    if (length > kMaxTockSamples || static_cast<double>(length) > kMaxTockSeconds * rate)
        return fail("Too long: at most 2 seconds");

    const auto samples = static_cast<int>(length);
    juce::AudioBuffer<float> buffer(1, samples);
    if (!reader->read(&buffer, 0, samples, 0, true, false))
        return fail("The file could not be read");

    TockImportResult result;
    result.sample.sampleRate = rate;
    result.sample.samples.assign(buffer.getReadPointer(0), buffer.getReadPointer(0) + samples);
    auto& data_ = result.sample.samples;

    for (float s : data_)
        if (!std::isfinite(s))
            return fail("The file contains invalid samples");

    // Centre, scale to peak 1 and fade the last 2 ms out so the sample never ends with a click.
    double mean = 0.0;
    for (float s : data_)
        mean += s;
    mean /= static_cast<double>(data_.size());

    float peak = 0.0f;
    for (float& s : data_)
    {
        s = static_cast<float>(s - mean);
        peak = std::max(peak, std::abs(s));
    }
    if (!(peak > 1.0e-6f))
        return fail("The file is silent");

    for (float& s : data_)
        s /= peak;

    const auto fade = std::min(data_.size(), static_cast<size_t>(0.002 * rate));
    for (size_t i = 0; i < fade; ++i)
        data_[data_.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);

    return result;
}

bool isValidStoredTockSample(const dsp::TockSample& sample) noexcept
{
    if (!std::isfinite(sample.sampleRate) || sample.sampleRate < kMinTockSampleRate ||
        sample.sampleRate > kMaxTockSampleRate)
        return false;
    if (sample.samples.size() < static_cast<size_t>(kMinTockSamples) ||
        sample.samples.size() > static_cast<size_t>(kMaxTockSamples))
        return false;
    return std::all_of(sample.samples.begin(), sample.samples.end(),
                       [](float s) { return std::isfinite(s) && std::abs(s) <= 4.0f; });
}

} // namespace fcm::core

// SPDX-License-Identifier: AGPL-3.0-only
#include "core/NoiseImport.h"

#include "core/WavetableImport.h"

#include <algorithm>
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <numbers>

namespace fcm::core
{

namespace
{
NoiseImportResult fail(const juce::String& message)
{
    NoiseImportResult result;
    result.error = message;
    return result;
}

// Brings `input` from `rate` down to kStoredNoiseRate with a Hann-windowed sinc low-pass (12 zero
// crossings each side at the output rate), so a long file at 96 or 192 kHz is stored at most 480000
// samples long without folding its highs back into the audible band.
std::vector<float> resampleDown(const std::vector<float>& input, double rate)
{
    const double ratio = rate / kStoredNoiseRate; // > 1
    const auto outLength = static_cast<size_t>(static_cast<double>(input.size()) / ratio);
    constexpr double kCutoff = 0.45;    // of the output rate
    constexpr double kHalfWidth = 12.0; // zero crossings of the output rate

    std::vector<float> out(outLength);
    for (size_t n = 0; n < outLength; ++n)
    {
        const double centre = static_cast<double>(n) * ratio;
        const auto first = static_cast<long long>(std::ceil(centre - kHalfWidth * ratio));
        const auto last = static_cast<long long>(std::floor(centre + kHalfWidth * ratio));
        double sum = 0.0, weight = 0.0;
        for (long long i = first; i <= last; ++i)
        {
            if (i < 0 || i >= static_cast<long long>(input.size()))
                continue;
            const double x = (static_cast<double>(i) - centre) / ratio; // in output samples
            const double window = 0.5 + 0.5 * std::cos(std::numbers::pi * x / kHalfWidth);
            const double arg = 2.0 * kCutoff * x;
            const double sinc =
                std::abs(arg) < 1.0e-12 ? 1.0 : std::sin(std::numbers::pi * arg) / (std::numbers::pi * arg);
            const double k = 2.0 * kCutoff * sinc * window;
            sum += k * input[static_cast<size_t>(i)];
            weight += k;
        }
        out[n] = static_cast<float>(weight > 1.0e-9 ? sum / weight : 0.0);
    }
    return out;
}
} // namespace

NoiseImportResult importNoiseSample(const void* data, size_t numBytes)
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
    if (!std::isfinite(rate) || rate < kMinNoiseSampleRate || rate > kMaxNoiseSampleRate)
        return fail("Unsupported sample rate (8 kHz to 192 kHz)");

    const auto length = reader->lengthInSamples;
    if (length < kMinNoiseSamples)
        return fail("Too short: at least 16 samples");
    if (static_cast<double>(length) > kMaxNoiseSeconds * rate)
        return fail("Too long: at most 10 seconds");

    const auto samples = static_cast<int>(length);
    juce::AudioBuffer<float> buffer(1, samples);
    if (!reader->read(&buffer, 0, samples, 0, true, false))
        return fail("The file could not be read");

    std::vector<float> audio(buffer.getReadPointer(0), buffer.getReadPointer(0) + samples);
    for (float s : audio)
        if (!std::isfinite(s))
            return fail("The file contains invalid samples");

    NoiseImportResult result;
    result.sample.sampleRate = rate;
    if (rate > kStoredNoiseRate)
    {
        audio = resampleDown(audio, rate);
        result.sample.sampleRate = kStoredNoiseRate;
        if (audio.size() < static_cast<size_t>(kMinNoiseSamples))
            return fail("Too short: at least 16 samples");
    }

    // Centre it and scale it to peak 1, like the factory noises.
    double mean = 0.0;
    for (float s : audio)
        mean += s;
    mean /= static_cast<double>(audio.size());

    float peak = 0.0f;
    for (float& s : audio)
    {
        s = static_cast<float>(s - mean);
        peak = std::max(peak, std::abs(s));
    }
    if (!(peak > 1.0e-6f))
        return fail("The file is silent");
    for (float& s : audio)
        s /= peak;

    result.sample.samples = std::move(audio);
    return result;
}

bool isValidStoredNoiseSample(const dsp::NoiseSample& sample) noexcept
{
    if (!std::isfinite(sample.sampleRate) || sample.sampleRate < kMinNoiseSampleRate ||
        sample.sampleRate > kMaxNoiseSampleRate)
        return false;
    if (sample.samples.size() < static_cast<size_t>(kMinNoiseSamples) ||
        sample.samples.size() > static_cast<size_t>(kMaxNoiseSamples))
        return false;
    return std::all_of(sample.samples.begin(), sample.samples.end(),
                       [](float s) { return std::isfinite(s) && std::abs(s) <= 4.0f; });
}

} // namespace fcm::core

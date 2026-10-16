// SPDX-License-Identifier: AGPL-3.0-only
#include "core/NoiseSelector.h"

#include "core/NoiseImport.h"
#include "core/ParameterIds.h"
#include "core/StoredAudio.h"
#include "core/WavetableImport.h"
#include "dsp/noise/FactoryNoises.h"

#include <algorithm>
#include <bit>
#include <cmath>

static_assert(std::endian::native == std::endian::little, "saved samples are little-endian floats");

namespace fcm::core
{

NoiseSelector::NoiseSelector(juce::AudioProcessorValueTreeState& stateIn) : state(stateIn)
{
    raw = state.getRawParameterValue(ParameterIds::kNoiseType);
    jassert(raw != nullptr); // layout and selector out of sync
    update();
    state.addParameterListener(ParameterIds::kNoiseType, this);
}

NoiseSelector::~NoiseSelector()
{
    state.removeParameterListener(ParameterIds::kNoiseType, this);
    cancelPendingUpdate();
}

int NoiseSelector::wantedEntry() const
{
    // A damaged value never gets here (state loading sanitizes it), but the index is clamped anyway.
    const float value = raw != nullptr ? raw->load() : 0.0f;
    const int index =
        std::isfinite(value) ? static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0e6f))) : 0;
    return std::clamp(index, 0, userEntry());
}

int NoiseSelector::userEntry() noexcept
{
    return dsp::FactoryNoises::count();
}

void NoiseSelector::update()
{
    const int wanted = wantedEntry();
    if (wanted == entry && !dirty)
        return;

    // The User entry without a user sample uses the first built-in one.
    std::unique_ptr<const dsp::NoiseSample> sample;
    if (wanted == userEntry() && hasUser())
        sample = std::make_unique<dsp::NoiseSample>(userSample);
    else
        sample = dsp::FactoryNoises::build(wanted == userEntry() ? 0 : wanted);

    if (sample != nullptr)
    {
        published.publish(std::move(sample));
        entry = wanted;
        dirty = false;
    }
}

bool NoiseSelector::loadUser(dsp::NoiseSample sample, const juce::String& name)
{
    if (!isValidStoredNoiseSample(sample))
        return false;

    userSample = std::move(sample);
    userDisplayName = sanitiseWavetableName(name);
    dirty = true;
    update();
    return true;
}

void NoiseSelector::clearUser()
{
    if (!hasUser())
        return;

    userSample = {};
    userDisplayName.clear();
    dirty = true;
    update();
}

namespace
{
const juce::Identifier kNodeType{"UserNoiseSample"};
constexpr int kMaxBase64Characters = 2'600'000; // 480000 float32 samples, plus padding
} // namespace

juce::ValueTree NoiseSelector::toValueTree() const
{
    if (!hasUser())
        return {};

    juce::MemoryBlock bytes(userSample.samples.data(), userSample.samples.size() * sizeof(float));
    juce::ValueTree node(kNodeType);
    node.setProperty("name", userDisplayName, nullptr);
    node.setProperty("sampleRate", userSample.sampleRate, nullptr);
    node.setProperty("samples", bytes.toBase64Encoding(), nullptr);
    return node;
}

bool NoiseSelector::restoreFrom(const juce::ValueTree& node)
{
    const auto restoreNothing = [this]
    {
        clearUser();
        return false;
    };

    if (!node.isValid() || node.getType() != kNodeType)
        return restoreNothing();

    const juce::String text = node["samples"].toString();
    if (text.isEmpty() || text.length() > kMaxBase64Characters)
        return restoreNothing();

    auto samples = decodeStoredFloats(text, static_cast<size_t>(kMaxNoiseSamples));
    if (!samples.has_value())
        return restoreNothing();

    dsp::NoiseSample sample;
    sample.sampleRate = static_cast<double>(node["sampleRate"]);
    sample.samples = std::move(*samples);
    if (!loadUser(std::move(sample), node["name"].toString()))
        return restoreNothing();
    return true;
}

void NoiseSelector::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void NoiseSelector::handleAsyncUpdate()
{
    update();
}

} // namespace fcm::core

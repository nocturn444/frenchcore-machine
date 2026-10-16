// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TockSelector.h"

#include "core/ParameterIds.h"
#include "core/StoredAudio.h"
#include "core/TockImport.h"
#include "core/WavetableImport.h"
#include "dsp/tock/FactoryTocks.h"

#include <algorithm>
#include <bit>
#include <cmath>

static_assert(std::endian::native == std::endian::little, "saved samples are little-endian floats");

namespace fcm::core
{

TockSelector::TockSelector(juce::AudioProcessorValueTreeState& stateIn) : state(stateIn)
{
    raw = state.getRawParameterValue(ParameterIds::kTockSample);
    jassert(raw != nullptr); // layout and selector out of sync
    update();
    state.addParameterListener(ParameterIds::kTockSample, this);
}

TockSelector::~TockSelector()
{
    state.removeParameterListener(ParameterIds::kTockSample, this);
    cancelPendingUpdate();
}

int TockSelector::wantedEntry() const
{
    // A damaged value never gets here (state loading sanitizes it), but the index is clamped anyway.
    const float value = raw != nullptr ? raw->load() : 0.0f;
    const int index =
        std::isfinite(value) ? static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0e6f))) : 0;
    return std::clamp(index, 0, userEntry());
}

int TockSelector::userEntry() noexcept
{
    return dsp::FactoryTocks::count();
}

void TockSelector::update()
{
    const int wanted = wantedEntry();
    if (wanted == entry && !dirty)
        return;

    // The User entry without a user sample uses the first built-in one.
    std::unique_ptr<const dsp::TockSample> sample;
    if (wanted == userEntry() && hasUser())
        sample = std::make_unique<dsp::TockSample>(userSample);
    else
        sample = dsp::FactoryTocks::build(wanted == userEntry() ? 0 : wanted);

    if (sample != nullptr)
    {
        peaks = waveformPeaks(*sample, kDisplayBins);
        detected = detectTransient(*sample);
        ++generationCount;
        published.publish(std::move(sample));
        entry = wanted;
        dirty = false;
    }
}

bool TockSelector::loadUser(dsp::TockSample sample, const juce::String& name)
{
    if (!isValidStoredTockSample(sample))
        return false;

    userSample = std::move(sample);
    userDisplayName = sanitiseWavetableName(name);
    dirty = true;
    update();
    return true;
}

void TockSelector::clearUser()
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
const juce::Identifier kNodeType{"UserTockSample"};
constexpr int kMaxBase64Characters = 2'600'000; // 480000 float32 samples, plus padding
} // namespace

juce::ValueTree TockSelector::toValueTree() const
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

bool TockSelector::restoreFrom(const juce::ValueTree& node)
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

    auto samples = decodeStoredFloats(text, static_cast<size_t>(kMaxTockSamples));
    if (!samples.has_value())
        return restoreNothing();

    dsp::TockSample sample;
    sample.sampleRate = static_cast<double>(node["sampleRate"]);
    sample.samples = std::move(*samples);
    if (!loadUser(std::move(sample), node["name"].toString()))
        return restoreNothing();
    return true;
}

void TockSelector::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void TockSelector::handleAsyncUpdate()
{
    update();
}

} // namespace fcm::core

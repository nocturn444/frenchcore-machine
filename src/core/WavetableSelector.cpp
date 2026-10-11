// SPDX-License-Identifier: AGPL-3.0-only
#include "core/WavetableSelector.h"

#include "core/ParameterIds.h"
#include "core/WavetableImport.h"
#include "dsp/oscillator/FactoryWavetables.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

static_assert(std::endian::native == std::endian::little, "saved wavetables are little-endian floats");

namespace fcm::core
{

WavetableSelector::WavetableSelector(juce::AudioProcessorValueTreeState& stateIn, int oscillatorIndex)
    : state(stateIn),
      parameterId(ParameterIds::oscillator(oscillatorIndex, ParameterIds::Oscillator::kWavetable)),
      oscillator(oscillatorIndex)
{
    raw = state.getRawParameterValue(parameterId);
    jassert(raw != nullptr); // layout and selector out of sync
    update();
    state.addParameterListener(parameterId, this);
}

WavetableSelector::~WavetableSelector()
{
    state.removeParameterListener(parameterId, this);
    cancelPendingUpdate();
}

int WavetableSelector::wantedEntry() const
{
    // A damaged value never gets here (state loading sanitizes it), but the index is clamped anyway.
    const float value = raw != nullptr ? raw->load() : 0.0f;
    const int index =
        std::isfinite(value) ? static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0e6f))) : 0;
    return std::clamp(index, 0, userEntry());
}

int WavetableSelector::userEntry() noexcept
{
    return dsp::FactoryWavetables::count();
}

void WavetableSelector::update()
{
    const int wanted = wantedEntry();
    if (wanted == entry && !dirty)
        return;

    // The User entry without a user wavetable plays the Sine.
    std::unique_ptr<const dsp::WavetableData> table;
    if (wanted == userEntry())
        table = hasUser() ? dsp::WavetableData::fromFrames(userFrames) : dsp::FactoryWavetables::build(0);
    else
        table = dsp::FactoryWavetables::build(wanted);

    if (table != nullptr)
    {
        published.publish(std::move(table));
        entry = wanted;
        dirty = false;
    }
}

bool WavetableSelector::loadUser(std::vector<float> frames, const juce::String& name)
{
    if (!isValidStoredWavetable(frames))
        return false;

    userFrames = std::move(frames);
    userDisplayName = sanitiseWavetableName(name);
    dirty = true;
    update();
    return true;
}

void WavetableSelector::clearUser()
{
    if (userFrames.empty())
        return;

    userFrames.clear();
    userDisplayName.clear();
    dirty = true;
    update();
}

namespace
{
juce::Identifier nodeTypeFor(int oscillator)
{
    return juce::Identifier("UserWavetable" + juce::String(oscillator + 1));
}

constexpr int kMaxBase64Characters = 2'800'000; // 256 frames of float32, plus padding
} // namespace

juce::ValueTree WavetableSelector::toValueTree() const
{
    if (!hasUser())
        return {};

    juce::MemoryBlock bytes(userFrames.data(), userFrames.size() * sizeof(float));
    juce::ValueTree node(nodeTypeFor(oscillator));
    node.setProperty("name", userDisplayName, nullptr);
    node.setProperty("frames", bytes.toBase64Encoding(), nullptr);
    return node;
}

bool WavetableSelector::restoreFrom(const juce::ValueTree& node)
{
    const auto restoreNothing = [this]
    {
        clearUser();
        return false;
    };

    if (!node.isValid() || node.getType() != nodeTypeFor(oscillator))
        return restoreNothing();

    const juce::String text = node["frames"].toString();
    if (text.isEmpty() || text.length() > kMaxBase64Characters)
        return restoreNothing();

    juce::MemoryBlock bytes;
    if (!bytes.fromBase64Encoding(text) || bytes.getSize() % sizeof(float) != 0)
        return restoreNothing();

    std::vector<float> frames(bytes.getSize() / sizeof(float));
    std::memcpy(frames.data(), bytes.getData(), bytes.getSize());
    if (!loadUser(std::move(frames), node["name"].toString()))
        return restoreNothing();
    return true;
}

void WavetableSelector::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void WavetableSelector::handleAsyncUpdate()
{
    update();
}

} // namespace fcm::core

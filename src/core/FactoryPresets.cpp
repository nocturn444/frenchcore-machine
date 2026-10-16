// SPDX-License-Identifier: AGPL-3.0-only
#include "core/FactoryPresets.h"

#include "core/EnvelopeState.h"
#include "core/ParameterIds.h"
#include "dsp/envelope/EnvelopePresets.h"
#include "dsp/oscillator/FactoryWavetables.h"
#include "dsp/tock/FactoryTocks.h"

#include <cstring>
#include <string>
#include <vector>

namespace fcm::core
{

namespace
{
struct Override
{
    std::string id;
    double value; // in the parameter's own units, as saved
};

struct Definition
{
    std::string name;
    std::vector<Override> overrides;
    std::string ampShape;
    std::string pitchShape;
};

// The Wavetable and Tock Sample parameters save the entry number; the tables are looked up by name
// so that the preset table reads like what it means.
double wavetableNamed(const char* name)
{
    for (int i = 0; i < dsp::FactoryWavetables::count(); ++i)
        if (std::strcmp(dsp::FactoryWavetables::name(i), name) == 0)
            return i;
    jassertfalse;
    return 0;
}

double tockSampleNamed(const char* name)
{
    for (int i = 0; i < dsp::FactoryTocks::count(); ++i)
        if (std::strcmp(dsp::FactoryTocks::name(i), name) == 0)
            return i;
    jassertfalse;
    return 0;
}

const std::vector<Definition>& definitions()
{
    // Distortion types: 0 Soft Clip, 1 Hard Clip, 2 Overdrive, 3 Foldback, 4 Bitcrush.
    static const std::vector<Definition> table = {
        {"Init", {}, "Kick + Bass", "Classic Drop"},

        {"Classic Punch",
         {{"osc1.wavetable", wavetableNamed("Hard Clip")},
          {"osc1.position", 0.4},
          {"distortion.enabled", 1},
          {"distortion.type", 0},
          {"distortion.drive", 40},
          {"distortion.postgain", -6}},
         "Short Punch",
         "Short Punch"},

        {"Rumble Kick",
         {{"osc1.wavetable", wavetableNamed("Rumble Core")},
          {"osc1.position", 0.5},
          {"distortion.enabled", 1},
          {"distortion.type", 2},
          {"distortion.drive", 55},
          {"distortion.postgain", -8},
          {"eq.enabled", 1},
          {"eq.b1.gain", 4.0}},
         "Long Tail",
         "Long Tail"},

        {"Hardcore Crunch",
         {{"osc1.wavetable", wavetableNamed("Hardcore Punch")},
          {"osc1.position", 0.6},
          {"distortion.enabled", 1},
          {"distortion.type", 3},
          {"distortion.drive", 60},
          {"distortion.postgain", -9},
          {"tock.on", 1},
          {"tock.mode", 1},
          {"tock.sample", tockSampleNamed("Click")},
          {"tock.ampEnv", 1}}, // as in 0.1, where the Tock always followed the amp envelope
         "Kick + Bass",
         "Classic Drop"},

        {"Gabber Fold",
         {{"osc1.wavetable", wavetableNamed("Wave Fold")},
          {"osc1.position", 0.7},
          {"distortion.enabled", 1},
          {"distortion.type", 1},
          {"distortion.drive", 70},
          {"distortion.postgain", -9},
          {"stereo.enabled", 1},
          {"stereo.highwidth", 150}},
         "Kick + Bass",
         "Short Punch"},
    };
    return table;
}

juce::ValueTree shapeNode(dsp::EnvelopeKind kind, const std::string& shapeName)
{
    EnvelopeState envelope(kind);
    for (const auto& preset : dsp::EnvelopePresets::all(kind))
        if (preset.name == shapeName)
        {
            envelope.set(preset.envelope);
            break;
        }
    return envelope.toValueTree();
}
} // namespace

int FactoryPresets::count() noexcept
{
    return static_cast<int>(definitions().size());
}

juce::String FactoryPresets::name(int index)
{
    return index >= 0 && index < count() ? juce::String(definitions()[static_cast<size_t>(index)].name)
                                         : juce::String();
}

juce::ValueTree FactoryPresets::makeState(int index, const juce::Identifier& rootType)
{
    if (index < 0 || index >= count())
        return {};

    const auto& definition = definitions()[static_cast<size_t>(index)];
    juce::ValueTree tree(rootType);
    tree.setProperty("stateVersion", kStateVersion, nullptr);
    for (const auto& entry : definition.overrides)
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", juce::String(entry.id), nullptr);
        parameter.setProperty("value", entry.value, nullptr);
        tree.appendChild(parameter, nullptr);
    }
    tree.appendChild(shapeNode(dsp::EnvelopeKind::amp, definition.ampShape), nullptr);
    tree.appendChild(shapeNode(dsp::EnvelopeKind::pitch, definition.pitchShape), nullptr);
    return tree;
}

} // namespace fcm::core

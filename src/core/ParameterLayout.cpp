// SPDX-License-Identifier: AGPL-3.0-only
#include "core/ParameterLayout.h"

#include "core/ParameterIds.h"
#include "dsp/engine/EngineParameters.h"
#include "dsp/oscillator/FactoryWavetables.h"
#include "dsp/tock/FactoryTocks.h"

namespace fcm::core
{

namespace
{
constexpr int kParameterVersion = 1;

juce::ParameterID id(const std::string& text)
{
    return juce::ParameterID{text, kParameterVersion};
}

juce::String noteName(int note)
{
    // JUCE convention: MIDI note 60 is C3.
    return juce::MidiMessage::getMidiNoteName(note, true, true, 3);
}

void addGlobalParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kOutputGain), "Output Gain", juce::NormalisableRange<float>{-48.0f, 12.0f, 0.1f},
        0.0f, juce::AudioParameterFloatAttributes{}.withLabel("dB")));

    layout.add(
        std::make_unique<juce::AudioParameterBool>(id(ParameterIds::kLimiterOn), "Safety Limiter", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(id(ParameterIds::kKeyTrack), "Key Track", true));

    layout.add(std::make_unique<juce::AudioParameterInt>(
        id(ParameterIds::kRootNote), "Root Note", 12, 84, 36,
        juce::AudioParameterIntAttributes{}
            .withStringFromValueFunction([](int value, int) { return noteName(value); })
            .withValueFromStringFunction(
                [](const juce::String& text)
                {
                    // Accept a plain note number as well as a name such as "C1".
                    if (text.containsOnly("0123456789"))
                        return text.getIntValue();
                    for (int note = 12; note <= 84; ++note)
                        if (noteName(note).equalsIgnoreCase(text.trim()))
                            return note;
                    return 36;
                })));

    layout.add(std::make_unique<juce::AudioParameterBool>(id(ParameterIds::kGlideOn), "Glide", false));

    // Skewed so the middle of the knob is 100 ms: most useful glides are short.
    juce::NormalisableRange<float> glideRange{1.0f, 1000.0f, 1.0f};
    glideRange.setSkewForCentre(100.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kGlideTime), "Glide Time", glideRange, 100.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("ms")));

    // Oversampling for the effects that need it, per docs/specs/fx-chain.md (1x, 2x, 4x, 8x).
    layout.add(std::make_unique<juce::AudioParameterChoice>(id(ParameterIds::kQuality), "Quality",
                                                            juce::StringArray{"1x", "2x", "4x", "8x"}, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kVelocitySensitivity), "Velocity Sensitivity",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")));
}

void addEnvelopeParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    // Time scale: a multiplier, centred on 1 so the knob is symmetric in octaves.
    auto timeScaleRange = []
    {
        juce::NormalisableRange<float> range{0.25f, 4.0f, 0.01f};
        range.setSkewForCentre(1.0f);
        return range;
    };
    const auto times = juce::AudioParameterFloatAttributes{}.withLabel("x");

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kAmpEnvTimeScale), "Amp Env Time Scale", timeScaleRange(), 1.0f, times));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kPitchEnvTimeScale), "Pitch Env Time Scale", timeScaleRange(), 1.0f, times));
    layout.add(
        std::make_unique<juce::AudioParameterFloat>(id(ParameterIds::kPitchEnvAmount), "Pitch Env Amount",
                                                    juce::NormalisableRange<float>{0.0f, 96.0f, 0.1f}, 48.0f,
                                                    juce::AudioParameterFloatAttributes{}.withLabel("st")));

    juce::NormalisableRange<float> splitRange{5.0f, 500.0f, 1.0f};
    splitRange.setSkewForCentre(80.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kPunchTailSplit), "Punch Tail Split", splitRange, 80.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("ms")));

    // The volume of each part on its own, in dB like the output gain (-48 is silence).
    const juce::NormalisableRange<float> levelRange{-48.0f, 12.0f, 0.1f};
    const auto db = juce::AudioParameterFloatAttributes{}.withLabel("dB");
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(ParameterIds::kPunchLevel), "Punch Level",
                                                           levelRange, 0.0f, db));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(ParameterIds::kTailLevel), "Tail Level",
                                                           levelRange, 0.0f, db));
}

void addOscillatorParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int index)
{
    namespace Osc = ParameterIds::Oscillator;
    const juce::String name = "Osc " + juce::String(index + 1) + " ";
    const bool defaultOn = (index == 0);

    layout.add(std::make_unique<juce::AudioParameterBool>(id(ParameterIds::oscillator(index, Osc::kOn)),
                                                          name + "On", defaultOn));
    // The choice list is append-only: the saved value is the entry number (docs/specs/wavetable-factory.md).
    juce::StringArray wavetableNames;
    for (int i = 0; i < dsp::FactoryWavetables::count(); ++i)
        wavetableNames.add(dsp::FactoryWavetables::name(i));
    wavetableNames.add("User"); // the wavetable the user loaded; see WavetableSelector::userEntry()
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        id(ParameterIds::oscillator(index, Osc::kWavetable)), name + "Wavetable", wavetableNames, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(ParameterIds::oscillator(index, Osc::kLevel)),
                                                           name + "Level",
                                                           juce::NormalisableRange<float>{0.0f, 1.0f}, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        id(ParameterIds::oscillator(index, Osc::kPitch)), name + "Pitch", -24, 24, 0,
        juce::AudioParameterIntAttributes{}.withLabel("st")));
    layout.add(std::make_unique<juce::AudioParameterInt>(id(ParameterIds::oscillator(index, Osc::kOctave)),
                                                         name + "Octave", -4, 4, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::oscillator(index, Osc::kFine)), name + "Fine",
        juce::NormalisableRange<float>{-100.0f, 100.0f, 0.1f}, 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("ct")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::oscillator(index, Osc::kPosition)), name + "Position",
        juce::NormalisableRange<float>{0.0f, 1.0f}, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::oscillator(index, Osc::kPhase)), name + "Phase",
        juce::NormalisableRange<float>{0.0f, 360.0f, 1.0f}, 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("deg")));
}
void addTockParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>(id(ParameterIds::kTockOn), "Tock On", false));
    // Both lists are append-only: the saved value is the entry number (docs/specs/tock.md).
    layout.add(std::make_unique<juce::AudioParameterChoice>(id(ParameterIds::kTockMode), "Tock Mode",
                                                            juce::StringArray{"Synth", "Sample"}, 0));
    juce::StringArray sampleNames;
    for (int i = 0; i < dsp::FactoryTocks::count(); ++i)
        sampleNames.add(dsp::FactoryTocks::name(i));
    sampleNames.add("User"); // the sample the user loaded; see TockSelector::userEntry()
    layout.add(std::make_unique<juce::AudioParameterChoice>(id(ParameterIds::kTockSample), "Tock Sample",
                                                            sampleNames, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kTockLevel), "Tock Level", juce::NormalisableRange<float>{-48.0f, 12.0f, 0.1f},
        -12.0f, juce::AudioParameterFloatAttributes{}.withLabel("dB")));
    layout.add(
        std::make_unique<juce::AudioParameterInt>(id(ParameterIds::kTockTune), "Tock Tune", -24, 24, 0,
                                                  juce::AudioParameterIntAttributes{}.withLabel("st")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kTockStart), "Tock Start", juce::NormalisableRange<float>{0.0f, 20.0f, 0.1f}, 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("ms")));
    juce::NormalisableRange<float> decayRange{2.0f, 200.0f, 0.1f};
    decayRange.setSkewForCentre(25.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        id(ParameterIds::kTockDecay), "Tock Decay", decayRange, 25.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(ParameterIds::kTockTone), "Tock Tone",
                                                           juce::NormalisableRange<float>{0.0f, 1.0f}, 0.7f));
}
void addEffectParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                         const dsp::EffectRegistry& registry)
{
    // Every effect exists as a fixed pool of copies (docs/decisions/0010): the first keeps the 0.1
    // IDs and names, the others add the copy number to both.
    for (int index = 0; index < registry.count(); ++index)
    {
        for (int copy = 1; copy <= dsp::kInstancesPerEffect; ++copy)
        {
            const auto& descriptor = *registry.descriptor(index);
            const std::string instance = registry.instanceId(dsp::EffectRegistry::instanceIndex(index, copy));
            const juce::String name = juce::String(std::string(descriptor.displayName)) +
                                      (copy > 1 ? " " + juce::String(copy) : juce::String());

            layout.add(std::make_unique<juce::AudioParameterBool>(
                id(ParameterIds::effect(instance, ParameterIds::kEffectEnabled)), name + " On", false));
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id(ParameterIds::effect(instance, ParameterIds::kEffectMix)), name + " Mix",
                juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, descriptor.defaultMix * 100.0f,
                juce::AudioParameterFloatAttributes{}.withLabel("%")));

            for (const auto& spec : registry.parameters(index))
            {
                const auto parameterId = id(ParameterIds::effect(instance, spec.idSuffix));
                const juce::String parameterName = name + " " + juce::String(std::string(spec.name));
                const auto unit = juce::String(std::string(spec.unit));

                switch (spec.kind)
                {
                case dsp::ParameterKind::toggle:
                    layout.add(std::make_unique<juce::AudioParameterBool>(parameterId, parameterName,
                                                                          spec.defaultValue >= 0.5f));
                    break;
                case dsp::ParameterKind::choice:
                {
                    juce::StringArray choices;
                    for (const auto& choice : spec.choices)
                        choices.add(juce::String(std::string(choice)));
                    layout.add(std::make_unique<juce::AudioParameterChoice>(
                        parameterId, parameterName, choices, static_cast<int>(spec.defaultValue)));
                    break;
                }
                case dsp::ParameterKind::integer:
                    layout.add(std::make_unique<juce::AudioParameterInt>(
                        parameterId, parameterName, static_cast<int>(spec.minimum),
                        static_cast<int>(spec.maximum), static_cast<int>(spec.defaultValue),
                        juce::AudioParameterIntAttributes{}.withLabel(unit)));
                    break;
                case dsp::ParameterKind::number:
                {
                    juce::NormalisableRange<float> range{spec.minimum, spec.maximum, spec.step};
                    if (spec.skewCentre > spec.minimum && spec.skewCentre < spec.maximum)
                        range.setSkewForCentre(spec.skewCentre);
                    layout.add(std::make_unique<juce::AudioParameterFloat>(
                        parameterId, parameterName, range, spec.defaultValue,
                        juce::AudioParameterFloatAttributes{}.withLabel(unit)));
                    break;
                }
                }
            }
        }
    }
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const dsp::EffectRegistry& registry)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    addGlobalParameters(layout);
    addEnvelopeParameters(layout);
    for (int index = 0; index < dsp::kNumOscillators; ++index)
        addOscillatorParameters(layout, index);
    addTockParameters(layout);
    addEffectParameters(layout, registry);

    return layout;
}

} // namespace fcm::core

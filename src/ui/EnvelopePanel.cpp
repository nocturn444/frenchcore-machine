// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EnvelopePanel.h"

#include "core/ParameterIds.h"
#include "dsp/envelope/EnvelopePresets.h"
#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
constexpr int kMargin = 8;
constexpr int kTitleHeight = 28;
constexpr int kKnobRowHeight = 100;
constexpr int kValueBoxHeight = 18;
constexpr int kSyncButtonWidth = 64;

using dsp::AmpKnob;
using dsp::BreakpointEnvelope;
using dsp::PitchKnob;
} // namespace

EnvelopePanel::Config EnvelopePanel::ampConfig(juce::AudioProcessorValueTreeState& parameters)
{
    Config config;
    config.title = "Amp Envelope";

    const auto knob = [](const char* name, double min, double max, double interval, double mid,
                         const char* suffix, double scale, bool isTime, AmpKnob which)
    {
        return ShapeKnob{name,
                         min,
                         max,
                         interval,
                         mid,
                         suffix,
                         scale,
                         isTime,
                         [which](const BreakpointEnvelope& e) { return e.knob(which); },
                         [which](BreakpointEnvelope& e, float v) { e.setKnob(which, v); }};
    };

    config.shapeKnobs = {knob("Attack", 1.0, 500.0, 0.1, 25.0, " ms", 1.0, true, AmpKnob::attack),
                         knob("Hold", 0.0, 1000.0, 0.1, 60.0, " ms", 1.0, true, AmpKnob::hold),
                         knob("Decay", 0.0, 2000.0, 1.0, 150.0, " ms", 1.0, true, AmpKnob::decay),
                         knob("Sustain", 0.0, 1.0, 0.001, 0.0, " %", 100.0, false, AmpKnob::sustain),
                         knob("Release", BreakpointEnvelope::kMinReleaseMs, 2000.0, 1.0, 120.0, " ms", 1.0,
                              true, AmpKnob::release)};

    config.trailingKnobs = {{"Time Scale", core::ParameterIds::kAmpEnvTimeScale}};

    // The Punch / Tail split is in real milliseconds; the display is in envelope time, which the
    // time scale stretches.
    auto* split = parameters.getRawParameterValue(core::ParameterIds::kPunchTailSplit);
    auto* scale = parameters.getRawParameterValue(core::ParameterIds::kAmpEnvTimeScale);
    config.markerMs = [split, scale]() -> std::optional<float> { return split->load() / scale->load(); };
    return config;
}

EnvelopePanel::Config EnvelopePanel::pitchConfig()
{
    Config config;
    config.title = "Pitch Envelope";
    config.parameterKnobs = {{"Amount", core::ParameterIds::kPitchEnvAmount}};

    const auto knob = [](const char* name, double min, double max, double interval, double mid,
                         const char* suffix, double scale, bool isTime, PitchKnob which)
    {
        return ShapeKnob{name,
                         min,
                         max,
                         interval,
                         mid,
                         suffix,
                         scale,
                         isTime,
                         [which](const BreakpointEnvelope& e) { return e.knob(which); },
                         [which](BreakpointEnvelope& e, float v) { e.setKnob(which, v); }};
    };

    config.shapeKnobs = {knob("Attack", 0.0, 200.0, 0.1, 20.0, " ms", 1.0, true, PitchKnob::attack),
                         knob("Decay", 1.0, 1000.0, 1.0, 100.0, " ms", 1.0, true, PitchKnob::decay),
                         knob("Curve", -1.0, 1.0, 0.01, 0.0, "", 1.0, false, PitchKnob::curve),
                         knob("End", 0.0, 1.0, 0.001, 0.0, " %", 100.0, false, PitchKnob::end)};

    config.trailingKnobs = {{"Time Scale", core::ParameterIds::kPitchEnvTimeScale}};
    return config;
}

EnvelopePanel::EnvelopePanel(core::EnvelopeState& envelopeState,
                             juce::AudioProcessorValueTreeState& parameters,
                             std::function<float()> currentTempo, Config panelConfig)
    : state(envelopeState), tempo(std::move(currentTempo)), config(std::move(panelConfig)),
      editor(envelopeState, tempo, config.markerMs)
{
    title.setText(config.title, juce::dontSendNotification);
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(title);

    // Sync: the envelope's times follow the tempo and the knobs show musical figures.
    syncButton.setTitle("Sync");
    syncButton.onClick = [this]
    {
        const bool on = syncButton.getToggleState();
        const float bpm = tempo();
        state.edit(
            [&](BreakpointEnvelope& envelope)
            {
                if (on)
                    envelope.enableSync(bpm);
                else
                    envelope.disableSync(bpm);
            });
    };
    addAndMakeVisible(syncButton);

    // Presets: choosing one replaces the shape; the menu then goes back to its prompt, since any
    // further editing makes the shape something else.
    presets.setTextWhenNothingSelected("Load shape...");
    const auto& table = dsp::EnvelopePresets::all(state.kind());
    for (size_t i = 0; i < table.size(); ++i)
        presets.addItem(table[i].name, static_cast<int>(i) + 1);
    presets.onChange = [this]
    {
        const int id = presets.getSelectedId();
        if (id <= 0)
            return;

        // A preset is a shape in milliseconds; a synced envelope keeps following the tempo with it.
        const bool wasSynced = state.get().isSynced();
        auto shape = dsp::EnvelopePresets::all(state.kind())[static_cast<size_t>(id - 1)].envelope;
        if (wasSynced)
            shape.enableSync(tempo());
        state.set(shape);
        presets.setSelectedId(0, juce::dontSendNotification);
    };
    addAndMakeVisible(presets);
    addAndMakeVisible(editor);

    for (const auto& spec : config.parameterKnobs)
        addParameterKnob(knobs, spec, parameters);

    for (const auto& spec : config.shapeKnobs)
    {
        Knob knob;
        knob.shape = &spec;
        styleKnob(knob, spec.name);
        const size_t index = knobs.size();
        knob.slider->onValueChange = [this, index]
        {
            if (!refreshing)
                editShapeKnob(index, knobs[index].slider->getValue());
        };
        knobs.push_back(std::move(knob));
    }

    for (const auto& spec : config.trailingKnobs)
        addParameterKnob(knobs, spec, parameters);

    state.addChangeListener(this);
    refreshKnobs();
}

EnvelopePanel::~EnvelopePanel()
{
    state.removeChangeListener(this);
}

void EnvelopePanel::styleKnob(Knob& knob, const juce::String& name)
{
    knob.slider =
        std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
    knob.slider->setTitle(name);
    knob.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, kValueBoxHeight);
    knob.label = std::make_unique<juce::Label>(juce::String(), name);
    knob.label->setJustificationType(juce::Justification::centred);
    knob.label->setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(*knob.slider);
    addAndMakeVisible(*knob.label);
}

void EnvelopePanel::addParameterKnob(std::vector<Knob>& into, const ParameterKnob& spec,
                                     juce::AudioProcessorValueTreeState& parameters)
{
    Knob knob;
    styleKnob(knob, spec.name);
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(spec.parameterId)))
    {
        if (ranged->getLabel().isNotEmpty())
            knob.slider->setTextValueSuffix(" " + ranged->getLabel());
        knob.attachment = std::make_unique<juce::SliderParameterAttachment>(*ranged, *knob.slider);
    }
    into.push_back(std::move(knob));
}

void EnvelopePanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshKnobs();
}

void EnvelopePanel::showMilliseconds(Knob& knob)
{
    const auto& spec = *knob.shape;
    juce::NormalisableRange<double> range(spec.minimum, spec.maximum, spec.interval);
    if (spec.midPoint > spec.minimum && spec.midPoint < spec.maximum)
        range.setSkewForCentre(spec.midPoint);

    auto& slider = *knob.slider;
    slider.setNormalisableRange(range);
    slider.setTextValueSuffix(spec.suffix);
    slider.textFromValueFunction = nullptr;
    slider.valueFromTextFunction = nullptr;

    // A knob that shows a share (0..1 as a percentage) keeps its own text functions.
    if (!juce::approximatelyEqual(spec.displayScale, 1.0))
    {
        const double scale = spec.displayScale;
        slider.textFromValueFunction = [scale](double value)
        { return juce::String(juce::roundToInt(value * scale)); };
        slider.valueFromTextFunction = [scale](const juce::String& text)
        { return text.getDoubleValue() / scale; };
    }
    knob.showsFigures = false;
}

void EnvelopePanel::showFigures(Knob& knob, float referenceBpm)
{
    const auto& spec = *knob.shape;
    *knob.choices = dsp::timeChoices(static_cast<float>(spec.minimum), static_cast<float>(spec.maximum),
                                     static_cast<double>(referenceBpm));

    auto& slider = *knob.slider;
    const auto choices = knob.choices;
    slider.setNormalisableRange(
        juce::NormalisableRange<double>(0.0, static_cast<double>(choices->size() - 1), 1.0));
    slider.setTextValueSuffix({});
    slider.textFromValueFunction = [choices](double value)
    {
        const auto index = juce::jlimit(0, static_cast<int>(choices->size()) - 1, juce::roundToInt(value));
        return juce::String((*choices)[static_cast<size_t>(index)].label);
    };
    slider.valueFromTextFunction = [choices](const juce::String& text)
    {
        for (size_t i = 0; i < choices->size(); ++i)
            if (juce::String((*choices)[i].label).equalsIgnoreCase(text.trim()))
                return static_cast<double>(i);
        return 0.0;
    };
    knob.showsFigures = true;
}

void EnvelopePanel::editShapeKnob(size_t index, double sliderValue)
{
    const auto& spec = *knobs[index].shape;
    const float bpm = tempo();

    state.edit(
        [&](BreakpointEnvelope& envelope)
        {
            if (spec.isTime && envelope.isSynced())
            {
                // The figure is a length at the current tempo: rebase first so every other time is
                // expressed at that tempo too, and the new one is exact.
                envelope.rebaseTo(bpm);
                const auto choices =
                    dsp::timeChoices(static_cast<float>(spec.minimum), static_cast<float>(spec.maximum),
                                     static_cast<double>(bpm));
                const auto choice =
                    juce::jlimit(0, static_cast<int>(choices.size()) - 1, juce::roundToInt(sliderValue));
                spec.set(envelope, choices[static_cast<size_t>(choice)].ms);
            }
            else
            {
                spec.set(envelope, static_cast<float>(sliderValue));
            }
        });
}

void EnvelopePanel::refreshKnobs()
{
    // The model changed (a drag in the display, a preset, a knob, sync, a restored state): bring the
    // knobs along without sending their changes back.
    const juce::ScopedValueSetter<bool> guard(refreshing, true);
    const auto& envelope = state.get();
    syncButton.setToggleState(envelope.isSynced(), juce::dontSendNotification);

    for (auto& knob : knobs)
    {
        if (knob.shape == nullptr)
            continue;

        const bool figures = knob.shape->isTime && envelope.isSynced();
        if (figures)
            showFigures(knob, envelope.syncRefBpm());
        else if (knob.showsFigures || !knob.configured)
            showMilliseconds(knob);
        knob.configured = true;

        const float milliseconds = knob.shape->get(envelope);
        const double value = figures ? static_cast<double>(dsp::nearestChoice(*knob.choices, milliseconds))
                                     : static_cast<double>(milliseconds);
        knob.slider->setValue(value, juce::dontSendNotification);
        knob.slider->updateText();
    }
}

void EnvelopePanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(style::kPanel);
    g.fillRoundedRectangle(bounds, style::kCornerSize);
    g.setColour(style::kPanelBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), style::kCornerSize, 1.0f);
}

void EnvelopePanel::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto header = area.removeFromTop(kTitleHeight);
    presets.setBounds(header.removeFromRight(160));
    header.removeFromRight(kMargin);
    syncButton.setBounds(header.removeFromRight(kSyncButtonWidth));
    title.setBounds(header);

    const auto layoutKnobs = [](std::vector<Knob>& row, juce::Rectangle<int> bounds, int cellWidth)
    {
        for (auto& knob : row)
        {
            auto cell = bounds.removeFromLeft(cellWidth);
            knob.label->setBounds(cell.removeFromTop(16));
            knob.slider->setBounds(cell);
        }
    };

    auto knobRow = area.removeFromBottom(kKnobRowHeight);
    if (!knobs.empty())
        layoutKnobs(knobs, knobRow, knobRow.getWidth() / static_cast<int>(knobs.size()));

    area.removeFromBottom(4);
    editor.setBounds(area);
}

} // namespace fcm::ui

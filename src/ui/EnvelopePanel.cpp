// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/EnvelopePanel.h"

#include "core/ParameterIds.h"
#include "dsp/envelope/EnvelopePresets.h"

namespace fcm::ui
{

namespace
{
constexpr int kMargin = 8;
constexpr int kTitleHeight = 28;
constexpr int kKnobRowHeight = 100;
constexpr int kSliderRowHeight = 28;
constexpr int kSlidersPerRow = 2;
constexpr int kValueBoxHeight = 18;

using dsp::AmpKnob;
using dsp::BreakpointEnvelope;
using dsp::PitchKnob;
} // namespace

EnvelopePanel::Config EnvelopePanel::ampConfig(juce::AudioProcessorValueTreeState& parameters)
{
    Config config;
    config.title = "Amp Envelope";

    const auto knob = [](const char* name, double max, double interval, double mid, const char* suffix,
                         double scale, AmpKnob which)
    {
        return ShapeKnob{name,
                         0.0,
                         max,
                         interval,
                         mid,
                         suffix,
                         scale,
                         [which](const BreakpointEnvelope& e) { return e.knob(which); },
                         [which](BreakpointEnvelope& e, float v) { e.setKnob(which, v); }};
    };

    config.shapeKnobs = {knob("Attack", 500.0, 0.1, 25.0, " ms", 1.0, AmpKnob::attack),
                         knob("Hold", 1000.0, 0.1, 60.0, " ms", 1.0, AmpKnob::hold),
                         knob("Decay", 2000.0, 1.0, 150.0, " ms", 1.0, AmpKnob::decay),
                         knob("Sustain", 1.0, 0.001, 0.0, " %", 100.0, AmpKnob::sustain),
                         knob("Release", 2000.0, 1.0, 120.0, " ms", 1.0, AmpKnob::release)};
    config.shapeKnobs.back().minimum = BreakpointEnvelope::kMinReleaseMs;

    config.sliderParameters = {{core::ParameterIds::kAmpEnvTimeScale, "Time Scale"},
                               {core::ParameterIds::kPunchTailBalance, "Punch/Tail"},
                               {core::ParameterIds::kPunchTailSplit, "Split"}};

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
                         const char* suffix, double scale, PitchKnob which)
    {
        return ShapeKnob{name,
                         min,
                         max,
                         interval,
                         mid,
                         suffix,
                         scale,
                         [which](const BreakpointEnvelope& e) { return e.knob(which); },
                         [which](BreakpointEnvelope& e, float v) { e.setKnob(which, v); }};
    };

    config.shapeKnobs = {knob("Attack", 0.0, 200.0, 0.1, 20.0, " ms", 1.0, PitchKnob::attack),
                         knob("Decay", 1.0, 1000.0, 1.0, 100.0, " ms", 1.0, PitchKnob::decay),
                         knob("Curve", -1.0, 1.0, 0.01, 0.0, "", 1.0, PitchKnob::curve),
                         knob("End", 0.0, 1.0, 0.001, 0.0, " %", 100.0, PitchKnob::end)};

    config.sliderParameters = {{core::ParameterIds::kPitchEnvTimeScale, "Time Scale"}};
    return config;
}

EnvelopePanel::EnvelopePanel(core::EnvelopeState& envelopeState,
                             juce::AudioProcessorValueTreeState& parameters, Config panelConfig)
    : state(envelopeState), config(std::move(panelConfig)), editor(envelopeState, config.markerMs)
{
    title.setText(config.title, juce::dontSendNotification);
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(title);

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
        state.set(dsp::EnvelopePresets::all(state.kind())[static_cast<size_t>(id - 1)].envelope);
        presets.setSelectedId(0, juce::dontSendNotification);
    };
    addAndMakeVisible(presets);
    addAndMakeVisible(editor);

    for (const auto& spec : config.parameterKnobs)
    {
        Knob knob;
        styleKnob(knob, spec.name);
        if (auto* parameter = parameters.getParameter(spec.parameterId);
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
        {
            if (ranged->getLabel().isNotEmpty())
                knob.slider->setTextValueSuffix(" " + ranged->getLabel());
            knob.attachment = std::make_unique<juce::SliderParameterAttachment>(*ranged, *knob.slider);
        }
        knobs.push_back(std::move(knob));
    }

    for (const auto& spec : config.shapeKnobs)
    {
        Knob knob;
        knob.shape = &spec;
        styleKnob(knob, spec.name);

        auto& slider = *knob.slider;
        slider.setRange(spec.minimum, spec.maximum, spec.interval);
        if (spec.midPoint > spec.minimum && spec.midPoint < spec.maximum)
            slider.setSkewFactorFromMidPoint(spec.midPoint);
        slider.setTextValueSuffix(spec.suffix);
        if (!juce::approximatelyEqual(spec.displayScale, 1.0))
        {
            const double scale = spec.displayScale;
            slider.textFromValueFunction = [scale](double value)
            { return juce::String(juce::roundToInt(value * scale)); };
            slider.valueFromTextFunction = [scale](const juce::String& text)
            { return text.getDoubleValue() / scale; };
        }
        slider.onValueChange = [this, &spec, &slider]
        {
            if (refreshing)
                return;
            const auto value = static_cast<float>(slider.getValue());
            state.edit([&](BreakpointEnvelope& envelope) { spec.set(envelope, value); });
        };
        knobs.push_back(std::move(knob));
    }

    for (const auto& [id, shortName] : config.sliderParameters)
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(id));
        if (ranged == nullptr)
            continue;

        ParameterSlider entry;
        entry.label = std::make_unique<juce::Label>(juce::String(), shortName);
        entry.label->setFont(juce::FontOptions(13.0f));
        entry.slider =
            std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        entry.slider->setTitle(ranged->getName(32));
        entry.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
        if (ranged->getLabel().isNotEmpty())
            entry.slider->setTextValueSuffix(" " + ranged->getLabel());
        entry.attachment = std::make_unique<juce::SliderParameterAttachment>(*ranged, *entry.slider);
        addAndMakeVisible(*entry.label);
        addAndMakeVisible(*entry.slider);
        sliders.push_back(std::move(entry));
    }

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

void EnvelopePanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshKnobs();
}

void EnvelopePanel::refreshKnobs()
{
    // The model changed (a drag in the display, a preset, a restored state): bring the knobs along
    // without sending their changes back.
    const juce::ScopedValueSetter<bool> guard(refreshing, true);
    for (auto& knob : knobs)
        if (knob.shape != nullptr)
            knob.slider->setValue(static_cast<double>(knob.shape->get(state.get())),
                                  juce::dontSendNotification);
}

void EnvelopePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1d1f25));
}

void EnvelopePanel::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto header = area.removeFromTop(kTitleHeight);
    presets.setBounds(header.removeFromRight(160));
    title.setBounds(header);

    // The small sliders go in rows of two under the knobs.
    const int numSliders = static_cast<int>(sliders.size());
    const int sliderRows = (numSliders + kSlidersPerRow - 1) / kSlidersPerRow;
    auto bottom = area.removeFromBottom(sliderRows * kSliderRowHeight);
    for (int row = 0; row < sliderRows; ++row)
    {
        auto rowArea = bottom.removeFromTop(kSliderRowHeight);
        const int inRow = juce::jmin(kSlidersPerRow, numSliders - row * kSlidersPerRow);
        const int each = rowArea.getWidth() / kSlidersPerRow;
        for (int i = 0; i < inRow; ++i)
        {
            auto& entry = sliders[static_cast<size_t>(row * kSlidersPerRow + i)];
            auto cell = rowArea.removeFromLeft(each).reduced(4, 2);
            entry.label->setBounds(cell.removeFromLeft(78));
            entry.slider->setBounds(cell);
        }
    }

    auto knobRow = area.removeFromBottom(kKnobRowHeight);
    if (!knobs.empty())
    {
        const int each = knobRow.getWidth() / static_cast<int>(knobs.size());
        for (auto& knob : knobs)
        {
            auto cell = knobRow.removeFromLeft(each);
            knob.label->setBounds(cell.removeFromTop(16));
            knob.slider->setBounds(cell);
        }
    }

    area.removeFromBottom(4);
    editor.setBounds(area);
}

} // namespace fcm::ui

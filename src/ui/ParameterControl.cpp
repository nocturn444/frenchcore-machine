// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/ParameterControl.h"

#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
constexpr int kNameHeight = 15;
constexpr int kValueHeight = 16;
constexpr int kCompactNameWidth = 54;
constexpr int kCompactValueWidth = 62;
} // namespace

ParameterControl::ParameterControl(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                                   juce::String label, bool compact)
    : isCompact(compact), parameter(state.getParameter(parameterId))
{
    jassert(parameter != nullptr);
    if (parameter == nullptr)
        return;

    const auto text = label.isNotEmpty() ? label : parameter->getName(32);
    name.setText(text, juce::dontSendNotification);
    name.setFont(juce::FontOptions(12.5f));
    name.setColour(juce::Label::textColourId, style::kMutedText);
    name.setMinimumHorizontalScale(0.7f);
    name.setJustificationType(compact ? juce::Justification::centredLeft : juce::Justification::centred);
    name.setInterceptsMouseClicks(false, false);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter))
    {
        controlKind = Kind::choice;
        combo = std::make_unique<juce::ComboBox>();
        combo->setTitle(text);
        combo->addItemList(choice->choices, 1);
        comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*parameter, *combo);
        addAndMakeVisible(*combo);
        if (!compact)
            addAndMakeVisible(name);
    }
    else if (parameter->isBoolean())
    {
        controlKind = Kind::toggle;
        toggle = std::make_unique<juce::ToggleButton>(text);
        toggle->setTitle(text);
        buttonAttachment = std::make_unique<juce::ButtonParameterAttachment>(*parameter, *toggle);
        addAndMakeVisible(*toggle);
    }
    else
    {
        controlKind = Kind::knob;
        slider = std::make_unique<juce::Slider>(
            compact ? juce::Slider::LinearHorizontal : juce::Slider::RotaryVerticalDrag,
            compact ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow);
        slider->setTitle(text);
        slider->setTextBoxStyle(compact ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow, false,
                                compact ? kCompactValueWidth : 70, kValueHeight);
        if (parameter->getLabel().isNotEmpty())
            slider->setTextValueSuffix(" " + parameter->getLabel());
        sliderAttachment = std::make_unique<juce::SliderParameterAttachment>(*parameter, *slider);
        slider->setDoubleClickReturnValue(
            true, static_cast<double>(parameter->convertFrom0to1(parameter->getDefaultValue())));

        // Short, readable numbers: kilohertz from 1000 Hz up, and two decimals for a continuous value
        // instead of the seven the parameter would print.
        const auto* ranged = parameter;
        if (parameter->getLabel() == "Hz")
        {
            slider->setTextValueSuffix({});
            slider->textFromValueFunction = [](double hz) {
                return hz >= 1000.0 ? juce::String(hz / 1000.0, 2) + "k" : juce::String(juce::roundToInt(hz));
            };
            slider->valueFromTextFunction = [](const juce::String& text)
            {
                const auto trimmed = text.trim();
                const double number = trimmed.getDoubleValue();
                return trimmed.endsWithIgnoreCase("k") ? number * 1000.0 : number;
            };
        }
        else if (ranged->getNormalisableRange().interval <= 0.0f && ranged->getNumSteps() > 1000)
        {
            slider->textFromValueFunction = [](double value) { return juce::String(value, 2); };
            slider->valueFromTextFunction = [](const juce::String& text) { return text.getDoubleValue(); };
        }
        slider->updateText();
        addAndMakeVisible(*slider);
        addAndMakeVisible(name);
    }
}

void ParameterControl::setWheelAdjusts(bool adjusts)
{
    if (slider != nullptr)
        slider->setScrollWheelEnabled(adjusts);
}

void ParameterControl::addToggledListener(std::function<void(bool)> listener)
{
    if (toggle == nullptr)
        return;

    toggledListeners.push_back(std::move(listener));
    toggle->onClick = [this]
    {
        for (const auto& each : toggledListeners)
            each(toggle->getToggleState());
    };
}

void ParameterControl::resized()
{
    auto area = getLocalBounds();
    switch (controlKind)
    {
    case Kind::toggle:
        if (toggle != nullptr)
            toggle->setBounds(isCompact ? area
                                        : area.withSizeKeepingCentre(area.getWidth(), kSwitchHeight - 8));
        break;
    case Kind::choice:
        if (combo != nullptr && isCompact)
        {
            combo->setBounds(area);
        }
        else if (combo != nullptr)
        {
            area = area.withSizeKeepingCentre(area.getWidth(), kSwitchHeight);
            name.setBounds(area.removeFromTop(kNameHeight));
            combo->setBounds(area.reduced(2, 0));
        }
        break;
    case Kind::knob:
        if (slider != nullptr)
        {
            if (isCompact)
            {
                name.setBounds(area.removeFromLeft(kCompactNameWidth));
                slider->setBounds(area);
            }
            else
            {
                name.setBounds(area.removeFromTop(kNameHeight));
                // A narrow cell gets a narrow value box.
                slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, juce::jmin(70, area.getWidth()),
                                        kValueHeight);
                slider->setBounds(area);
            }
        }
        break;
    }
}

} // namespace fcm::ui

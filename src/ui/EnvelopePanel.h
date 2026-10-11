// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/EnvelopeEditor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// One envelope's panel (docs/specs/envelope-breakpoint.md §2): a preset menu, the display, five
// knobs that move the main points of the same line, and the envelope's global parameters. The
// same class serves the amp and the pitch envelope; only the data differs.
class EnvelopePanel : public juce::Component, private juce::ChangeListener
{
  public:
    // A knob on the envelope shape (not an automatable parameter).
    struct ShapeKnob
    {
        juce::String name;
        double minimum, maximum, interval;
        double midPoint;     // value at the middle of the knob, 0 for a linear knob
        juce::String suffix; // shown after the number
        double displayScale; // 100 shows a 0..1 value as a percentage
        std::function<float(const dsp::BreakpointEnvelope&)> get;
        std::function<void(dsp::BreakpointEnvelope&, float)> set;
    };

    // A knob that edits an automatable parameter (the pitch Amount).
    struct ParameterKnob
    {
        juce::String name;
        juce::String parameterId;
    };

    struct Config
    {
        juce::String title;
        std::vector<ShapeKnob> shapeKnobs;
        std::vector<ParameterKnob> parameterKnobs; // shown first
        // Small sliders under the knobs: parameter ID and the short name shown next to it.
        std::vector<std::pair<juce::String, juce::String>> sliderParameters;
        std::function<std::optional<float>()> markerMs; // see EnvelopeEditor
    };

    EnvelopePanel(core::EnvelopeState& state, juce::AudioProcessorValueTreeState& parameters, Config config);
    ~EnvelopePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // The two configurations of the MVP.
    static Config ampConfig(juce::AudioProcessorValueTreeState& parameters);
    static Config pitchConfig();

  private:
    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        const ShapeKnob* shape = nullptr; // null for a parameter knob
    };

    struct ParameterSlider
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshKnobs();
    void styleKnob(Knob& knob, const juce::String& name);

    core::EnvelopeState& state;
    Config config;

    juce::Label title;
    juce::ComboBox presets;
    EnvelopeEditor editor;
    std::vector<Knob> knobs;
    std::vector<ParameterSlider> sliders;
    bool refreshing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopePanel)
};

} // namespace fcm::ui

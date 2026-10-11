// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/envelope/MusicalTime.h"
#include "ui/EnvelopeEditor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// One envelope's panel (docs/specs/envelope-breakpoint.md §2): a preset menu, a Sync switch, the
// display, five knobs that move the main points of the same line, and the envelope's global
// parameters. The same class serves the amp and the pitch envelope; only the data differs.
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
        bool isTime;         // a time in milliseconds: shown as a musical figure when the envelope is synced
        std::function<float(const dsp::BreakpointEnvelope&)> get;
        std::function<void(dsp::BreakpointEnvelope&, float)> set;
    };

    // A knob that edits an automatable parameter.
    struct ParameterKnob
    {
        juce::String name;
        juce::String parameterId;
    };

    struct Config
    {
        juce::String title;
        std::vector<ShapeKnob> shapeKnobs;
        std::vector<ParameterKnob> parameterKnobs; // shown first, in the main knob row
        // Small sliders under the knobs: parameter ID and the short name shown next to it.
        std::vector<std::pair<juce::String, juce::String>> sliderParameters;
        std::vector<ParameterKnob> extraKnobs;          // a second knob row under the sliders
        std::function<std::optional<float>()> markerMs; // see EnvelopeEditor
    };

    EnvelopePanel(core::EnvelopeState& state, juce::AudioProcessorValueTreeState& parameters,
                  std::function<float()> currentTempo, Config config);
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
        // The steps of a time knob while synced (shared with the slider's text functions).
        std::shared_ptr<std::vector<dsp::TimeChoice>> choices =
            std::make_shared<std::vector<dsp::TimeChoice>>();
        bool showsFigures = false;
        bool configured = false;
    };

    struct ParameterSlider
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshKnobs();
    void showMilliseconds(Knob& knob);
    void showFigures(Knob& knob, float referenceBpm);
    void styleKnob(Knob& knob, const juce::String& name);
    void addParameterKnob(std::vector<Knob>& into, const ParameterKnob& spec,
                          juce::AudioProcessorValueTreeState& parameters);
    void editShapeKnob(size_t index, double sliderValue);

    core::EnvelopeState& state;
    std::function<float()> tempo;
    Config config;

    juce::Label title;
    juce::ToggleButton syncButton{"Sync"};
    juce::ComboBox presets;
    EnvelopeEditor editor;
    std::vector<Knob> knobs;
    std::vector<Knob> extraKnobs;
    std::vector<ParameterSlider> sliders;
    bool refreshing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopePanel)
};

} // namespace fcm::ui

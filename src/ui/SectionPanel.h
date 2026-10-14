// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "ui/ParameterControl.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// A titled box of the editor grid (docs/UI_LAYOUT.md). It paints the box and gives the area inside it.
class SectionPanel : public juce::Component
{
  public:
    explicit SectionPanel(juce::String panelTitle = {}) : title(std::move(panelTitle)) {}

    void paint(juce::Graphics& g) override;

  protected:
    // The area inside the box, below the title.
    juce::Rectangle<int> contentArea() const;

  private:
    juce::String title;
};

// A box whose content is rows of parameter controls (and a few custom components, like a Load button).
// Rows with a knob share the free height; rows of only switches and menus keep a fixed height.
class ControlPanel : public SectionPanel
{
  public:
    struct Item
    {
        juce::String parameterId;
        juce::String label; // empty: the parameter's own name
    };

    ControlPanel(juce::AudioProcessorValueTreeState& state, juce::String title);

    // Starts a new row of the given parameters. Returns the row number.
    int addRow(const std::vector<Item>& items);

    // Adds a component at the end of a row; it takes `weight` shares of the width (a control takes 1).
    void addCustom(int row, juce::Component& component, float weight = 1.0f);

    void resized() override;

  private:
    struct Cell
    {
        juce::Component* component;
        float weight;
    };

    struct Row
    {
        std::vector<Cell> cells;
        bool hasKnob = false;
    };

    juce::AudioProcessorValueTreeState& parameters;
    std::vector<std::unique_ptr<ParameterControl>> controls;
    std::vector<Row> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanel)
};

// A box kept free for a later feature (the LFOs, "Other"): it shows its name and a short note
// faintly (for "Other", the licence notice until the box gets its own contents).
class PlaceholderPanel : public SectionPanel
{
  public:
    explicit PlaceholderPanel(juce::String panelTitle, juce::String note);

    void paint(juce::Graphics& g) override;

  private:
    juce::String note;
};

} // namespace fcm::ui

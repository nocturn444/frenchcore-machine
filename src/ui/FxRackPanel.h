// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/FxOrderState.h"
#include "dsp/fx/EffectRegistry.h"
#include "ui/ParameterControl.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// The effects, one strip each from top to bottom in the order of the sound, with a handle bar to drag
// them into another order (docs/specs/fx-rack.md). It only reads and moves the order through
// FxOrderState; the controls are attached to the parameters in the usual way. Strips are built from
// the effects' parameter descriptions, so a new effect needs no code here.
class FxRackPanel : public juce::Component, private juce::ChangeListener
{
  public:
    FxRackPanel(juce::AudioProcessorValueTreeState& state, core::FxOrderState& order,
                const dsp::EffectRegistry& registry);
    ~FxRackPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // The height all the strips need together, for the editor's layout check.
    int preferredHeight() const;

  private:
    // One effect: a handle bar with its On and Mix, an optional picture and its own controls.
    class Slot : public juce::Component
    {
      public:
        Slot(FxRackPanel& owner, const dsp::EffectRegistry& registry, int registryIndex,
             juce::AudioProcessorValueTreeState& state);

        const juce::String& id() const noexcept { return effectId; }
        int preferredHeight() const noexcept { return height; }

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

      private:
        // Controls that belong together (an EQ band); an effect without sub-groups has one, unnamed.
        struct Group
        {
            juce::String name;
            std::vector<std::unique_ptr<ParameterControl>> switches; // on/off and menus
            std::vector<std::unique_ptr<ParameterControl>> knobs;
        };

        static constexpr int kHandleHeight = 26;

        FxRackPanel& rack;
        juce::String effectId;
        juce::String name;
        std::unique_ptr<ParameterControl> onSwitch;
        std::unique_ptr<ParameterControl> mixSlider;
        std::unique_ptr<juce::Component> decoration;
        int decorationHeight = 0;
        std::vector<Group> groups;
        int height = kHandleHeight;
        bool dragging = false;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void layoutSlots();
    void dragSlot(const juce::String& effectId, const juce::MouseEvent& e);
    std::vector<Slot*> slotsInOrder() const;

    core::FxOrderState& order;
    std::vector<std::unique_ptr<Slot>> slots; // in registration order
};

} // namespace fcm::ui

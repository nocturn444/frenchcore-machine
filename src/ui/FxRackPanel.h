// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/FxOrderState.h"
#include "dsp/fx/EffectRegistry.h"
#include "ui/GenericParameterPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// The effects, one column each in the order of the sound, with a handle bar to drag them into another
// order (docs/specs/fx-rack.md). It only reads and moves the order through FxOrderState; the controls
// are attached to the parameters in the usual way.
class FxRackPanel : public juce::Component, private juce::ChangeListener
{
  public:
    FxRackPanel(juce::AudioProcessorValueTreeState& state, core::FxOrderState& order,
                const dsp::EffectRegistry& registry);
    ~FxRackPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    // One effect: a handle bar and the controls of its group.
    class Slot : public juce::Component
    {
      public:
        Slot(FxRackPanel& owner, juce::String effectId, const juce::String& displayName,
             juce::AudioProcessorValueTreeState& state);

        const juce::String& id() const noexcept { return effectId; }

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

      private:
        static constexpr int kHandleHeight = 22;

        FxRackPanel& rack;
        juce::String effectId;
        juce::String name;
        GenericParameterPanel controls;
        bool dragging = false;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void layoutSlots();
    void dragSlot(const juce::String& effectId, const juce::MouseEvent& e);

    core::FxOrderState& order;
    std::vector<std::unique_ptr<Slot>> slots; // in registration order
};

} // namespace fcm::ui

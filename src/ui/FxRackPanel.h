// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/FxRackState.h"
#include "dsp/fx/EffectRegistry.h"
#include "ui/ParameterControl.h"

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace fcm::ui
{

// One rack of effects: one strip each from top to bottom in the order of the sound, with a handle bar
// to drag them into another order, "++" to duplicate and "x" to remove, and "+ Add effect" at the
// end (docs/specs/mixer-and-buses.md section 2, docs/specs/fx-rack.md). The strips scroll when they
// do not fit. It only reads and changes the rack through FxRackState; the controls are attached to
// the parameters in the usual way. Strips are built from the effects' parameter descriptions, so a
// new effect needs no code here.
class FxRackPanel : public juce::Component, private juce::ChangeListener
{
  public:
    // `note` gives the pitch the tail is on (semitones from C3) to the pictures that follow it.
    FxRackPanel(juce::AudioProcessorValueTreeState& state, core::FxRackState& racks, dsp::RackId rack,
                const dsp::EffectRegistry& registry, std::function<float()> note = {});
    ~FxRackPanel() override;

    void resized() override;

    // The height all the strips and the Add button need together.
    int preferredHeight() const;

  private:
    // One effect instance: a handle bar with its name, On, Mix, duplicate and remove, an optional
    // picture and its own controls.
    class Slot : public juce::Component
    {
      public:
        Slot(FxRackPanel& owner, const dsp::EffectRegistry& registry, const std::string& instanceId,
             juce::AudioProcessorValueTreeState& state, const std::function<float()>& note);

        const std::string& id() const noexcept { return instance; }
        int preferredHeight() const noexcept { return height; }
        void setCanDuplicate(bool allowed) { duplicateButton.setEnabled(allowed); }
        // The height the strip needs now: it grows by a row of knobs when a group has more than three.
        void updateHeight();

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

      private:
        // Controls that belong together (an EQ band); an effect without sub-groups has one, unnamed.
        struct Group
        {
            juce::String key;  // what the parameter IDs share before the dot ("b1")
            juce::String name; // its title ("Band 1")
            std::vector<std::unique_ptr<ParameterControl>> switches; // on/off and menus
            std::vector<std::unique_ptr<ParameterControl>> knobs;
        };

        static constexpr int kHandleHeight = 26;

        FxRackPanel& rack;
        std::string instance;
        juce::String name;
        std::unique_ptr<ParameterControl> onSwitch;
        std::unique_ptr<ParameterControl> mixSlider;
        juce::TextButton duplicateButton{"++"};
        juce::TextButton removeButton{"x"};
        std::unique_ptr<juce::Component> decoration;
        int decorationHeight = 0;
        std::vector<Group> groups;
        // Show or hide controls when the parameter they depend on changes (ParameterSpec::shownWhen).
        std::vector<std::unique_ptr<juce::ParameterAttachment>> conditions;
        bool ready = false;
        int height = kHandleHeight;
        bool dragging = false;
    };

    // What the viewport scrolls: the strips, the Add button and the note of an empty rack.
    class Content : public juce::Component
    {
      public:
        void paint(juce::Graphics& g) override;
        bool empty = true;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void rebuildSlots();
    void layoutSlots();
    void showAddMenu();
    void dragSlot(const std::string& instanceId, const juce::MouseEvent& e);
    std::vector<Slot*> slotsInOrder() const;

    juce::AudioProcessorValueTreeState& parameters;
    core::FxRackState& racks;
    std::function<float()> noteSource;
    dsp::RackId rackId;
    const dsp::EffectRegistry& registry;

    juce::Viewport viewport;
    Content content;
    juce::TextButton addButton{"+ Add effect"};
    std::vector<std::unique_ptr<Slot>> slots; // only the instances that are in the rack
};

} // namespace fcm::ui

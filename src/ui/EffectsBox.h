// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/FxRackState.h"
#include "ui/FxRackPanel.h"
#include "ui/MixerPanel.h"
#include "ui/SectionPanel.h"

#include <array>
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>

namespace fcm::ui
{

// The Effects box of the window (docs/specs/mixer-and-buses.md section 2, ui-grid-layout.md section 8):
// four tabs at its top, Mixer, Bus A, Bus B and Main, over the mixer or one of the three racks. A new
// window opens on Main; the tab chosen is kept while it is open.
class EffectsBox : public SectionPanel, private juce::ChangeListener
{
  public:
    enum Tab
    {
        mixerTab,
        busATab,
        busBTab,
        mainTab,
        kNumTabs
    };

    // `environment` is what the effects' decorations are given; the box adds the way to show a larger
    // editor (docs/decisions/0012 item 4), so `environment.showEditor` is not needed.
    EffectsBox(juce::AudioProcessorValueTreeState& state, core::FxRackState& racks,
               const dsp::EffectRegistry& registry, DecorationEnvironment environment = {});
    ~EffectsBox() override;

    void resized() override;

    // The larger editor over the rack: at most one, closed by its Done button, by Escape, by changing
    // tab and by any change of the racks (the effect it belongs to may be gone).
    void showEditor(std::unique_ptr<juce::Component> content, const juce::String& title);
    void closeEditor();
    bool isEditorOpen() const noexcept { return editor != nullptr; }

    void showTab(Tab tab);
    Tab currentTab() const noexcept { return shown; }

  private:
    static constexpr int kTabHeight = 24;

    // The overlay: a title, a Done button and the content, over everything below the tabs.
    class Editor : public juce::Component
    {
      public:
        Editor(std::unique_ptr<juce::Component> content, const juce::String& title,
               std::function<void()> onDone);

        void paint(juce::Graphics& g) override;
        void resized() override;
        bool keyPressed(const juce::KeyPress& key) override;

      private:
        static constexpr int kHeaderHeight = 28;

        std::unique_ptr<juce::Component> content;
        juce::String title;
        juce::TextButton done{"Done"};
        std::function<void()> onDone;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    core::FxRackState& racks;
    MixerPanel mixer;
    std::array<FxRackPanel, 3> rackPanels; // Bus A, Bus B, Main
    std::array<juce::TextButton, kNumTabs> tabs;
    Tab shown = mainTab;
    std::unique_ptr<Editor> editor;
};

} // namespace fcm::ui

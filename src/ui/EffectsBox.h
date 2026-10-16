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
class EffectsBox : public SectionPanel
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

    EffectsBox(juce::AudioProcessorValueTreeState& state, core::FxRackState& racks,
               const dsp::EffectRegistry& registry, std::function<float()> note = {});

    void resized() override;

    void showTab(Tab tab);
    Tab currentTab() const noexcept { return shown; }

  private:
    static constexpr int kTabHeight = 24;

    MixerPanel mixer;
    std::array<FxRackPanel, 3> rackPanels; // Bus A, Bus B, Main
    std::array<juce::TextButton, kNumTabs> tabs;
    Tab shown = mainTab;
};

} // namespace fcm::ui

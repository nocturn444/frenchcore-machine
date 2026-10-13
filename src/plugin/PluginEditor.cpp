// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginEditor.h"

#include "core/ParameterIds.h"
#include "plugin/PluginProcessor.h"
#include "ui/Style.h"

namespace fcm
{

namespace
{
namespace Ids = core::ParameterIds;

// The design grid (docs/UI_LAYOUT.md), in pixels at scale 1.
constexpr int kMargin = 8;
constexpr int kGap = ui::style::kGap;
constexpr int kOscillatorRowHeight = 280;
constexpr int kEnvelopeRowHeight = 250;
constexpr int kBottomRowHeight = 180;
constexpr int kLeftBlockWidth = 912; // the two oscillators and the two envelopes
constexpr int kLfoWidth = 300;
constexpr int kTockWidth = 440;
constexpr int kPunchTailWidth = 250;
constexpr int kGlobalWidth = 350;

constexpr int kBodyHeight = ui::PresetBar::kHeight + kGap + kOscillatorRowHeight + kGap + kEnvelopeRowHeight +
                            kGap + kBottomRowHeight + kMargin;

// Smallest and largest window, as shares of the design size.
constexpr double kMinScale = 0.6;
constexpr double kMaxScale = 1.6;
} // namespace

TockPanel::TockPanel(juce::AudioProcessorValueTreeState& state, std::function<void(const juce::File&)> onLoad)
    : ControlPanel(state, "Tock"), loadRequested(std::move(onLoad))
{
    const int switches =
        addRow({{Ids::kTockOn, "On"}, {Ids::kTockMode, "Mode"}, {Ids::kTockSample, "Sample"}});
    addCustom(switches, loadButton);
    addRow({{Ids::kTockLevel, "Level"},
            {Ids::kTockTune, "Tune"},
            {Ids::kTockStart, "Start"},
            {Ids::kTockDecay, "Decay"},
            {Ids::kTockTone, "Tone"}});
    loadButton.setTooltip("Load a tock sample from a WAV file");
    loadButton.onClick = [this] { choose(); };
}

void TockPanel::choose()
{
    chooser = std::make_unique<juce::FileChooser>("Choose a Tock sample (WAV)", juce::File{}, "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& finished)
                         {
                             const auto file = finished.getResult();
                             if (file != juce::File{})
                                 loadRequested(file);
                         });
}

int PluginEditor::designHeight(bool withAudition)
{
    return kBodyHeight + (withAudition ? ui::AuditionPanel::kHeight : 0);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel(nullptr);
}

PluginEditor::PluginEditor(PluginProcessor& owner)
    : AudioProcessorEditor(owner),
      presetBar(
          owner.getPresets(), [&owner] { return owner.wavetableMessage(); }, owner.wavetableChanged()),
      osc1(owner.getState(), 0, owner.selectorFor(0),
           [&owner](const juce::File& file) { owner.loadUserWavetable(0, file); }),
      osc2(owner.getState(), 1, owner.selectorFor(1),
           [&owner](const juce::File& file) { owner.loadUserWavetable(1, file); }),
      ampPanel(
          owner.getAmpEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::ampConfig(owner.getState())),
      pitchPanel(
          owner.getPitchEnvelope(), owner.getState(), [&owner] { return owner.currentTempo(); },
          ui::EnvelopePanel::pitchConfig()),
      fxRack(owner.getState(), owner.getFxOrder(), dsp::defaultEffectRegistry()), fxArea(fxRack),
      lfoPanel("LFO", "Reserved for a later version"),
      tockPanel(owner.getState(), [&owner](const juce::File& file) { owner.loadUserTockSample(file); }),
      punchTailPanel(owner.getState(), "Punch / Tail"), globalPanel(owner.getState(), "Global"),
      otherPanel("Other", "Reserved")
{
    punchTailPanel.addRow({{Ids::kPunchTailSplit, "Split"},
                           {Ids::kPunchLevel, "Punch Level"},
                           {Ids::kTailLevel, "Tail Level"}});

    globalPanel.addRow({{Ids::kOutputGain, "Output"},
                        {Ids::kRootNote, "Root Note"},
                        {Ids::kGlideTime, "Glide Time"},
                        {Ids::kVelocitySensitivity, "Velocity"}});
    globalPanel.addRow({{Ids::kLimiterOn, "Limiter"},
                        {Ids::kKeyTrack, "Key Track"},
                        {Ids::kGlideOn, "Glide"},
                        {Ids::kQuality, "Quality"}});

    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(content);
    for (auto* child : std::initializer_list<juce::Component*>{&presetBar, &osc1, &osc2, &ampPanel,
                                                               &pitchPanel, &fxArea, &lfoPanel, &tockPanel,
                                                               &punchTailPanel, &globalPanel, &otherPanel})
        content.addAndMakeVisible(*child);

    if (owner.isStandalone())
    {
        auditionPanel = std::make_unique<ui::AuditionPanel>(owner.getAuditionState());
        content.addAndMakeVisible(*auditionPanel);
    }

    const int height = designHeight(auditionPanel != nullptr);
    content.setSize(kDesignWidth, height);
    layoutContent();

    setResizable(true, true);
    setResizeLimits(juce::roundToInt(kDesignWidth * kMinScale), juce::roundToInt(height * kMinScale),
                    juce::roundToInt(kDesignWidth * kMaxScale), juce::roundToInt(height * kMaxScale));
    getConstrainer()->setFixedAspectRatio(static_cast<double>(kDesignWidth) / static_cast<double>(height));
    setSize(kDesignWidth, height);
}

void PluginEditor::layoutContent()
{
    // The left block, row by row.
    const int top = ui::PresetBar::kHeight + kGap;
    const int oscillatorWidth = (kLeftBlockWidth - kGap) / 2;
    presetBar.setBounds(0, 0, kDesignWidth, ui::PresetBar::kHeight);

    osc1.setBounds(kMargin, top, oscillatorWidth, kOscillatorRowHeight);
    osc2.setBounds(kMargin + oscillatorWidth + kGap, top, oscillatorWidth, kOscillatorRowHeight);

    const int envelopeTop = top + kOscillatorRowHeight + kGap;
    ampPanel.setBounds(kMargin, envelopeTop, oscillatorWidth, kEnvelopeRowHeight);
    pitchPanel.setBounds(kMargin + oscillatorWidth + kGap, envelopeTop, oscillatorWidth, kEnvelopeRowHeight);

    // The FX box takes the right side of both rows.
    const int fxLeft = kMargin + kLeftBlockWidth + kGap;
    fxArea.setBounds(fxLeft, top, kDesignWidth - kMargin - fxLeft,
                     kOscillatorRowHeight + kGap + kEnvelopeRowHeight);

    // The bottom row.
    const int bottomTop = envelopeTop + kEnvelopeRowHeight + kGap;
    int x = kMargin;
    const auto place = [&](juce::Component& component, int width)
    {
        component.setBounds(x, bottomTop, width, kBottomRowHeight);
        x += width + kGap;
    };
    place(lfoPanel, kLfoWidth);
    place(tockPanel, kTockWidth);
    place(punchTailPanel, kPunchTailWidth);
    place(globalPanel, kGlobalWidth);
    place(otherPanel, kDesignWidth - kMargin - x);

    if (auditionPanel != nullptr)
        auditionPanel->setBounds(0, kBodyHeight, kDesignWidth, ui::AuditionPanel::kHeight);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::style::kWindow);
}

void PluginEditor::resized()
{
    // One scale for the whole grid. The window keeps the grid's aspect ratio, but a host may not honour
    // it, so the smaller of the two scales is used and the grid is centred.
    const float height = static_cast<float>(content.getHeight());
    const float scale = juce::jmin(static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth),
                                   static_cast<float>(getHeight()) / height);
    content.setTransform(juce::AffineTransform::scale(scale));
    content.setTopLeftPosition(
        juce::roundToInt((static_cast<float>(getWidth()) - kDesignWidth * scale) / 2.0f),
        juce::roundToInt((static_cast<float>(getHeight()) - height * scale) / 2.0f));
}

} // namespace fcm

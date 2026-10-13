// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/OscillatorPanel.h"

#include "core/ParameterIds.h"
#include "ui/Style.h"

namespace fcm::ui
{

namespace
{
namespace Osc = core::ParameterIds::Oscillator;

constexpr int kStripHeight = 24;
constexpr int kKnobRowHeight = 92;
constexpr int kLoadWidth = 70;
constexpr int kOnWidth = 66;
} // namespace

OscillatorPanel::OscillatorPanel(juce::AudioProcessorValueTreeState& state, int oscillator,
                                 core::WavetableSelector& selector,
                                 std::function<void(const juce::File&)> onLoad)
    : SectionPanel("Oscillator " + juce::String(oscillator + 1)), loadRequested(std::move(onLoad)),
      display(selector, *state.getParameter(core::ParameterIds::oscillator(oscillator, Osc::kPosition)))
{
    const auto id = [oscillator](const char* suffix)
    { return juce::String(core::ParameterIds::oscillator(oscillator, suffix)); };

    onSwitch = std::make_unique<ParameterControl>(state, id(Osc::kOn), "On", true);
    wavetable = std::make_unique<ParameterControl>(state, id(Osc::kWavetable), "Wavetable", true);
    pitch = std::make_unique<ParameterControl>(state, id(Osc::kPitch), "Pitch", true);
    octave = std::make_unique<ParameterControl>(state, id(Osc::kOctave), "Octave", true);
    fine = std::make_unique<ParameterControl>(state, id(Osc::kFine), "Fine", true);
    position = std::make_unique<ParameterControl>(state, id(Osc::kPosition), "Position");
    level = std::make_unique<ParameterControl>(state, id(Osc::kLevel), "Level");
    phase = std::make_unique<ParameterControl>(state, id(Osc::kPhase), "Phase");

    for (auto* control : {onSwitch.get(), wavetable.get(), pitch.get(), octave.get(), fine.get(),
                          position.get(), level.get(), phase.get()})
        addAndMakeVisible(*control);
    addAndMakeVisible(display);
    addAndMakeVisible(loadButton);
    loadButton.setTooltip("Load a wavetable from a WAV file");
    loadButton.onClick = [this] { choose(); };
}

void OscillatorPanel::choose()
{
    chooser = std::make_unique<juce::FileChooser>("Choose a wavetable (WAV)", juce::File{}, "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& finished)
                         {
                             const auto file = finished.getResult();
                             if (file != juce::File{})
                                 loadRequested(file);
                         });
}

void OscillatorPanel::resized()
{
    auto area = contentArea();

    // Header: the title is painted by the box; On, the menu and Load share the rest of its row.
    auto header = getLocalBounds().removeFromTop(style::kTitleHeight).withTrimmedLeft(130).reduced(4, 1);
    loadButton.setBounds(header.removeFromRight(kLoadWidth).withSizeKeepingCentre(kLoadWidth, 22));
    header.removeFromRight(6);
    wavetable->setBounds(header.removeFromRight(juce::jmin(190, header.getWidth() - kOnWidth)));
    onSwitch->setBounds(header.removeFromRight(kOnWidth));

    auto strip = area.removeFromTop(kStripHeight);
    const int third = strip.getWidth() / 3;
    pitch->setBounds(strip.removeFromLeft(third));
    octave->setBounds(strip.removeFromLeft(third));
    fine->setBounds(strip);
    area.removeFromTop(4);

    auto knobs = area.removeFromBottom(kKnobRowHeight);
    const int cell = knobs.getWidth() / 3;
    position->setBounds(knobs.removeFromLeft(cell));
    level->setBounds(knobs.removeFromLeft(cell));
    phase->setBounds(knobs);
    area.removeFromBottom(4);

    display.setBounds(area);
}

} // namespace fcm::ui

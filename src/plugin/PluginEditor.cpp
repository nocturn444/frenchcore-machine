// SPDX-License-Identifier: AGPL-3.0-only
#include "plugin/PluginEditor.h"

#include "plugin/PluginProcessor.h"

namespace fcm
{

namespace
{
// Default size and the fixed aspect ratio of the editor (docs/UI_LAYOUT.md).
constexpr int kDefaultWidth = 1100;
constexpr int kDefaultHeight = 640;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& owner) : AudioProcessorEditor(owner)
{
    addAndMakeVisible(layoutPreview);

    setResizable(true, true);
    setResizeLimits(kDefaultWidth * 2 / 3, kDefaultHeight * 2 / 3, kDefaultWidth * 2, kDefaultHeight * 2);
    getConstrainer()->setFixedAspectRatio(double(kDefaultWidth) / double(kDefaultHeight));
    setSize(kDefaultWidth, kDefaultHeight);
}

void PluginEditor::resized()
{
    layoutPreview.setBounds(getLocalBounds());
}

} // namespace fcm

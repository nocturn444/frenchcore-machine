// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/oscillator/WavetableData.h"

#include <memory>
#include <vector>

namespace fcm::dsp
{

// The wavetables that ship with the plugin (docs/specs/wavetable-factory.md). They are generated
// by formulas, so they are original and free to redistribute. The list is append-only: the saved
// value of the Wavetable parameter is the index, so reordering would change old projects.
// Nothing here runs on the audio thread.
class FactoryWavetables
{
  public:
    static int count() noexcept;

    // Display name of table `index`, or an empty view when out of range.
    static const char* name(int index) noexcept;

    // Frames of table `index` (numFrames * WavetableData::kFrameSize samples). Every frame has no
    // DC offset and a peak of exactly 1. Out-of-range indices give the Sine table.
    static std::vector<float> makeFrames(int index);

    // The band-limited table, ready for the oscillator.
    static std::unique_ptr<const WavetableData> build(int index);
};

} // namespace fcm::dsp

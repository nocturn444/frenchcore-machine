// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/tock/TockSample.h"

#include <vector>

namespace fcm::core
{

// What the Tock box shows and the Detect button does with a sample (docs/specs/tock.md §8). Both are
// pure functions that run on the message thread.

// The loudest absolute value of each of `bins` equal parts of the sample, scaled so the largest is 1.
// Empty for an empty sample or fewer than one bin.
std::vector<float> waveformPeaks(const dsp::TockSample& sample, int bins);

// Where the transient of a sample is, as fractions of its length (0 to 1, start before end).
struct TransientRange
{
    double start = 0.0;
    double end = 1.0;
};

// Finds the hit at the start of a kick sample: the start is 1 ms before the first value above -30 dB of
// the peak; the end is where the energy above 1 kHz (a 1 ms RMS window) has fallen 20 dB below its
// maximum after that maximum, between 5 and 80 ms after the start. A silent or flat sample, or one
// with nothing above 1 kHz, gives the whole sample (0 and 1).
TransientRange detectTransient(const dsp::TockSample& sample);

} // namespace fcm::core

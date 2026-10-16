// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

namespace fcm::core
{

// User samples (Tock, Noise) at a rate above 48 kHz are stored at 48 kHz, so a 10-second file is at most
// 480000 samples and projects stay small.
inline constexpr double kResampleTargetRate = 48000.0;

// Brings `input` from `rate` (above kResampleTargetRate) down to kResampleTargetRate with a
// Hann-windowed sinc low-pass. Runs on a background thread.
std::vector<float> resampleDownTo48k(const std::vector<float>& input, double rate);

} // namespace fcm::core

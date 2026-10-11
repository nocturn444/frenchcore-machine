// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>

namespace fcm::core
{

// How long the host should keep calling us after the last note: the amp envelope's release,
// stretched by its time scale, plus a little margin. The time scale comes from a parameter, so a
// damaged value must never reach the host as a NaN, an infinity or a negative time.
inline double tailLengthSeconds(float releaseSeconds, float timeScale) noexcept
{
    const double release =
        std::isfinite(releaseSeconds) ? std::max(0.0, static_cast<double>(releaseSeconds)) : 0.0;
    const double scale = std::isfinite(timeScale) ? std::max(0.01, static_cast<double>(timeScale)) : 1.0;
    return release * scale + 0.05;
}

} // namespace fcm::core

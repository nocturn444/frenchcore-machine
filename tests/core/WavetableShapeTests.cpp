// SPDX-License-Identifier: AGPL-3.0-only
#include "core/WavetableShape.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::core::interpolatedFrame;

namespace
{
// Three frames of 4 samples: all 0, all 1, all 4.
const std::vector<float> kFrames{0, 0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4};
} // namespace

TEST_CASE("The first and last positions give the first and last frame", "[core][wavetable][display]")
{
    REQUIRE(interpolatedFrame(kFrames, 4, 0.0f) == std::vector<float>(4, 0.0f));
    REQUIRE(interpolatedFrame(kFrames, 4, 1.0f) == std::vector<float>(4, 4.0f));
}

TEST_CASE("Positions between frames blend the neighbours", "[core][wavetable][display]")
{
    REQUIRE(interpolatedFrame(kFrames, 4, 0.5f) == std::vector<float>(4, 1.0f)); // exactly frame 1
    const auto quarter = interpolatedFrame(kFrames, 4, 0.25f);                   // halfway frame 0 and 1
    const auto threeQuarters = interpolatedFrame(kFrames, 4, 0.75f);             // halfway frame 1 and 2
    for (size_t i = 0; i < 4; ++i)
    {
        REQUIRE_THAT(quarter[i], WithinAbs(0.5, 1.0e-6));
        REQUIRE_THAT(threeQuarters[i], WithinAbs(2.5, 1.0e-6));
    }
}

TEST_CASE("A single frame is always the same frame", "[core][wavetable][display]")
{
    const std::vector<float> one{1, 2, 3, 4};
    for (const float position : {0.0f, 0.3f, 1.0f})
        REQUIRE(interpolatedFrame(one, 4, position) == one);
}

TEST_CASE("Unusable positions and inputs are safe", "[core][wavetable][display]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    REQUIRE(interpolatedFrame(kFrames, 4, nan) == std::vector<float>(4, 0.0f));
    REQUIRE(interpolatedFrame(kFrames, 4, -inf) == std::vector<float>(4, 0.0f));
    // Like the oscillator, anything that is not a finite number counts as position 0.
    REQUIRE(interpolatedFrame(kFrames, 4, inf) == std::vector<float>(4, 0.0f));
    REQUIRE(interpolatedFrame(kFrames, 4, -3.0f) == std::vector<float>(4, 0.0f));
    REQUIRE(interpolatedFrame(kFrames, 4, 7.0f) == std::vector<float>(4, 4.0f));

    REQUIRE(interpolatedFrame({}, 4, 0.5f).empty());
    REQUIRE(interpolatedFrame(kFrames, 0, 0.5f).empty());
    REQUIRE(interpolatedFrame(kFrames, 5, 0.5f).empty()); // 12 is not a multiple of 5
}

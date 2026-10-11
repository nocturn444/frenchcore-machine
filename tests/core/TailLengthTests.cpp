// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TailLength.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>

using Catch::Matchers::WithinAbs;
using fcm::core::tailLengthSeconds;

TEST_CASE("Tail length is the release stretched by the time scale plus a margin", "[core][tail]")
{
    REQUIRE_THAT(tailLengthSeconds(0.03f, 1.0f), WithinAbs(0.08, 1.0e-6));
    REQUIRE_THAT(tailLengthSeconds(0.03f, 2.0f), WithinAbs(0.11, 1.0e-6));
}

TEST_CASE("Tail length stays finite and non-negative for any value", "[core][tail]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    for (const float scale : {nan, inf, -inf, -1.0f, 0.0f, 1.0e-9f})
        for (const float release : {nan, inf, -inf, -0.5f, 0.0f, 0.03f})
        {
            const double tail = tailLengthSeconds(release, scale);
            REQUIRE(std::isfinite(tail));
            REQUIRE(tail >= 0.05);
        }
}

// SPDX-License-Identifier: AGPL-3.0-only
#include "core/TempoState.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("TempoState starts at 120 BPM and keeps what it is given", "[core][sync]")
{
    fcm::core::TempoState tempo;
    REQUIRE_THAT(tempo.get(), Catch::Matchers::WithinAbs(120.0, 1.0e-6));

    tempo.set(173.5f);
    REQUIRE_THAT(tempo.get(), Catch::Matchers::WithinAbs(173.5, 1.0e-6));
}

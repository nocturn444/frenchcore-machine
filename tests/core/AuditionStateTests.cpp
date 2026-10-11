// SPDX-License-Identifier: AGPL-3.0-only
#include "core/AuditionState.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using fcm::core::AuditionState;

TEST_CASE("AuditionState starts off at 200 BPM with nothing queued", "[core][audition]")
{
    AuditionState state;
    REQUIRE_FALSE(state.isAutoPlay());
    REQUIRE_THAT(state.getBpm(), Catch::Matchers::WithinAbs(200.0, 1.0e-6));

    fcm::dsp::LiveNote event;
    REQUIRE_FALSE(state.popNote(event));
}

TEST_CASE("AuditionState keeps the tempo inside the supported range", "[core][audition]")
{
    AuditionState state;

    state.setBpm(185.0f);
    REQUIRE_THAT(state.getBpm(), Catch::Matchers::WithinAbs(185.0, 1.0e-6));

    state.setBpm(1.0f);
    REQUIRE_THAT(state.getBpm(), Catch::Matchers::WithinAbs(AuditionState::kMinBpm, 1.0e-6));

    state.setBpm(9999.0f);
    REQUIRE_THAT(state.getBpm(), Catch::Matchers::WithinAbs(AuditionState::kMaxBpm, 1.0e-6));

    state.setAutoPlay(true);
    REQUIRE(state.isAutoPlay());
    state.setAutoPlay(false);
    REQUIRE_FALSE(state.isAutoPlay());
}

TEST_CASE("AuditionState delivers keyboard notes to the audio side in order", "[core][audition]")
{
    AuditionState state;
    REQUIRE(state.sendNote(true, 36, 100));
    REQUIRE(state.sendNote(false, 36, 0));

    fcm::dsp::LiveNote event;
    REQUIRE(state.popNote(event));
    REQUIRE((event.on && event.note == 36 && event.velocity == 100));
    REQUIRE(state.popNote(event));
    REQUIRE((!event.on && event.note == 36));
    REQUIRE_FALSE(state.popNote(event));
}

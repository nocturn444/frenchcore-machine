// SPDX-License-Identifier: AGPL-3.0-only
#include "core/AboutText.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The licence notice names the version, the licence and the source", "[core][about]")
{
    const juce::String version(fcm::core::kVersion);
    // The build passes the CMake project version, which is always three numbers.
    REQUIRE(juce::StringArray::fromTokens(version, ".", "").size() == 3);
    REQUIRE(version.containsOnly("0123456789."));

    const auto notice = fcm::core::legalNotice();
    REQUIRE(notice.contains("Frenchcore Machine " + version));
    REQUIRE(notice.contains("AGPL-3.0"));
    REQUIRE(notice.contains("no warranty"));
    REQUIRE(notice.contains("github.com/nocturn444/frenchcore-machine"));
}

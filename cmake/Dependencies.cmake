# SPDX-License-Identifier: AGPL-3.0-only
# Third-party dependencies, pinned to exact tags. Fetched with git (not archive URLs) because the
# cloud agent environment blocks GitHub archive downloads. Record every dependency and its licence
# in docs/ARCHITECTURE.md.

CPMAddPackage(
    NAME JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW ON
)

if(FCM_BUILD_TESTS)
    CPMAddPackage(
        NAME Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.16.1
        GIT_SHALLOW ON
    )
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

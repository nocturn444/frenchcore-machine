# SPDX-License-Identifier: AGPL-3.0-only
# Third-party dependencies, pinned to the full commit SHA of a release tag (the tag is in the
# comment). A tag can be moved; a commit SHA cannot. Fetched with git (not archive URLs) because the
# cloud agent environment blocks GitHub archive downloads. Record every dependency and its licence
# in docs/ARCHITECTURE.md.

CPMAddPackage(
    NAME JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 3fb5cbc0340758596dac39f7c8de47d562b4896e # 9.0.4
    # Shallow clones by SHA are not reliable on every git server, so fetch the history.
    GIT_SHALLOW OFF
)

if(FCM_BUILD_TESTS)
    CPMAddPackage(
        NAME Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG 08092139210881f01a34e60c5f2fd6afa0ea4024 # v3.16.1
        GIT_SHALLOW OFF
    )
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

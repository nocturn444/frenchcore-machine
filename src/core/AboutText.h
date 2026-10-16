// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <juce_core/juce_core.h>

// FCM_VERSION comes from the CMake project version (src/core/CMakeLists.txt).
#ifndef FCM_VERSION
#error "FCM_VERSION must be defined by the build"
#endif

namespace fcm::core
{

inline constexpr const char* kVersion = FCM_VERSION;
inline constexpr const char* kSourceUrl = "github.com/nocturn444/frenchcore-machine";

// The licence notice the editor shows (docs/specs/legal-notice.md). AGPL-3.0 section 0 asks an
// interactive program to show an appropriate legal notice: the licence, no warranty, and where the
// source is.
inline juce::String legalNotice()
{
    return juce::String("Frenchcore Machine ") + kVersion +
           "\nFree software, AGPL-3.0, no warranty\nSource: " + kSourceUrl;
}

} // namespace fcm::core

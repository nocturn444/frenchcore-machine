# SPDX-License-Identifier: AGPL-3.0-only
#
# Our code is split into four libraries with one-way dependencies (see docs/ARCHITECTURE.md §3):
#
#   plugin -> ui -> core -> dsp
#
# They are INTERFACE libraries: each final binary (plugin, tests) compiles their sources together
# with the JUCE modules it links. JUCE modules must be linked PRIVATE by the final target, so this
# avoids compiling JUCE twice into one binary. The dsp boundary is enforced by the dsp test binary,
# which only links juce_dsp: a dsp file that includes UI or plugin headers fails to build there.

include(${PROJECT_SOURCE_DIR}/cmake/FcmJuceOptions.cmake)

add_subdirectory(dsp)
add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(plugin)

# SPDX-License-Identifier: AGPL-3.0-only
add_library(fcm_ui INTERFACE)
target_sources(fcm_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/AuditionPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopeEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EnvelopePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GenericParameterPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WavetablePanel.cpp
)
target_link_libraries(fcm_ui INTERFACE fcm_core)

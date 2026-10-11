# 0001: JUCE 9 with CMake and CPM

- Status: Accepted
- Date: 2026-10-11

## Context

Frenchcore Machine must ship as VST3, AU and Standalone on Windows, macOS and Linux, with a custom UI (envelope editors, drag-and-drop FX rack) and real-time DSP. The team is small and contributors may be AI agents working in cloud containers, so the build must work from a clean checkout with one command.

Alternatives considered: iPlug2 (MIT, smaller ecosystem and community), DPF (Linux-centric, LV2/CLAP focus), raw VST3 SDK (too much plumbing), nih-plug in Rust (no AU support, smaller pool of contributors).

## Decision

Use JUCE 9 with C++20, built with CMake. Fetch JUCE (pinned to tag `9.0.3` at the time of writing) and Catch2 through CPM.cmake. Dependencies are fetched with `GIT_REPOSITORY` + `GIT_TAG`, not archive URLs, because the cloud agent environment allows git over HTTPS but blocks GitHub archive downloads. No git submodules and no Projucer.

## Consequences

- One code base produces all formats and platforms.
- Large community, well-known patterns (APVTS, `juce_dsp`), easy to find help.
- JUCE's free license requires the project to be AGPLv3 (see ADR 0002).
- First configure downloads JUCE (cached afterwards).

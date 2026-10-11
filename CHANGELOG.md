# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Initial project documentation: plan, architecture, MVP scope, ADRs, spec template, contributing guide, bilingual README.
- AGPL-3.0 license.
- Agent instructions (`CLAUDE.md`), GitHub issue and PR templates, and a setup hook for Claude Code cloud sessions.
- Build system (CMake + CPM, JUCE 9.0.3, Catch2) producing VST3, AU and Standalone targets.
- Plugin skeleton: silent engine, output gain parameter, resizable editor showing the agreed layout grid.
- CI on Linux, macOS and Windows: build, unit tests, pluginval at strictness 10, clang-format check, downloadable builds.

[Unreleased]: https://github.com/nocturn444/frenchcore-machine/commits/main

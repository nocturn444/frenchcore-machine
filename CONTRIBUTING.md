# Contributing to Frenchcore Machine

Thanks for helping. This project is documentation-driven: shared written understanding comes before code. Please read this whole file before opening a pull request.

## Ways to contribute

- **Report a bug** with the bug report template (DAW, OS, plugin format and version, steps to reproduce).
- **Suggest a feature** with the feature request template. Explain the sound or workflow problem, not only the solution.
- **Contribute sounds:** factory wavetables and presets. They must be your own work, or explicitly licensed for redistribution under AGPLv3-compatible terms.
- **Write code or docs** following the workflow below.

## Workflow

1. **Find or open an issue** describing the change.
2. **Spec first.** For any new feature, add a spec in `docs/specs/` from the [template](docs/specs/_TEMPLATE.md). For architectural changes, update [ARCHITECTURE.md](docs/ARCHITECTURE.md) and add an ADR in `docs/decisions/`.
3. **Branch** from `main`: `feat/<short-name>`, `fix/<short-name>`, `docs/<short-name>` or `chore/<short-name>`.
4. **Tests, then code.** Write the tests the spec lists, then implement.
5. **Update docs** in the same PR: user guide, `CHANGELOG.md` (under `Unreleased`) and `docs/PLAN.md` status.
6. **Open a pull request** with the template and make sure CI is green.

One feature or fix per pull request. Small PRs get reviewed faster.

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/): `feat: add foldback distortion`, `fix(envelope): prevent click on retrigger`, `docs: describe glide`. Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `build`, `ci`, `chore`.

## Code style

- C++20, formatted with the repository's `.clang-format`. Run `clang-format -i` on changed files.
- Every source file starts with `// SPDX-License-Identifier: AGPL-3.0-only`.
- Names: `PascalCase` for types, `camelCase` for functions and variables, `kPascalCase` for constants, files named after their main class.
- Namespace everything under `fcm` (`fcm::dsp`, `fcm::core`, `fcm::ui`).
- Respect the library boundaries in [ARCHITECTURE.md §3](docs/ARCHITECTURE.md#3-repository-layout): DSP never includes UI or plugin headers.

## Real-time rules (non-negotiable)

Code that runs on the audio thread (`processBlock`, `Effect::process`, oscillator and envelope rendering) must never:

- allocate or free memory (no `new`, `std::vector::push_back`, `std::string` building, `std::function` creation);
- lock a mutex or wait on anything;
- do file or network I/O, or log;
- call into UI code.

See [ARCHITECTURE.md §6](docs/ARCHITECTURE.md#6-threading-and-real-time-rules) for the patterns to use instead.

## Parameters and compatibility

- Parameter IDs are permanent once released. Never rename or reuse them.
- State format changes require a version bump and a migration.
- Adding an effect follows [ADDING_AN_EFFECT.md](docs/ADDING_AN_EFFECT.md).

## Versioning and releases

[Semantic Versioning](https://semver.org): `0.x` during early development. A release moves the `Unreleased` section of `CHANGELOG.md` under the new version and is tagged `vX.Y.Z`; CI attaches the binaries.

## AI-assisted contributions

AI coding agents are welcome contributors and follow the same rules. Agent-specific instructions are in [CLAUDE.md](CLAUDE.md).

## License

By contributing you agree that your contribution is licensed under the [AGPL-3.0](LICENSE).

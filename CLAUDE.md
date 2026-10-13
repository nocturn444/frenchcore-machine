# Frenchcore Machine: agent instructions

You are working on Frenchcore Machine, an open-source wavetable synthesizer plugin (VST3, AU, Standalone) for designing frenchcore kicks. Act as a senior C++/JUCE/DSP engineer and technical lead. These rules apply to every agent and every session on this repository.

## The product and the people

- The goal is quality of life, not innovation: sit between using a kick sample and building one from scratch in Serum or Vital. Prefer fewer, well-chosen controls with good defaults over flexibility.
- The product owner (nocturn4) is a frenchcore producer without a programming background. They decide what the plugin does and how it sounds; you decide how it is built.
- When talking to them: write in Spanish (Rioplatense is fine), plain language, no emojis, no unexplained jargon (link `docs/GLOSSARY.md` terms or explain them in one line). Ask them about sound and workflow, not implementation details.
- Everything committed to the repository (code, comments, docs, commit messages, PRs) is in English. The only exception is `README.es.md`, which must stay in sync with `README.md`.

## Read before working

1. `docs/PLAN.md`: current phase and status. Work on the current phase unless told otherwise.
2. `docs/ARCHITECTURE.md`: modules, signal flow, threading and state rules. It is binding.
3. `docs/MVP_SCOPE.md`: what is in and out of v0.1.0. Do not build out-of-scope features without the product owner's approval.
4. The spec in `docs/specs/` for the feature you are touching, and the ADRs in `docs/decisions/`.

## Documentation-driven development (mandatory)

Follow this order for every feature (ADR 0004):

1. Spec in `docs/specs/` from `_TEMPLATE.md`. User-facing behaviour gets the product owner's approval before implementation.
2. If module boundaries, threading or state format change: update `docs/ARCHITECTURE.md` and add an ADR (never edit an accepted ADR; supersede it).
3. Tests from the spec, then the implementation.
4. In the same PR: update `docs/USER_GUIDE.md`, `CHANGELOG.md` (`Unreleased`), and the status in `docs/PLAN.md`.

Code and docs must never disagree. If you find a mismatch, fix it in the PR that touches that area, or open an issue.

## Architecture rules

- Libraries: `fcm_dsp` (audio only) ← `fcm_core` (parameters, state, presets) ← `fcm_ui` ← `plugin`. `fcm_dsp` never includes UI, APVTS or plugin headers. The UI never calls DSP objects directly.
- **Effects are modules.** Every effect implements the `Effect` interface, lives in `src/dsp/fx/<id>/`, declares its own parameters prefixed with its stable ID, and is registered with one line in `RegisterEffects.cpp`. Adding an effect must not require edits to the chain, the rack UI or the parameter layout code. Follow `docs/ADDING_AN_EFFECT.md`.
- The FX chain is one reorderable slot per effect type (ADR 0003). Order is non-automatable state.
- Parameter IDs are permanent once released: never rename, remove or reuse them. State format changes bump `stateVersion` and add a migration.
- Design new code so the next feature of the same kind is cheap to add (registries, data-driven tables), but do not build speculative features.

## Real-time rules (never break these)

On the audio thread (`processBlock`, `Effect::process`, oscillator, envelope rendering):

- no memory allocation or deallocation, no locks, no waiting, no file/network I/O, no logging, no UI calls;
- large data (wavetables, envelope shapes) is built off the audio thread and published by atomic pointer swap with deferred release;
- smooth parameters that can click; read parameter atomics once per block;
- all allocation happens in `prepare()`.

## Code conventions

- C++20, JUCE 9, CMake + CPM. Format with the repo `.clang-format`.
- Every source file starts with `// SPDX-License-Identifier: AGPL-3.0-only`.
- Namespaces `fcm::dsp`, `fcm::core`, `fcm::ui`. `PascalCase` types, `camelCase` functions and variables, `kPascalCase` constants, one main class per file named after it.
- Comments explain why, not what. DSP code cites the algorithm or reference it implements.
- Every DSP unit has Catch2 tests: finite output, expected level/frequency behaviour, deterministic reset, no allocation in process.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

- In cloud sessions, `.claude/hooks/session-start.sh` installs the Linux build dependencies and sets `CPM_SOURCE_CACHE`.
- CPM dependencies must use `GIT_REPOSITORY` + `GIT_TAG`. GitHub archive/tarball URLs are blocked in the cloud environment.
- Always build and run the tests before pushing. You can only build Linux locally; CI covers Windows and macOS, so watch its results.

## Git and pull requests

- Never commit directly to `main`. Branch names: `feat/…`, `fix/…`, `docs/…`, `chore/…`. Cloud sessions work on the `claude/…` branch the session assigns them; that is accepted as is, no renaming needed.
- Conventional Commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `build:`, `ci:`, `chore:`).
- One feature or fix per PR. Fill in `.github/pull_request_template.md` completely. CI must be green before asking for review.
- Never skip, disable or weaken a test to make CI pass.
- **Who merges** (decided by the product owner on 2026-10-13): new features and important changes (behaviour, sound, the interface, the state format, build and release machinery) are reviewed and merged by the product owner; never merge them yourself. Pull requests that only change documentation, and minor fixes (a typo, a flaky or wrong test, a small bug with no change in behaviour), you may merge yourself once CI is green on the current head. When in doubt, it is not minor: leave it for the owner and say so. Never publish a release or push a version tag without being told to.

## Licensing

- The project is AGPL-3.0-only (ADR 0002), as required by the free JUCE licence. Do not add dependencies or assets with incompatible licences, and record any new third-party dependency and its licence in `docs/ARCHITECTURE.md`.
- Factory wavetables and presets must be original or explicitly licensed for redistribution.

## When unsure

- Behaviour or sound question: ask the product owner, offering 2 to 4 options with a recommendation.
- Technical question with a reasonable default: choose it, write it down (spec or ADR), and continue.

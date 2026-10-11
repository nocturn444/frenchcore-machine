# Status

Living summary for the product owner (nocturn4). Updated by the agent in every PR that changes something listed here. It is in English like the rest of the repository; the agent explains the important parts in Spanish in the chat.

Last updated: 2026-10-11.

## 1. Where the project is

| Phase | State |
|---|---|
| 0 Foundation, 1 Skeleton, 2 First sound | Done and merged |
| 3 Envelopes (editable amp/pitch, Punch/Tail, tempo sync, level knobs) | Done and merged |
| Security hardening (safe state loading, CI and supply chain, Standalone MIDI) | Done and merged |
| 4 Wavetables | Next, see section 2 |
| 5 FX system, 6 State and presets, 7 Hardening and release | Not started |

## 2. Open pull requests, in merge order

Nothing in this list is merged by the agent; the owner merges.

| Order | PR | What | CI |
|---|---|---|---|
| 1 | Dependabot #10, #9, #8 | Version bumps of `actions/checkout`, `actions/cache`, `actions/upload-artifact` (major versions) | Green |

Notes on the Dependabot pull requests: they are independent of each other and of the code, they only touch `.github/workflows/build.yml`. They are major version jumps; CI passed with them on all three systems. Merge them one by one (Dependabot rebases the others), or close them if you prefer to stay on the older pinned versions.

## 3. Decisions taken by the agent

Decisions that were not the owner's to make (technical, or a reasonable default). Anything about sound is listed in section 4 instead.

| Date | Decision | Where it is written |
|---|---|---|
| 2026-10-11 | A non-finite or out-of-range value in a saved project becomes the parameter's default (or the nearest limit). Loading never fails because of one bad value. | `docs/specs/state-safe-loading.md` |
| 2026-10-11 | A state saved by a newer version loads what this version knows and ignores the rest. | `docs/specs/state-safe-loading.md` |
| 2026-10-11 | Standalone MIDI copy is capped; note-offs and All Notes Off still get through when the cap is hit. | `docs/specs/standalone-audition.md` |
| 2026-10-11 | CI actions, JUCE and Catch2 are pinned by commit SHA; pluginval is checked by SHA-256. | `docs/ARCHITECTURE.md` section 9 |
| 2026-10-11 | `CODEOWNERS` makes the owner the reviewer of everything. Takes effect once branch protection requires code owner review. | `.github/CODEOWNERS` |
| 2026-10-11 | The Punch/Tail Split stays in milliseconds when tempo Sync is on (owner's answer). | `docs/specs/voice-punch-tail.md` |

## 4. What the owner needs to do or listen to

Things only the owner can do in GitHub (the agent cannot):

- Protect `main` (Settings, Branches): require the CI checks and a pull request before merging, and optionally "Require review from Code Owners".
- Enable "Private vulnerability reporting" (Settings, Security), because `SECURITY.md` sends reporters there.
- Check Settings, Actions, General: workflow permissions on "Read repository contents".

Things to listen to (sound):

- The Phase 3 envelopes in a DAW or the Standalone: the default "Kick + Bass" amp shape, the pitch drop, the Punch/Tail balance and the Punch and Tail level knobs. Tell the agent what feels wrong in plain words.

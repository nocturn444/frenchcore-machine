# 0003: FX chain: one reorderable slot per effect type

- Status: Accepted
- Date: 2026-10-11

## Context

The product owner wants a Serum-style effects rack the user can reorder, and the codebase must make adding effects cheap. Host automation and presets refer to parameters by fixed IDs, so a chain with arbitrary dynamic slots (e.g. "slot 3 drive") would change meaning when the user reorders, and would break saved automation.

Options:

1. Dynamic slots: any effect in any slot, duplicates allowed. Maximum flexibility; automation follows the slot, not the effect, which confuses users and breaks projects on reorder.
2. One fixed slot per effect type, reorderable (Serum model). Parameters belong to the effect, so automation follows the effect wherever it moves.

## Decision

Use option 2. Every registered effect exists exactly once in the chain. The user can enable/bypass it, set its mix and drag it to any position. The order is non-automatable state stored in the plugin state.

Effects are added through an `EffectRegistry`; each effect declares its own parameters with IDs prefixed by its stable effect ID.

## Consequences

- Automation and presets stay valid across reorders and across versions that add effects.
- No duplicates of the same effect in the MVP (e.g. two distortions). If needed later, an effect can be registered twice under two IDs (`distortion`, `distortion2`), or a new ADR can introduce dynamic slots.
- Adding an effect never touches the chain or the UI rack code.

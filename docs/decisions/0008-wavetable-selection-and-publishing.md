# 0008: Wavetables are chosen by a parameter and published to each oscillator

- Status: Accepted
- Date: 2026-10-11

## Context

Phase 4 lets each oscillator play a factory wavetable chosen by the user, and later a wavetable loaded from a file ([wavetable-factory](../specs/wavetable-factory.md)). A band-limited table is big (about 0.7 to 1.5 MB for the factory ones, up to 23 MB for 256 frames) and takes milliseconds to build, so it cannot be built on the audio thread. The choice must be automatable and saved with the project like any other control, while a user-loaded table is state that is not a parameter.

Options:

1. Build every factory table once and let the audio thread pick one by index. No publishing is needed for factory tables, but user tables would still need the pointer-swap machinery, so there would be two mechanisms, and all tables would sit in memory whether used or not.
2. One choice parameter per oscillator, and one `PublishedPointer<WavetableData>` per oscillator (the pattern of [ADR 0006](0006-envelope-model-and-publishing.md)). A message-thread selector watches the parameter, builds the table, publishes it. Factory and user tables take the same path.

## Decision

Use option 2.

- `oscN.wavetable` is a choice parameter. Its entries are only ever appended (the saved value is the entry number, so reordering or removing would change old projects).
- `core::WavetableSelector` (one per oscillator) listens to the parameter from any thread and, on the message thread, builds the table off the audio thread and calls `publish`. The old table is freed by the publisher once the audio thread has moved on, exactly as for envelope shapes.
- `Engine` acquires both tables once per block and passes them to the voice. Until something is published (or when a table is missing) the engine plays its built-in sine.
- The voice sets the new table on the sounding kick **and on every fading copy of the previous kick** at the start of the block. A fading copy therefore never holds a table older than the one the audio thread has acquired this block, which is the only one the hazard pointer protects. The cost is that a table change in the middle of a kick changes the timbre of its last 1.5 ms of fade-out, which is inaudible.

## Consequences

- Switching tables while a kick sounds changes the waveform at the next block boundary. That can click; it is a deliberate edit, not an effect, and it is accepted.
- The choice list is append-only. A test guards the factory names and their order.
- Memory holds one table per oscillator plus at most one retired table in use.
- The same selector will carry user-loaded wavetables, with their decoding done on a background thread.

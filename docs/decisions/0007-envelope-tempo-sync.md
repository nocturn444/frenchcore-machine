# 0007: Envelope tempo sync stores times at a reference tempo

- Status: Accepted
- Date: 2026-10-11

## Context

The product owner asked for envelope times that can follow the tempo, shown as musical figures (1/16, 1/8, ...) instead of milliseconds ([envelope-breakpoint](../specs/envelope-breakpoint.md)). A synced envelope has to keep its musical lengths when the tempo changes (a host changing tempo, or the Standalone BPM control). The envelope model, the editor and the saved state all work in milliseconds, and the audio thread must stay free of allocation and locks.

Options:

1. Store times in beats when synced and in milliseconds otherwise. Every bound, knob range and editor conversion then depends on the unit, and switching mode converts the whole shape.
2. Keep times in milliseconds always and add a quantizing helper: the knobs pick a figure and write milliseconds for the tempo at that moment. Simple, but the envelope no longer follows later tempo changes, which is what "synced" means.
3. Keep times in milliseconds, but remember the tempo they were written at (the **reference tempo**). When synced, every time is played scaled by `reference / current tempo`.

## Decision

Use option 3. The model gets `syncRefBpm` (0 means not synced). The published `EnvelopeShape` carries it. At each note-on the voice multiplies the envelope's time scale by `syncRefBpm / bpm` (the current tempo comes in with the other engine parameters), so a shape written at 120 BPM lasts half as long at 240 BPM. Nothing else in the renderer changes.

Edits made while synced first **rebase** the shape to the current tempo (times multiplied by `reference / current`, then the reference becomes the current tempo), so the musical lengths are preserved and the knobs write exact figures for the current tempo. Turning sync off rebases once and drops the reference, so the sound at that moment is unchanged; turning it on just records the current tempo. The reference is saved with the shape as an optional property.

The tempo is the host tempo in a plugin, the Auto-play BPM in the Standalone, and 120 when the host gives none. The processor stores it in a small atomic that the editor also reads.

## Consequences

- No unit change in the model, the knobs ranges, the editor or the published shape, only an extra number.
- The synced envelope follows tempo changes from the next note, like the time scale (read at note-on), never in the middle of a kick.
- Very low tempos can stretch a synced shape past 2000 ms (the cap applies to the authored times); that is accepted.

# 0005: The engine receives parameters as a plain struct

- Status: Accepted
- Date: 2026-10-11

## Context

`fcm_dsp` must not know about APVTS, parameter IDs or the plugin wrapper (ARCHITECTURE §3), so it can be unit-tested headless. But the engine needs the current value of every parameter on the audio thread, once per block (ARCHITECTURE §6). Something has to cross the boundary between `fcm_core` (which owns the parameters) and `fcm_dsp`.

Options:

1. Pass `std::atomic<float>*` pointers into the engine. Cheap, but the engine then depends on how parameters are stored, and tests need to create atomics for every value.
2. Let the engine include `juce_audio_processors` and read the APVTS. Breaks the dsp boundary.
3. A plain struct (`fcm::dsp::EngineParameters`) owned by `fcm_dsp`, filled by `fcm_core` once per block and passed to `Engine::setParameters`.

## Decision

Use option 3. `EngineParameters` holds already-converted, DSP-friendly values (linear gains, integer notes, booleans). `fcm_core` provides `EngineParameterReader`, which caches the raw parameter pointers at construction and builds an `EngineParameters` from them with no allocation. The reader is the only place that maps parameter IDs to DSP fields.

Effects (Phase 5) follow the same idea with their own parameter struct, supplied by the effect's `createParameters()` side; this ADR covers the voice and global parameters only.

## Consequences

- The dsp tests build an `EngineParameters` directly; no JUCE parameter machinery is needed to test DSP.
- Adding a parameter means: add the field, add it to the layout, read it in the reader. A core test checks the defaults round-trip, so a mismatch between layout and reader is caught.
- The struct is copied once per block (a few dozen bytes). Negligible.
- Unit conversions (dB to linear, degrees to normalized phase) live in the reader, not in the DSP.

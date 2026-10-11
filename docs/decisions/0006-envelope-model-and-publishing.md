# 0006: Envelope model, and publishing shapes to the audio thread

- Status: Accepted
- Date: 2026-10-11

## Context

The envelopes become editable in Phase 3 ([envelope-breakpoint](../specs/envelope-breakpoint.md)). The user edits a shape on the message thread, possibly dozens of times per second while dragging a point, and the audio thread must always read a complete, consistent shape without waiting, allocating or freeing. ARCHITECTURE §6 says to publish large immutable data by atomic pointer swap with deferred release, but not how to know when the old object is no longer in use. The wavetables of Phase 4 need the same mechanism.

Options for the deferred release:

1. Free the old object after a fixed delay. Unsafe if the audio thread stalls, and wasteful otherwise.
2. A block counter: free once the audio thread has started a later block. Wrong when the host stops calling the audio thread (objects pile up) and it needs the audio thread to cooperate with a counter.
3. A hazard pointer: the audio thread records the pointer it is using; the message thread frees a retired object only if it is not that pointer.
4. A fixed pool of preallocated slots with an in-use marker. No heap at all, but it caps the object size and count and does not suit the larger wavetables.

## Decision

Use option 3 in a small generic class, `fcm::dsp::PublishedPointer<T>`:

- `publish(std::unique_ptr<const T>)` (message side, serialised by a mutex that the audio thread never takes) exchanges the current pointer atomically, queues the old one as retired, and frees every retired object that is not the audio thread's current hazard.
- `acquire()` (audio side, once per block) loads the current pointer, stores it as the hazard, and re-checks that it is still current, retrying otherwise. It is lock-free and does not allocate.
- Destruction frees everything.

The editable shape is a separate value type, `BreakpointEnvelope`, which the UI may use (it holds data, not audio objects); the audio thread only ever sees the immutable `EnvelopeShape` built from it. `fcm_core::EnvelopeState` owns both the editable model and the publisher, and serialises the model to the saved state. At most the object the audio thread is using plus the current one are alive.

Envelope shapes are stored as child nodes `AmpEnvelope` and `PitchEnvelope` of the state, as already planned in ARCHITECTURE §5, with missing nodes meaning the default shape. As nothing has been released yet and the nodes are optional, `stateVersion` stays at 1.

## Consequences

- No allocation, free or lock on the audio thread, with a bounded number of live objects, whether or not the host keeps calling `processBlock`.
- Wavetables (Phase 4) reuse `PublishedPointer`.
- The audio thread keeps the last shape alive until its next block, which is harmless.
- The hazard pointer assumes a single audio thread reading a given publisher, which is how the engine uses it.

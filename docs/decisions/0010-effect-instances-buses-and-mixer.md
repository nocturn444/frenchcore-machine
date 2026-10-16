# 0010: Effect instances from a fixed pool, three racks (Bus A, Bus B, Main) and a source mixer

- Status: Accepted
- Date: 2026-10-16
- Supersedes: [0003](0003-fx-chain-model.md) (one slot per effect type). Keeps [0009](0009-effect-parameters-and-chain-latency.md) (parameters as plain data, constant latency), extending its latency rule to parallel racks.

## Context

For version 0.2 the product owner asked for a Serum 2 style mixer (answers of 2026-10-16, recorded in [mixer-and-buses](../specs/mixer-and-buses.md)):

- the same effect can be added several times (up to **4 copies of each effect type**), effects can be added, removed and reordered;
- each sound source (Osc 1, Osc 2, Noise, Tock) is sent to **Bus A**, **Bus B** or **Main**; the two buses have their own effects and end in Main, which has its own effects;
- a bus can apply its effects to the whole kick, only to the **Punch** or only to the **Tail** (the existing Split time). The whole signal enters the bus; outside the chosen part the bus passes its input through clean;
- every 0.1 project and preset must open and sound the same.

ADR 0003 rejected dynamic slots because automation and presets refer to fixed parameter IDs, and a parameter like "slot 3 drive" changes meaning when the user reorders. That reason still holds: a DAW identifies a parameter by a fixed ID and a fixed normalised range, and the set of parameters cannot change while a project is open.

## Decision

1. **A fixed pool of instances.** Every registered effect type has `kInstancesPerEffect = 4` instances that always exist as parameters. Instance 1 keeps the 0.1 IDs (`distortion.drive`); instances 2 to 4 use the effect ID with the instance number appended (`distortion2.drive`, `distortion3.drive`, `distortion4.drive`), display names "Distortion 2 Drive" and so on. The common controls follow the same rule (`distortion2.enabled`, `distortion2.mix`). An instance is identified everywhere by its **instance ID** (`distortion`, `distortion2`, ...). Automation follows an instance wherever it is moved.
2. **Three racks.** `busA`, `busB` and `main` each hold an ordered list of up to **8** instance IDs. An instance is in at most one rack. "Adding" an effect to a rack takes the lowest-numbered free instance of that type, resets its parameters to their defaults (message thread, through the parameters, so the host sees it) and switches it on; "removing" it takes it out of its rack and switches it off. Duplicating a strip takes a free instance and copies the parameter values. The rack contents are **state, not parameters** (a `FxRacks` node), like the 0.1 order.
3. **Publishing the racks to the audio thread.** Each rack is one `std::atomic<uint64_t>`: a 4-bit count and up to 8 instance indices of 6 bits (pool index = effect index * 4 + instance - 1, at most 64 instances). The message thread writes, the audio thread reads each rack once per block. A packed value with an index out of range, a repeated index, or an instance in two racks makes the audio thread treat the rack as empty for that block (it can only happen with a bug; the state loader never produces one).
4. **The source mixer.** The voice renders four mono stems (Osc 1, Osc 2, Noise, Tock) instead of one sum, each already through its own level, the amp envelope where it applies and the Punch/Tail levels. Each stem has a destination parameter (`mixer.<source>.dest`: Main, Bus A, Bus B), Mute and Solo. The stems are summed per destination into three mono signals, copied to two channels.
5. **Bus region.** Each bus has `bus.<a|b>.region` (Whole, Punch, Tail) and `bus.<a|b>.level`. With Punch or Tail, the bus output is `clean * (1 - w) + processed * w`, where `w` is the same 4 ms smoothstep at the Split time that the Punch/Tail levels use, counted from each kick's note-on, delayed by the rack's latency so it lines up with the processed signal. The clean signal is the bus input delayed by the same latency.
6. **Constant latency with parallel racks.** Let `P(q)` be the sum of `latencySamples(q)` over every instance in the pool. Every rack pads its own latency up to `P(q)` (a delay line after the rack), so a rack has the same delay whatever it holds. Sources sent straight to Main are delayed by `P(q)` too, so they line up with the bus outputs. The plugin reports `2 * P(q)` to the host. It only changes when Quality changes. Inside a rack, the per-slot dry compensation of ADR 0009 stays.
7. **Signal flow per block:** voice stems → mixer (dest, mute, solo) → Bus A rack, Bus B rack (each with region and level) and the direct-to-Main delay → sum → Main rack → output gain → safety limiter.
8. **State version 2.** `kStateVersion` becomes 2. The migration from version 1: the 0.1 `FxChain` order (all three effects, in their saved order, whether on or off) becomes the Main rack; Bus A and Bus B are empty; every source goes to Main; any new parameter whose default would change a 0.1 sound is set to its 0.1 behaviour (today only `tock.ampEnv` = on, see [tock](../specs/tock.md)). A 0.1 preset file goes through the same migration. The `FxChain` node is no longer written.
9. **Defaults for a fresh instance** (and the Init preset): Main holds Distortion, EQ and Stereo, off, as in 0.1, so the window looks familiar; Bus A and Bus B are empty.

## Consequences

- Automation and presets stay valid across reorders, across racks and across versions that add effect types, as with ADR 0003.
- The parameter list grows: every effect type costs four times its parameters even when unused (about 500 parameters in total for seven effect types). DAWs handle this, but their automation menus get long; the instance number in the name keeps it readable.
- Adding an effect type still needs no change in the chain, the racks, the layout code or the rack UI: the pool, the IDs and the menus are built from the registry.
- `kMaxEffects` (16 effect types) stays; the old 4-bit packed order is replaced by the per-rack packing above.
- A copy of an effect that has latency costs that latency in every rack (the padding), even if the copy is not used. With the current effects only Distortion has latency (a few samples at 2x), so the total stays small.
- More stems and three racks cost CPU only for what is used: an empty rack is a delay line, an instance that is off and faded out is skipped as before.

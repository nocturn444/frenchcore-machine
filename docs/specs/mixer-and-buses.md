# Spec: Mixer, effect buses and multiple effect copies

- Status: Approved (product owner's answers of 2026-10-16, questions 2.1 to 2.6); not implemented. Version 0.2, stages 1 and 2 of [PLAN](../PLAN.md).
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADR 0010](../decisions/0010-effect-instances-buses-and-mixer.md), [ADR 0011](../decisions/0011-effect-interface-additions.md), [ADR 0009](../decisions/0009-effect-parameters-and-chain-latency.md), [fx-chain](fx-chain.md) (0.1 machinery, still the base of one rack), [fx-rack](fx-rack.md) (0.1 UI, replaced by section 2 here), [voice-punch-tail](voice-punch-tail.md), [noise-oscillator](noise-oscillator.md), [tock](tock.md)

## 1. What it does for the user

Like the mixer and FX buses of Serum 2, simplified for a kick.

**The mixer.** Every sound source of the kick has a channel: **Osc 1**, **Osc 2**, **Noise** and **Tock**. Each channel has:

- **Level**: the same control as the source's own Level (turning one turns the other).
- **Destination**: **Main**, **Bus A** or **Bus B**. Default Main.
- **Mute** and **Solo**. Solo on any channel silences every channel that is not soloed. Both switch without clicks.

**The buses.** A bus is a row of effects of its own. **Bus A** and **Bus B** end in **Main**; Main has its own row of effects (the one 0.1 had) and goes to the output gain and the safety limiter. Each of Bus A and Bus B has:

- **Level** (-48 to +12 dB, default 0): the volume of what the bus sends to Main.
- **Apply to**: **Whole** (default), **Punch** or **Tail**. With Punch, the bus's effects are heard only in the punch part of the kick, before the Split time of the Punch / Tail box; after it, what entered the bus comes out clean. With Tail, the reverse. The change happens over the same 4 ms as the Punch / Tail levels. All of the sound enters the bus in every case, so a reverb or compressor on a Punch-only bus hears the whole kick, but only its punch part is heard processed; its echo stops at the Split (owner's choice, question 2.3 B).

Examples: send everything to Bus A with Apply to = Tail and put a Distortion there to distort only the tail; send Osc 2 alone to Bus B to EQ only that layer.

**Effects in every rack.**

- **Add** an effect with the "+" button at the end of a rack: a menu of every effect type. It is added at the end, switched on, with its default settings.
- **Up to 4 copies of each effect** in the whole plugin (for example two Distortions on Bus A, one on Main, one free) and **up to 8 effects per rack**. A menu entry is greyed out when its four copies are in use, and "+" is greyed out when the rack is full.
- **Remove** an effect with the "x" on its strip. **Duplicate** it with the "++" on its strip (the copy goes right after it, with the same settings, if a copy is free).
- **Reorder** by dragging the strip's bar, as in 0.1. Dragging works inside a rack; moving an effect to another rack is not offered (remove and add it there).
- The copies are numbered: the second Distortion is "Distortion 2" in the window and in the DAW's automation list ("Distortion 2 Drive"). Automation follows each copy wherever it is in its rack.
- Everything (racks, contents, order, routing) is saved with the project and the presets.

**Old projects.** A project or preset from 0.1 opens with its three effects in Main, in its order and with its settings, every source going to Main, and sounds the same.

## 2. User interface

The Effects box of the window ([ui-grid-layout](ui-grid-layout.md)) keeps its size and place and gets four tabs at the top: **Mixer**, **Bus A**, **Bus B**, **Main**. The window does not grow, so it fits the same screens as 0.1. The last tab used is remembered while the window is open; a new window opens on Main.

**Mixer tab** (566 x about 500 points):

```
 Osc 1      Osc 2      Noise      Tock     |  Bus A       Bus B
 (Level)    (Level)    (Level)    (Level)  |  (Level)     (Level)
 [Main v]   [Bus A v]  [Main v]   [Main v] |  [Whole v]   [Tail v]
 [M] [S]    [M] [S]    [M] [S]    [M] [S]  |
```

Six vertical channel strips: a name, a Level knob, a menu (Destination for the sources, Apply to for the buses) and, for the sources, Mute and Solo buttons. A source that is off (for example Osc 2 by default) is drawn dimmed.

**Bus A, Bus B and Main tabs**: the rack of 0.1 ([fx-rack](fx-rack.md)): one strip per effect in the rack, in order, with the bar (drag handle, name with its copy number, On, Mix, "++" duplicate, "x" remove) and the controls under it. The strips scroll vertically when they do not fit. At the end, a "+ Add effect" button with the menu. An empty rack shows only that button and one line: "Empty: the sound passes through".

Controls with `shownWhen` ([ADR 0011](../decisions/0011-effect-interface-additions.md)) are hidden when they do not apply (for example Bits and Rate unless the Distortion's Type is Bitcrush).

## 3. Parameters

Sources: `osc1`, `osc2`, `noise`, `tock`. Their Level controls are the existing `osc1.level`, `osc2.level`, `tock.level` and the new `noise.level` ([noise-oscillator](noise-oscillator.md)).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `mixer.<source>.dest` | `<Source>` Destination | Main, Bus A, Bus B | Main | | choice | crossfade 10 ms | no |
| `mixer.<source>.mute` | `<Source>` Mute | off / on | off | | | yes (10 ms) | yes |
| `mixer.<source>.solo` | `<Source>` Solo | off / on | off | | | yes (10 ms) | yes |
| `bus.a.level`, `bus.b.level` | Bus A / Bus B Level | -48 to +12, step 0.1 (-48 is silence) | 0 | dB | linear | yes (10 ms) | yes |
| `bus.a.region`, `bus.b.region` | Bus A / Bus B Apply To | Whole, Punch, Tail | Whole | | choice | crossfade 10 ms | yes |

Effect instances ([ADR 0010](../decisions/0010-effect-instances-buses-and-mixer.md)): for every registered effect `<id>` and copy `n` = 2, 3, 4, the instance ID is `<id><n>` and it has `<id><n>.enabled`, `<id><n>.mix` and every own parameter `<id><n>.<suffix>`, with the same ranges and defaults as copy 1 (`<id>.*`, unchanged from 0.1). Display names add the copy number after the effect name ("Distortion 2 Drive").

Choice lists are append-only. Destination is a parameter so presets and undo carry it, but it is not automatable (a host moving it per block would make the routing jump).

Non-parameter state: node `FxRacks` with three children `Rack` (`id` = `busA`, `busB`, `main`; property `effects` = comma-separated instance IDs in order).

## 4. DSP / implementation approach

- **Voice stems.** `Voice` renders four mono stems into a fixed `StemBuffers` (allocated in `prepare`): Osc 1 and Osc 2 (each through its level, the amp envelope and the Punch/Tail gain), Noise (same) and Tock (its level, the Punch/Tail gain, and the amp envelope only when `tock.ampEnv` is on, [tock](tock.md)). The ghost voice of a retrigger adds into the same stems. With every source on Main and no bus used, the sum of the stems equals the 0.1 voice output within float rounding (test below).
- **Region gate.** The voice also writes, per sample, the punch weight `w_punch` (the same smoothstep at the Split that the Punch/Tail levels use, 1 in the punch, 0 in the tail, counted from the newest kick's note-on). Bus region: Whole uses 1, Punch uses `w_punch`, Tail uses `1 - w_punch`.
- **Mixer.** Per sample, each stem times its smoothed mute/solo gain is added to the mono sum of its destination. A destination change crossfades over 10 ms between the old and new sum (both gains smoothed).
- **Racks.** `EffectRack` is the 0.1 `EffectChain` generalised: it owns no effects, it processes a list of instance pointers given by the packed rack value, keeps the per-slot dry compensation and mix of ADR 0009 and pads to the pool latency `P(q)`. `EffectPool` (in `fcm_dsp`) owns every instance (4 per registered type, built and prepared in `prepare`), their smoothed enabled/mix state and the instance IDs. An instance that is in no rack is not processed and is `reset()` when it enters a rack.
- **Bus output** = `level * (clean_delayed * (1 - w) + rack_out * w)`, `w` delayed by `P(q)` to match. Then Bus A + Bus B + Main-direct (delayed by `P(q)`) go into the Main rack, then output gain and limiter as before. Reported latency `2 * P(q)`.
- **Messages and state.** `core::FxRackState` replaces `core::FxOrderState`: add (lowest free copy; resets that instance's parameters to defaults through the APVTS, then switches it on), remove (switches it off), duplicate (copies values), move inside a rack, the lists, the packed values, and save/restore of `FxRacks`. Restore is untrusted input: unknown instance IDs dropped, an instance listed twice kept only the first time, more than 8 entries cut, nothing allocates per block.
- **Migration 1 → 2** in `StateMigrations.cpp` ([ADR 0010](../decisions/0010-effect-instances-buses-and-mixer.md) item 8), also used for 0.1 preset files.
- **Real-time:** no allocation, locks or waits; the packed rack values and parameters are read once per block.

## 5. Tests

- Engine: with all sources on Main and the default racks, a fixed note gives the same audio as 0.1.1 (aligned by the reported latency, every sample within 1e-6 of the reference, which is -120 dBFS: the stems are summed in a different order, so exact bit equality is not required) (record the reference with 0.1.1 before changing the voice); Destination moves a source between racks with no step larger than the signal step; Mute and Solo (one soloed, two soloed, mute on a soloed channel) give the expected stems; region Whole/Punch/Tail apply the effect only where expected (a gain test effect: measured gain before and after the Split, smooth across it); the reported latency is `2 * P(q)` for each Quality and a click put in any source arrives at the output at that delay whatever the routing.
- Racks: add takes the lowest free copy and resets it; a fifth copy is refused; a ninth effect in a rack is refused; remove frees the copy; duplicate copies the values; moving inside a rack changes the order; an invalid packed value processes nothing and never crashes; `process` allocates nothing (counting `operator new`).
- State: round trip of `FxRacks` and the mixer parameters; hostile `FxRacks` nodes (unknown IDs, duplicates, an instance in two racks, 1000 entries, huge text) load safely; migration of every 0.1 factory preset and of a saved 0.1.1 project gives the same audio as 0.1.1 within the same tolerance (reference renders committed as test data, a few kilobytes each).
- Layout: the IDs of copies 2 to 4 follow the rule; copy 1 IDs are unchanged; the released-ID guard gains the new IDs.
- Manual (Standalone): the mixer tab, adding/removing/duplicating effects, dragging, a tail-only distortion on Bus A, a soloed Osc 2.

## 6. Out of scope

- More than two buses, sends (a source to two places at once), panning, a meter per channel.
- Moving an effect between racks by dragging.
- A "Direct" destination that skips Main's effects.
- A separate Split per bus (one Split for everything, question 2.4 A).

## 7. Open questions

None.

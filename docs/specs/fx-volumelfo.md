# Spec: Volume LFO effect

- Status: Approved (product owner's answers of 2026-10-20, questions G.1 A, G.2 A, 1.1 to 1.4); not implemented yet (version 0.3, stage 6). Default shape, ranges and defaults are first choices by the agent, for the owner's ears.
- Owner: nocturn4 (product), implemented by agent session
- Related: [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md), [ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md), [envelope-breakpoint](envelope-breakpoint.md), [mixer-and-buses](mixer-and-buses.md)

## 1. What it does for the user

A volume shaper in the style of LFOTool: it turns the volume up and down following a **shape you draw**, repeated at a speed that usually follows the tempo.

Why (the owner's words, 2026-10-20): after a lot of distortion and reverb the kick becomes one uniform mass and loses its syncopation. Put the Volume LFO **after** the distortion and the reverb (in Main, or on a bus) and it cuts the mass back into rhythm: the main use is **pumping** like a sidechain, and it also works for creative gating and chopping.

- **The shape**: one cycle drawn with points, like the envelopes. A menu loads ready-made shapes (see §2). The level goes from silence (bottom) to the full sound (top).
- **Rate** and **Sync**: how long one cycle lasts. With Sync on (the default) it is a note value from 1/64 to 1 bar that follows the tempo; with Sync off it is in Hz (cycles per second).
- **Depth**: how much of the shape is applied. 100 % is the full shape; 0 % does nothing; 50 % only goes down to half volume.
- **Smooth**: rounds off sudden jumps in the shape so they do not click. At 0 % the edges are as sharp as possible (still never a click: there is always a ramp of at least 0.2 ms).

**The cycle starts from the beginning at every note** (the owner's answer G.1 A), so every kick gets the same shape, and the cycle keeps repeating until the next note. It has the common **On** and **Mix** controls (Mix starts at 100 %). It adds no delay. With a bus set to Apply To Tail ([mixer-and-buses](mixer-and-buses.md)) it acts only on the tail.

Note: this is an effect on the volume, not the future modulation system (the LFOs that will move other controls, [PLAN](../PLAN.md) "After 0.3").

## 2. User interface

**In the strip** (any rack): a picture of the shape (a decoration about 60 points high, the full width of the strip), then the knobs Rate (or the note value when Sync is on), Depth, Smooth and the switch Sync. The picture shows the cycle with a faint grid of quarters.

**The shape editor.** Clicking the picture opens a larger editor **over the rack, inside the Effects box** (the window does not grow, [ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md) item 4). It has:

- a title with the effect's name and copy ("Volume LFO 2");
- a **Shape** menu with the ready-made shapes: **Pump** (the default), **Sidechain**, **Gate 1/2**, **Gate 1/4**, **Gate 1/8**, **Saw Down**, **Saw Up**, **Triangle**, **Sine**, **Square**. Choosing one replaces the drawn shape (it is not a parameter: it cannot be automated);
- a **Grid** menu (Off, 1/4, 1/8, 1/16, 1/32 of the cycle; default 1/16) that dragged points snap to in time;
- the editing area, with the same gestures as the envelope editor ([envelope-breakpoint](envelope-breakpoint.md) §2): drag a point (it cannot pass its neighbours; the first and last points stay at the start and end of the cycle and only move up and down), double-click empty space to add a point (up to 32), double-click an extra point to delete it, drag a segment up or down to bend it;
- a **Done** button (Escape, or switching tabs, also closes it).

Ready-made shapes, as points `(position in the cycle, level)`; segments are straight unless said:

| Shape | Points |
|---|---|
| Pump (default) | (0, 1), (0.45, 1) bent so it falls fast first, (0.9, 0), (1, 0) |
| Sidechain | (0, 0), (0.35, 1) bent to rise slow first, (1, 1) |
| Gate 1/2 | full for the first half, silent for the second: (0, 1), (0.5, 1), (0.5, 0), (1, 0) |
| Gate 1/4, Gate 1/8 | the same pattern repeated 2 and 4 times in the cycle |
| Saw Down / Saw Up | (0, 1) to (1, 0) / (0, 0) to (1, 1) |
| Triangle | (0, 1), (0.5, 0), (1, 1) |
| Sine | (0, 1), (0.5, 0), (1, 1), both segments bent to follow a cosine closely |
| Square | (0, 1), (0.5, 1), (0.5, 0), (1, 0) with Smooth doing the rounding |

Pump is the default because, restarted at every kick, it keeps the hit at full volume and empties the end of each cycle, which is what gives the space back before the next kick. The point values are the agent's first voicing.

## 3. Parameters

Effect ID `volumelfo`, display name "Volume LFO" (copies `volumelfo2` to `volumelfo4`). `defaultMix` = 1.

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `volumelfo.sync` | Sync | off, on | on | | toggle | per block | yes |
| `volumelfo.rate` | Rate | 0.05 to 30, step 0.01 | 2 | Hz | skewed, midpoint 2 | per block | yes; shown when Sync is off |
| `volumelfo.division` | Rate | same list as `phaser.division` | 1/4 | | choice | per block | yes; shown when Sync is on |
| `volumelfo.depth` | Depth | 0 to 100, step 0.1 | 100 | % | linear | yes (10 ms) | yes |
| `volumelfo.smooth` | Smooth | 0 to 100, step 0.1 | 10 | % | linear | per block | yes |

The shape is not a parameter: it is an effect curve ([ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md) item 3), saved in the project and in presets in the `EffectCurves` node, reset to Pump when the effect is added, copied when it is duplicated.

## 4. DSP / implementation approach

- `fcm::dsp::VolumeLfoEffect` in `src/dsp/fx/volumelfo/`, one registration line, one decoration entry (`ui::CurveThumbnail`, which opens `ui::CurveEditor`). `defaultCurve()` is Pump; `curvePresets()` is the table above.
- Per sample: `phase` from `dsp::TempoLfo` (restarted by `noteOn` at the exact offset), `s = curve.valueAt(phase)` (the envelopes' segment formula, a cached segment index since the phase only moves forward between wraps), target gain `g = 1 - depth * (1 - s)`, linear amplitude.
- Smooth: a one-pole slew on `g` with a time constant from 0.2 ms (0 %) to 20 ms (100 %), skewed so the first half of the knob covers 0.2 to 5 ms. At a note-on the slew continues from where it is (no jump).
- The curve is read through `setCurve` once per block (a new drawing applies from the next block).
- No latency; `latencySamples` is 0. Non-finite input becomes 0.

## 5. Tests

- Pump at 1/4, 120 BPM, Depth 100: on a constant input, the gain is 1 at the note-on, about 0 at 450 ms, and the cycle repeats every 500 ms after it.
- A note-on at offset `k` restarts the shape at sample `k`; in Main behind an effect with latency, the restart lands where the note's sound reaches the effect ([ADR 0012](../decisions/0012-effect-note-triggers-curves-and-decorations.md) item 1).
- Depth 0 is unity (bit-exact); Depth 50 never goes below 0.5.
- Smooth 0 on Gate 1/2: the edge takes at least 0.2 ms (no step larger than the signal step); Smooth 100 takes about 20 ms.
- Each ready-made shape is a valid curve; the editor's operations (add, move, delete, bend, snap) keep a valid curve.
- State: a drawn curve round-trips through the project and a preset; duplicate copies it; add resets it; hostile `EffectCurves` nodes load safely (fallback to Pump).
- Generic tests from `RegisteredEffectsTests.cpp`.

## 6. Out of scope

- Following the DAW's bar position instead of the note, a moving playhead in the picture, more than one shape per copy, applying the shape to anything but volume (the future modulation system).

## 7. Open questions

None.

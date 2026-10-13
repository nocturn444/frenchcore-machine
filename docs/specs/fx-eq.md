# Spec: Parametric EQ effect

- Status: Implemented (Phase 5, third part), reworked on 2026-10-13 from a fixed low shelf / peak / high shelf into a parametric EQ on the product owner's request. Ranges and defaults are first choices by the agent.
- Owner: nocturn4 (product), implemented by agent session
- Related: [fx-chain](fx-chain.md), [fx-distortion](fx-distortion.md), [fx-rack](fx-rack.md), [ARCHITECTURE §4.6](../ARCHITECTURE.md), [ADDING_AN_EFFECT](../ADDING_AN_EFFECT.md)

## 1. What it does for the user

A parametric equaliser with four bands to shape the kick after (or before) the distortion. **Every band is the same and fully free**: you choose what it does (its **Type**), where (**Freq**, anywhere from 20 Hz to 20 kHz), how much (**Gain**, -18 to +18 dB) and how wide (**Q**).

Types:

- **Bell**: boosts or cuts a region around the frequency. A high Q is a thin notch, a low Q a wide sweep. Use it to dig out mud or add punch.
- **Low Shelf**: boosts or cuts everything below the frequency (sub weight).
- **High Shelf**: boosts or cuts everything above the frequency (click and air).
- **Low Cut**: removes everything below the frequency (a high-pass, 12 dB per octave). Gain is not used; Q sets the bump at the corner (0.71 is flat).
- **High Cut**: removes everything above the frequency (a low-pass, 12 dB per octave). Gain is not used.

Each band has an **On** switch. With every band's Gain at 0 and no cut in use the EQ changes nothing, bit for bit. It has the common **On** (off by default) and **Mix** controls, and it adds no delay.

The bands start as a low shelf at 80 Hz, bells at 400 Hz and 2.5 kHz and a high shelf at 8 kHz, all flat, so a first touch of Gain already does something useful.

## 2. User interface

The EQ strip of the Effects box shows a **response display**: the combined curve of the four bands over 20 Hz to 20 kHz (-18 to +18 dB), one numbered dot per band. Drag a dot sideways to change its Freq and up or down to change its Gain; turn the mouse wheel over a dot to change its Q; double-click a dot to switch the band on or off. Under the display the controls of each band are in a column: On, Type, Freq, Gain, Q. See [fx-rack](fx-rack.md).

## 3. Parameters

Effect ID `eq`. The common controls (`eq.enabled`, `eq.mix`) come from the chain. Band parameter IDs are `eq.b1.*` to `eq.b4.*`; the Type is the entry number of the list above (Bell = 0 ... High Cut = 4).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `eq.b1.on` | Band 1 On | off / on | on | | | per block | yes |
| `eq.b1.type` | Band 1 Type | Bell, Low Shelf, High Shelf, Low Cut, High Cut | Low Shelf | | choice | per block | yes |
| `eq.b1.freq` | Band 1 Freq | 20 to 20000, step 1 | 80 | Hz | skewed, midpoint 1000 | per block | yes |
| `eq.b1.gain` | Band 1 Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.b1.q` | Band 1 Q | 0.1 to 10, step 0.01 | 0.71 | | skewed, midpoint 1 | per block | yes |
| `eq.b2.on` | Band 2 On | off / on | on | | | per block | yes |
| `eq.b2.type` | Band 2 Type | Bell, Low Shelf, High Shelf, Low Cut, High Cut | Bell | | choice | per block | yes |
| `eq.b2.freq` | Band 2 Freq | 20 to 20000, step 1 | 400 | Hz | skewed, midpoint 1000 | per block | yes |
| `eq.b2.gain` | Band 2 Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.b2.q` | Band 2 Q | 0.1 to 10, step 0.01 | 1.0 | | skewed, midpoint 1 | per block | yes |
| `eq.b3.on` | Band 3 On | off / on | on | | | per block | yes |
| `eq.b3.type` | Band 3 Type | Bell, Low Shelf, High Shelf, Low Cut, High Cut | Bell | | choice | per block | yes |
| `eq.b3.freq` | Band 3 Freq | 20 to 20000, step 1 | 2500 | Hz | skewed, midpoint 1000 | per block | yes |
| `eq.b3.gain` | Band 3 Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.b3.q` | Band 3 Q | 0.1 to 10, step 0.01 | 1.0 | | skewed, midpoint 1 | per block | yes |
| `eq.b4.on` | Band 4 On | off / on | on | | | per block | yes |
| `eq.b4.type` | Band 4 Type | Bell, Low Shelf, High Shelf, Low Cut, High Cut | High Shelf | | choice | per block | yes |
| `eq.b4.freq` | Band 4 Freq | 20 to 20000, step 1 | 8000 | Hz | skewed, midpoint 1000 | per block | yes |
| `eq.b4.gain` | Band 4 Gain | -18 to +18, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.b4.q` | Band 4 Q | 0.1 to 10, step 0.01 | 0.71 | | skewed, midpoint 1 | per block | yes |

## 4. DSP / implementation approach

- `fcm::dsp::EqEffect` in `src/dsp/fx/eq/`, registered with one line in `RegisterEffects.cpp`, after Distortion.
- Four biquad filters per channel from the Audio EQ Cookbook (Robert Bristow-Johnson): peaking EQ, low shelf, high shelf, 2nd-order high-pass and 2nd-order low-pass, with `A = 10^(gain / 40)` for the first three. The coefficients are computed by plain functions into a fixed struct (no heap, so it is safe in `setParameters`), and the filters run in transposed direct form II in `double`.
- The frequency is limited to 0.45 of the sample rate and the Q to 0.1..20, so the filters stay stable at any setting. A band that is off, or a bell or shelf whose gain is below 0.01 dB in magnitude, is skipped entirely, so a flat EQ is bit-exact. A cut that is on always acts.
- The coefficients are updated once per block when the parameters change; the filter state continues, so a change is smooth for normal moves. Very large jumps in one block may produce a small step; that is accepted.
- No latency (0 at every factor); the effect ignores the oversampling factor.
- Non-finite input is replaced by 0; non-finite output never leaves the effect.
- `EqEffect::magnitudeDb` evaluates a design's response at a frequency. The display uses it through `core::EqResponse`, which reads the band values and returns the curve, so the UI never touches an effect object.

## 5. Tests

Catch2, in `tests/dsp/fx/EqTests.cpp` (the generic tests of `RegisteredEffectsTests.cpp` pick the EQ up automatically):

- Flat: default settings give bit-identical output for any signal; the defaults of the specs are the flat settings.
- Frequency response, measured with sines through the effect at 48 kHz (within 0.5 to 0.8 dB): shelf gains at their ends and half the gain at the corner; bell full gain at the centre and a narrower bell with a higher Q; a band can be any type at any frequency (a bell at 60 Hz on band 4); the cuts are -3 dB at the corner, strongly attenuating beyond it and flat elsewhere.
- A band that is off does nothing, even a cut.
- Combined bands add up (in dB) away from each other's influence.
- `magnitudeDb` agrees with the measured response for every type.
- Display (`core::EqResponse`, `tests/core/EqResponseTests.cpp`): the frequency and gain axes map the ends and the middle, clamp, invert each other and never produce a non-finite number; the bands are read from the parameters (defaults flat, a bell and a low cut are active and shape the curve, a band that is off does not count).
- Stability: extreme settings (Q at both ends and beyond, frequencies at both ends and beyond Nyquist, NaN, gains at both ends, every type, at 44.1, 48 and 96 kHz) give finite, bounded output for noise and for a loud kick-like sine burst.
- Changing parameters block by block (random types, on/off, frequency, gain, Q) does not blow up.
- Mono (one channel) buffers work.
- Layout: the IDs are in the released-ID guard with their ranges and defaults.

Manual check (Standalone): switch EQ on, drag the dots while the auto-play loop runs.

## 6. Out of scope

- More than four bands, a spectrum analyser behind the curve, linear-phase mode, steeper cut slopes (24 dB per octave).
- Smoothing the coefficients sample by sample.

## 7. Decisions taken

1. Four identical bands, each with a type choice, instead of fixed roles (product owner: "I would rather it were parametric").
2. The old parameter IDs (`eq.low.*`, `eq.mid.*`, `eq.high.*`) are gone: they never reached a release. A project saved with them ignores those entries and the EQ takes its defaults.
3. Cookbook biquads, no oversampling, no latency. Cuts are 12 dB per octave.
4. The response display and draggable dots are part of the first parametric version because a parametric EQ without a picture is hard to use.

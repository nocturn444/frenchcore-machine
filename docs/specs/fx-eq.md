# Spec: Parametric EQ effect

- Status: Implemented (Phase 5, third part), reworked on 2026-10-13 from a fixed low shelf / peak / high shelf into a parametric EQ on the product owner's request. Ranges and defaults are first choices by the agent. Version 0.2 adds Extreme and Key Track per band, [section 8](#8-version-02-extreme-peaks-and-key-track) (approved by the product owner on 2026-10-16, questions 5.1 to 5.3) and is implemented in 0.2 stage 3.
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

## 8. Version 0.2: extreme peaks and Key Track

Approved by the product owner on 2026-10-16. Stage 3 of the 0.2 plan ([PLAN](../PLAN.md)).

### 8.1 What it does for the user

Microwave and hardtekk kicks use very narrow, very loud peaks that "whistle". Two additions per band:

- **Extreme** (question 5.1 A): a switch per band. When on, that band's Gain goes up to **+36 dB** (and down to -36) and its Q up to **50**, for razor-thin, screaming peaks. The other bands keep the normal, comfortable ranges. Switching Extreme on in the window starts the extreme controls at the band's current Gain and Q, so the sound does not jump.
- **Key Track** (question 5.2: "follow the MIDI like Serum's filters"): a knob per band, 0 to 100 %. At 0 % (default) the band stays where you put it. At 100 % the band's frequency moves one octave for every octave of the note you play, so a peak tuned to the kick on C stays at the same musical place on every other note. Freq is the frequency at C3 (MIDI note 60); in between, it moves part of the way. It follows the note the tail settles on (with glide), not the pitch envelope. With the voice's Key Track off, the tail stays on the Root Note, and so does the band.

Very loud peaks are caught by the safety limiter at the output, as now (question 5.3 A); there is no automatic level compensation inside the EQ.

### 8.2 User interface

Under each band's Type: an **Extreme** switch and a **Key Track** knob. With Extreme on, the band's Gain and Q knobs are replaced by the extreme ones (`shownWhen`, [ADR 0011](../decisions/0011-effect-interface-additions.md)); the response display draws ±36 dB when any band is extreme, and dragging a dot moves the controls the band is using. The display draws the curve at the current note when Key Track is used (C3 when no note is playing).

### 8.3 New parameters

For each band `n` = 1 to 4 (the existing `eq.bn.gain` and `eq.bn.q` keep their released ranges; changing a released range would change automation in saved projects, so the extreme ranges are separate parameters):

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `eq.bn.extreme` | Band n Extreme | off / on | off | | | per block | yes |
| `eq.bn.xgain` | Band n Extreme Gain | -36 to +36, step 0.1 | 0 | dB | linear | per block | yes |
| `eq.bn.xq` | Band n Extreme Q | 0.1 to 50, step 0.01 | 1.0 | | skewed, midpoint 2 | per block | yes |
| `eq.bn.keytrack` | Band n Key Track | 0 to 100, step 0.1 | 0 | % | linear | per block | yes |

The same parameters exist for the copies `eq2` to `eq4` ([mixer-and-buses](mixer-and-buses.md)). The EQ grows from 20 to 36 parameters (within the new limit of 48, [ADR 0011](../decisions/0011-effect-interface-additions.md)). Effect `version` becomes 2; no migration is needed because every new parameter's default reproduces 0.1.

The copy of Gain and Q into the extreme controls when Extreme is switched on is done by the editor on the message thread (a click in the window), not by the effect, so automating the Extreme switch alone does not copy.

### 8.4 DSP / implementation approach

- A band uses `(extreme ? xgain : gain, extreme ? xq : q)`. The Q limit inside the filter design rises from 20 to 50; the frequency limit stays at 0.45 of the sample rate.
- Key Track: the band's frequency is `freq * 2^(keytrack/100 * noteSemitones / 12)` with `noteSemitones` from the `EffectContext` ([ADR 0011](../decisions/0011-effect-interface-additions.md)), then clamped to 20 Hz..0.45 sr. Coefficients are recomputed when the result changes (once per block).
- A Q of 50 with +36 dB is a very sharp resonance: the filters stay in `double` and transposed direct form II, which remains stable; the coefficient change per block is accepted as it is today (section 4).
- `core::EqResponse` and the display read the extreme values and the note through `core` (no DSP object).

### 8.5 Tests

- Defaults bit-identical to 0.1 (Extreme off, Key Track 0), including the migration tests.
- Extreme bell at +36 dB, Q 50, 1 kHz: +36 dB at the centre (within 1 dB), less than +3 dB at 1.1 kHz; finite and bounded for noise and a loud swept sine at 44.1, 48 and 96 kHz.
- Key Track 100 %: with note 72 the measured peak is at twice Freq; with 50 % at `freq * 2^0.5`; 0 % does not move; the voice's Key Track off keeps it at the Root Note's offset.
- Extreme off ignores xgain and xq; Extreme on ignores gain and q.
- `magnitudeDb` matches the measured response with the new ranges.
- Layout: the new IDs for `eq` and its copies in the released-ID guard.

### 8.5b Decisions made in stage 3 (technical, by the agent)

- A strip with more than two switches or menus in a group puts the extra ones (Extreme) on a row of their own, and a group with more than three knobs gets two rows of knobs (2 + 2 for the EQ: Freq, Gain, Q, Key Track), so the controls stay readable in the strip's width. The EQ strip is therefore taller (it scrolls with the others).
- The editor copies Gain and Q into the extreme controls when the user clicks Extreme on (a table of rules in `ui/EffectDecorations.cpp`, so the rack code knows nothing about the EQ); the copy is clamped by the extreme ranges, which contain the normal ones.
- The controls that replace each other (Gain and Extreme Gain, Q and Extreme Q) show under the same labels, "Gain" and "Q".
- The gain clamp inside the filter design rises from 30 to 36 dB and the Q clamp from 20 to 50 (nothing reachable in 0.1 changes).
- The Key Track frequency is recomputed when the note changes (the context arrives after the parameters each block); the display reads the note from `Engine::currentNoteSemitones()` (an atomic) and repaints at 10 Hz only when a band follows the note.

### 8.6 Out of scope for 0.2

- Following the pitch envelope (question 5.2 asked for the MIDI note only), automatic gain compensation (5.3 A), more bands.

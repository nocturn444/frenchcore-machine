# Spec: Tock (the click layer)

- Status: Implemented (Phase 4, fourth part; every sound decision below was taken by the agent on the product owner's instruction to decide and keep going, and waits for their ears). Reworked for version 0.2 in [section 8](#8-version-02-trimming-pogo-and-its-own-volume) (approved by the product owner on 2026-10-16, questions 4.1 to 4.5; implemented in stage 6). Where section 8 and the sections above disagree, section 8 is the 0.2 behaviour.
- Owner: nocturn4 (product), implemented by agent session
- Related: [voice-note-handling](voice-note-handling.md), [voice-punch-tail](voice-punch-tail.md), [ADR 0008](../decisions/0008-wavetable-selection-and-publishing.md), [ARCHITECTURE §4.3](../ARCHITECTURE.md), [UI_LAYOUT](../UI_LAYOUT.md)

## 1. What it does for the user

The **Tock** is a very short click or knock played at the start of every kick, on top of the oscillators. In frenchcore it is the "attack" you hear on top of the thump: it makes the kick cut through a mix.

- **On**: the Tock is off by default, so the kick sounds as it did before.
- **Mode**: **Synth** makes the click from scratch (a short tone burst plus a burst of noise); **Sample** plays a short recorded-style sound chosen in **Sample**.
- **Sample**: four built-in tock sounds, in menu order (the order never changes, new ones are only added at the end): **Click** (a dry, tiny click), **Knock** (a short low thud, a wooden knock), **Snap** (a quick burst of noisy snap) and **Tick** (a short bright tick). A fifth entry, **User**, plays a sample you load yourself (below).
- **Level**: how loud the Tock is, from silent to +12 dB. Default -12 dB.
- **Tune**: pitch of the Tock in semitones. In Synth mode it moves the tone of the click; in Sample mode it plays the sample faster or slower (and so higher or lower). Default 0.
- **Start**: delay between the note and the Tock, 0 to 20 milliseconds. Default 0. A small delay puts the click just after the first bit of the pitch drop.
- **Decay**: how fast the Tock fades out, 2 to 200 milliseconds (the time to fall by 60 dB). Default 25 ms.
- **Tone**: darkens or brightens it. At 0 the Tock is dull (a low-pass at 200 Hz), at 1 it is open (20 kHz). Default 0.7.

The Tock is part of the kick: it follows the velocity, the Punch/Tail gain and the amp envelope like the oscillators do (it is added before the amp envelope), so releasing the key and retriggering behave the same for it. **Start, Tune, Decay, Tone, Mode and Sample are read when the note starts**, so changing them affects the next kick, not the one that is sounding. Level and On change smoothly at once.

### Your own tock sample

A **Load sample...** button in the Tock box opens a WAV file. If it is usable, **Tock Sample** switches to the **User** entry and the Tock plays it in Sample mode.

- Any WAV works as long as it is short: **at most 2 seconds** and 384000 samples, **at least 16 samples**, at any sample rate from 8 kHz to 192 kHz (it plays at the right pitch whatever the rate). Only the first channel is used.
- It is centred (no DC offset), scaled so its loudest point is full scale, and given a 2 ms fade-out at the end so it never ends with a click.
- An unusable file is refused with a short message under the buttons ("Not a WAV file", "Too long: at most 2 seconds", "The file contains invalid samples", "The file is silent", ...) and the Tock keeps what it had.
- The sample is **saved inside the project** (name and audio; never the file path), like a user wavetable.
- Choosing **User** with nothing loaded uses the first built-in sample (Click).

## 2. User interface

The generic panel gets a fourth column, **Tock**, with the eight controls. The real Tock panel of [UI_LAYOUT.md](../UI_LAYOUT.md) comes with the real panels.

## 3. Parameters

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `tock.on` | Tock On | off / on | off | | | at note-on | yes |
| `tock.mode` | Tock Mode | Synth / Sample | Synth | | | at note-on | yes |
| `tock.sample` | Tock Sample | choice: Click, Knock, Snap, Tick, User | Click | | | at note-on | yes |
| `tock.level` | Tock Level | -48 to +12, step 0.1 (-48 is silence) | -12 | dB | linear | yes (10 ms) | yes |
| `tock.tune` | Tock Tune | -24 to +24 (integer) | 0 | semitones | | at note-on | yes |
| `tock.start` | Tock Start | 0 to 20, step 0.1 | 0 | ms | linear | at note-on | yes |
| `tock.decay` | Tock Decay | 2 to 200, step 0.1 | 25 | ms | skewed, midpoint 25 ms | at note-on | yes |
| `tock.tone` | Tock Tone | 0 to 1 | 0.7 | | linear | at note-on | yes |

The Sample and Mode lists are append-only (the saved value is the entry number).

## 4. DSP / implementation approach

- `fcm::dsp::Tock` (in `fcm_dsp`) renders one sample at a time and costs nothing when idle. `start(settings)` is called at note-on with Mode, Tune, Start, Decay and Tone; `process()` returns the Tock sample (0 when not running). Everything is a plain value (it is copied into the fading copy of the previous kick, like the oscillators).
- **Synth**: a sine at `1200 Hz * 2^(tune/12)` (capped below 0.45 of the sample rate) multiplied by an exponential decay, plus white noise from a small deterministic generator with a faster decay (the envelope cubed), mixed 0.7 tone and 0.5 noise and scaled by 0.8.
- **Sample**: the sample is read with linear interpolation at the rate `2^(tune/12) * sampleRate_of_sample / engineSampleRate`, times the same exponential decay; it stops when the sample ends or the envelope falls below -80 dB.
- **Decay**: `env = exp(-t * ln(1000) / decaySamples)`: -60 dB after Decay.
- **Tone**: one-pole low-pass, cutoff `200 Hz * 100^tone`, coefficient `1 - exp(-2 pi fc / sr)`.
- **Start**: a delay in samples before the Tock begins, counted from the note-on.
- The factory tock samples (`fcm::dsp::FactoryTocks`) are generated by code at 48 kHz (original, free to redistribute), peak-normalised to 1, and built off the audio thread. The selected one is published by `core::TockSelector` through a `PublishedPointer<TockSample>` per [ADR 0008](../decisions/0008-wavetable-selection-and-publishing.md); the voice sets it on the sounding kick and on its fading copies at the start of each block, like the wavetables.
- Parameters travel in `EngineParameters` (`TockParameters`) like the others; `EngineParameterReader` converts them and falls back to the defaults for any value that is not finite.
- Real-time: nothing allocates, locks or waits; the noise generator and the filter are plain state.

### 4.1 User sample

- `core::importTockSample` turns WAV bytes into a `TockSample` or a message. Order of checks: size at most 32 MiB; JUCE's WAV reader opens it; 1 to 8 channels; declared length from 16 samples to 384000 and at most 2 seconds at the file's own rate; sample rate 8000 to 192000; only the first channel is read; every sample finite; then centred, scaled to peak 1, silent files refused, a 2 ms fade-out applied.
- Loading runs on the same one-thread loader as wavetables, off the audio and message threads, and the result is published on the message thread by `TockSelector::loadUser`.
- Saved as a node `UserTockSample` with `name`, `sampleRate` and `samples` (base64 float32). Restoring is untrusted input: at most 2,100,000 characters of text, a stored byte count that is plain digits and matches the text exactly (`core::decodeStoredFloats`, checked before anything is allocated), whole floats, at most 384000 samples, finite, magnitude at most 4, sample rate 8000 to 192000; anything else leaves no user sample.

## 5. Tests

Catch2:

- `FactoryTocks`: four tables with the documented names in order; finite samples, peak 1, no DC to speak of, each ends near silence; deterministic.
- `Tock`: idle gives exactly 0; after `start` it makes sound and finally stops by itself; the level falls 60 dB at Decay (within 3 dB) in Synth mode; Start delays the first sound by that many samples (silent before, sound after); Tune doubles the tone frequency per octave in Synth mode and the playback rate in Sample mode; Tone 0 has much less energy above 2 kHz than Tone 1; sample mode with no sample is silent; all outputs finite for extreme settings (decay and start at the limits, tone 0 and 1, tune +-24, sample rates 44.1, 48, 96 kHz); `reset` makes two runs bit-identical.
- `Voice`/`Engine`: with the Tock off the output is bit-identical to before; on, the first milliseconds of the kick contain the extra energy and the long tail does not change; it follows velocity; a retrigger fades the old Tock with the old kick; changing Tune mid-kick does not change that kick; switching Mode or Sample takes effect at the next note.
- `EngineParameterReader`: defaults and conversions, and non-finite values fall back (the state-loader tests that visit every parameter cover the new ones automatically).
- `TockSelector`: initial sample published; parameter change publishes the new one; an unusable saved value picks a valid one; the list order is guarded.
- Parameters: the new IDs are in the released-ID guard and have the documented defaults and ranges.

- User sample: valid 16-bit and float files are accepted (sample rate and length kept), only the first channel is used, centring, peak 1 and the 2 ms fade; every refusal (not a WAV, truncated, huge declared length, too long, too short, bad sample rate, NaN or infinity, silent, over the byte limit); 3000 randomly damaged files never crash; the selector loads, plays, saves and restores bit for bit and refuses every bad node; the User entry with nothing loaded uses the first sample; the Sample parameter list now ends with User.

Manual check (Standalone): turn Tock on and listen to the four samples and the Synth mode with auto-play running.

## 6. Out of scope

- Several user samples at once, a folder browser, other formats than WAV.
- A tock panel with a waveform display, a "choke" or envelope shape for the tock, stereo tocks.
- Per-note variation (random pitch or start).

## 7. Decisions taken

All of these are first choices for the owner's ears:

1. Off by default, so existing patches sound the same.
2. Four factory samples and the synth click as described; generated by code, 48 kHz.
3. The Tock goes through the amp envelope and the Punch/Tail gain like the oscillators (ARCHITECTURE says "before the amp envelope"), so a very fast amp attack softens a Tock that starts at 0 ms: raise Start or shorten the amp attack if it is too soft.
4. Most controls are read at note-on, not live, so a kick never changes character while it rings.
5. Default Level -12 dB, Decay 25 ms, Tone 0.7.

## 8. Version 0.2: trimming, Pogo and its own volume

Approved by the product owner on 2026-10-16. Stage 6 of the 0.2 plan ([PLAN](../PLAN.md)).

### 8.1 Why

The owner reported that the Tock is barely audible even at full Level. The probable cause written first was that the safety limiter flattens it under a loud hit. Stage 6 started by measuring it.

**Measurement** (Synth mode, the default amp envelope, one note, 48 kHz, left channel, the first 10 ms; the Tock's contribution is the difference between a render with the Tock and one without, as energy relative to the kick alone; a throwaway test, not committed):

| Oscillator 1 level | Tock level | Limiter | Tock energy vs the kick | Tock peak |
|---|---|---|---|---|
| 1.0 | -12 dB (default) | on | -26 dB | 0.12 |
| 0.3 | -12 dB | on | -16 dB | 0.12 |
| 1.0 | +12 dB (the knob's maximum) | on | -4 dB | 1.37 |
| 1.0 | +12 dB | off | -4 dB | 1.93 |
| 0.3 | +12 dB | on / off | -0.6 / -0.5 dB | 1.27 / 1.93 |

**Result.** The limiter hypothesis is only a small part: with the oscillator at full level it takes about 3 dB off the Tock's peak at +12 dB and nothing at the default level, where the Tock is far below the 0.8 knee. The real reasons are: (1) at the default -12 dB the Tock has 1/400 of the energy of the kick's first 10 ms (-26 dB), and even at the knob's maximum it only reaches about -4 dB, so there is little room to make it clearly heard; (2) the Tock follows the amp envelope, so any amp attack or dip in the first milliseconds lowers it further (by construction: its gain is the amp curve; Amp Env makes this optional); (3) the pitch envelope starts the kick's own tone high (+48 semitones), so the first 10 ms of the kick are bright and the click competes with it in the same band. Not measured: Sample mode with a real kick sample.

**What this changes.** Amp Env off (the stage's main fix) removes reason 2. For reasons 1 and 3 nothing was changed on its own: the Level default and range are unchanged (a released default; the owner decides if a louder default or a wider range is wanted, see STATUS), and Trim plus Pogo give the tools to shape a click that cuts through.

### 8.2 What changes for the user

- **Its own volume** (question 4.2 A): a new switch **Amp Env** decides whether the Tock goes through the amp envelope. **Off by default**: the Tock plays at its own Level, untouched by the kick's attack. Projects and presets from 0.1 open with it **on**, so they sound as before (migration, [ADR 0010](../decisions/0010-effect-instances-buses-and-mixer.md) item 8). The Tock still follows the Punch / Tail levels and velocity, and has its own channel in the mixer ([mixer-and-buses](mixer-and-buses.md)), so it can go to a bus of its own.
- **Load any kick and keep its transient** (question 4.1 A: Synth mode and the four built-in sounds stay). In Sample mode the Tock box shows the **waveform** of the current sample with two handles: **Trim Start** and **Trim End**. Only the part between them plays. They work on the built-in sounds too (at 0 % and 100 %, the defaults, nothing changes). A 0.5 ms fade-in (only when Trim Start is above 0) and a 2 ms fade-out at Trim End keep the cut from clicking.
- **Detect** button (question 4.4 A): places the handles on the transient of the sample by itself (start just before the first hit, end where the hit's high-frequency energy has fallen by 20 dB, between 5 and 80 ms later). You can then move them.
- **Pogo** (question 4.3 A): a bipolar knob, -24 to +24 semitones, that bends the Tock's pitch over the length of the part that plays: positive values make it rise as it plays, negative values make it fall. 0 (default) does nothing. In Synth mode it bends the tone of the click. The curve is exponential (fast at first), a first choice for the owner's ears.
- **Longer samples** (question 4.5 A): your own sample can be up to **10 seconds** (a whole kick), so you can load a full kick and trim it. Files at a sample rate above 48 kHz are converted to 48 kHz when loaded, to keep projects and presets small.
- Existing controls (Level, Tune, Start, Decay, Tone, Mode, Sample) stay as they are. Decay still fades the Tock out; set it long (200 ms) to hear the trimmed part as it is.

### 8.3 User interface

Tock box (440 x 180 points, unchanged size): row 1: On, Mode, Sample, Load sample..., Detect, Amp Env; row 2: the waveform display (about 60 points high) with the two handles, the part outside them dimmed (Synth mode shows a flat "Synth" label instead); row 3: knobs Level, Tune, Start, Decay, Tone, Pogo. Dragging a handle changes the Trim parameter (so it can be automated and undone); double-click a handle to reset it.

### 8.4 New parameters

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `tock.ampEnv` | Tock Amp Env | off / on | off (on for states from 0.1) | | | at note-on | yes |
| `tock.trimStart` | Tock Trim Start | 0 to 100, step 0.01 | 0 | % of the sample | linear | at note-on | yes |
| `tock.trimEnd` | Tock Trim End | 0 to 100, step 0.01 | 100 | % of the sample | linear | at note-on | yes |
| `tock.pogo` | Tock Pogo | -24 to +24, step 0.01 | 0 | semitones | linear | at note-on | yes |

If Trim End is not at least 1 ms after Trim Start, the Tock plays 1 ms from Trim Start.

### 8.5 DSP / implementation approach

- `Tock::start(settings)` gains `trimStart`, `trimEnd` (fractions) and `pogoSemitones`. Sample mode reads from `trimStart * length` to `trimEnd * length` with the fades above; the pitch offset at time `t` within the played part of length `T` (in output samples, accounting for Tune) is `pogo * (1 - exp(-4 t / T)) / (1 - exp(-4))`, applied to the read rate (Sample) or to the tone frequency (Synth, where `T` is the Decay time).
- Amp Env off: the voice adds the Tock to the Tock stem after the amp envelope instead of before it (still times Punch/Tail gain and velocity).
- Detect: `core::detectTransient(const TockSample&)` on the message thread: onset = first sample above -30 dB of the peak, minus 1 ms; end = after the peak, where a 1 ms RMS envelope of the signal high-passed at 1 kHz falls 20 dB below its maximum, clamped to 5..80 ms after the onset. Pure function, unit tested; it writes the two Trim parameters.
- Import: `kMaxTockSeconds` = 10; above 48 kHz the file is resampled to 48 kHz on the loader thread (windowed-sinc, `juce::WindowedSincInterpolator`); stored samples at most 480000. Restore accepts at most 480000 samples (the 0.1 limit of 384000 is below that, so 0.1 states still load).
- The waveform display reads a decimated copy of the current sample through `core::TockSelector` (message thread), never the audio objects.

### 8.5b Technical decisions made while implementing

- A trim that starts after 0 gets the 0.5 ms fade-in and one that ends before 100 % the 2 ms fade-out, both counted in source samples (so Tune changes their audible length a little). At 0 % and 100 % no fade is added (the import already fades the last 2 ms), which keeps the defaults bit-identical to 0.1.
- Pogo bends the read rate, so a positive Pogo makes the part play faster and end sooner (about 22 ms instead of 40 for +12 on a 40 ms part), a negative one longer. The bend curve is measured against the nominal length of the part.
- Without Amp Env the Tock still stops when the amp envelope ends, because the kick stops there; a very short amp envelope can therefore cut a long Tock.
- Detect runs when a sample is published (message thread, a few milliseconds even for 10 seconds) and the button only writes the two Trim parameters. The built-in Click sample is shorter than the minimum, so Detect leaves it whole.
- The Tock and Noise imports share the 48 kHz conversion (`core/Resample.*`); the stored limit of both is 480000 samples. The 0.1 limit of 384000 is below it, so every saved sample still loads.
- The migration from state version 1 adds `tock.ampEnv` = on to the state (inside the existing 1 to 2 step, because 0.2 is not released yet); presets and projects from 0.1 therefore sound as before. The Hardcore Crunch factory preset sets it on itself.

### 8.6 Tests

- The measurement of 8.1, then: with Amp Env off and a slow amp attack (20 ms), the Tock's first 5 ms are as loud as with Amp Env on and a 0 ms attack.
- Every 0.1 factory preset that uses the Tock (Hardcore Crunch) and a saved 0.1.1 project with the Tock on render the same after migration (the tolerance of [mixer-and-buses](mixer-and-buses.md) section 5).
- Trim: only the chosen part plays (energy outside it is zero); the fades are there; Trim End before Trim Start plays 1 ms; defaults are bit-identical to 0.1.
- Pogo: +12 makes the read rate reach about twice the start rate at the end of the part; -12 half; 0 is bit-identical; Synth mode bends the tone.
- Detect: on synthetic kicks with known onset and click length, the handles land within 1 ms of the onset and within the documented end rule; silence and DC give 0 % and 100 %.
- Import: a 9-second file loads, an 11-second one is refused ("Too long: at most 10 seconds"), a 96 kHz file is converted to 48 kHz and keeps its pitch; the 0.1 import tests still pass with the new limits.
- Parameters: new IDs in the released-ID guard.

### 8.7 Out of scope for 0.2

- Several samples layered, a sample editor beyond the two handles, stereo Tock, Pogo with a separate time control (question 4.3 B).

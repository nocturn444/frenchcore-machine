# Spec: Tock (the click layer)

- Status: Implemented (Phase 4, fourth part; every sound decision below was taken by the agent on the product owner's instruction to decide and keep going, and waits for their ears)
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

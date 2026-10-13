# Spec: Wavetable oscillator

- Status: Implemented (format and anti-aliasing from `docs/ARCHITECTURE.md` §4.2; the defaults in section 7 are still waiting for the product owner's review)
- Owner: nocturn4 (product), implemented by agent session
- Related: [voice-note-handling](voice-note-handling.md), [ARCHITECTURE §4.2](../ARCHITECTURE.md)

## 1. What it does for the user

There are two identical oscillators, **Osc 1** and **Osc 2**. Each one plays a wavetable (a stack of single-cycle waveforms) and both follow the kick's pitch drop and the MIDI note. In Phase 2 the only wavetable is a built-in pure sine, so the kick is a clean sine kick; factory and user wavetables arrive in Phase 4.

Per oscillator:

- **On**: switches the oscillator on or off (switching is click-free).
- **Level**: how loud this oscillator is in the mix.
- **Pitch** (semitones), **Octave**, **Fine** (cents): tune this oscillator relative to the kick's pitch. Useful for layering, for example osc 2 one octave up.
- **Position**: which waveform of the wavetable plays; in between waveforms it morphs smoothly. With the built-in sine (one waveform) it has no audible effect yet.
- **Phase**: where in the waveform the oscillator starts at every note. The same phase every time means every kick starts identically.

The oscillator never makes harsh aliasing sounds, even at the very top of the fast pitch drop, because the wavetable has pre-made versions with the high harmonics removed for each pitch range (see [glossary](../GLOSSARY.md): band-limited / mipmaps).

Defaults: Osc 1 on at full level, Osc 2 off at full level. A fresh instance is a clean sine kick.

## 2. User interface

The oscillator panels of [UI_LAYOUT.md](../UI_LAYOUT.md): a header with On, the Wavetable menu and Load; a strip with Pitch, Octave and Fine; the wavetable display; knobs for Position, Level and Phase ([ui-grid-layout](ui-grid-layout.md)).

## 3. Parameters

Each oscillator has these, with the ID prefix `osc1.` or `osc2.`:

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `oscN.on` | Osc N On | off / on | osc1 on, osc2 off | | | yes (via level, 10 ms) | yes |
| `oscN.level` | Osc N Level | 0 to 1 | 1.0 | linear gain | linear | yes (10 ms) | yes |
| `oscN.pitch` | Osc N Pitch | -24 to +24 (integer) | 0 | semitones | | yes (20 ms, with octave and fine) | yes |
| `oscN.octave` | Osc N Octave | -4 to +4 (integer) | 0 | octaves | | yes (as above) | yes |
| `oscN.fine` | Osc N Fine | -100 to +100, step 0.1 | 0 | cents | linear | yes (as above) | yes |
| `oscN.position` | Osc N Position | 0 to 1 | 0 | normalized | linear | yes (10 ms) | yes |
| `oscN.phase` | Osc N Phase | 0 to 360 | 0 | degrees | linear | no (read at note-on) | yes |

Total detune in semitones is `pitch + 12 * octave + fine / 100`.

## 4. DSP / implementation approach

### 4.1 Wavetable data (`fcm::dsp::WavetableData`)

- A wavetable is `N` frames (1 to 256) of 2048 samples each, matching Serum/Vital single-cycle frames. A single-cycle WAV is the case `N = 1`.
- On construction from raw frames, the builder creates **11 mip levels**. Level `m` keeps harmonics `1 .. 1024 / 2^m` (level 0 keeps 1023: bin 1024 is the Nyquist bin, whose phase is ambiguous, so it is removed; level 10 keeps only the fundamental) plus the DC term. Each frame is transformed with the real FFT (`juce::dsp::FFT`, order 11), the bins above the limit are zeroed, and the inverse transform gives the band-limited frame. This follows the standard band-limited wavetable mipmap approach (octave-spaced tables built by FFT truncation, as described in Nigel Redmon's "A wavetable oscillator" series on earlevel.com).
- Every stored frame has one extra guard sample equal to its first sample, so linear interpolation never wraps. Storage is `11 * N * 2049` floats in one contiguous array (about 23 MB for the largest 256-frame table).
- The data is immutable after construction. It is built off the audio thread. Phase 2 builds the built-in sine in `Engine::prepare()`; Phase 4 publishes user wavetables by atomic pointer swap as described in ARCHITECTURE §6.
- `WavetableData::makeSine()` returns one frame holding one cycle of `sin`.

### 4.2 Oscillator (`fcm::dsp::WavetableOscillator`)

- 64-bit phase accumulator in [0, 1). Increment per sample is `frequency / sampleRate`; frequency is clamped to [0, 0.45 * sampleRate].
- Mip level for the current frequency: the smallest `m` such that `1024 / 2^m <= nyquist / frequency` (equivalently `m = ceil(log2(2048 * frequency / sampleRate))`, clamped to 0..10), so no harmonic can exceed Nyquist. The level is chosen per sample, because the pitch envelope sweeps the frequency inside a note. Known limitation: the switch between levels is not crossfaded, so a table with strong upper harmonics has a small spectral step at each octave boundary of the sweep. Revisit with the factory wavetables in Phase 4.
- Output: linear interpolation between samples, then between the two neighbouring frames for a fractional `position * (N - 1)`.
- `reset(startPhase)` sets the phase; no other state besides the phase and the position exists.
- Real-time safe: no allocation, no locks. The wavetable is held by raw pointer owned by the engine and must outlive the oscillator.

## 5. Tests

- `WavetableData`: mip 0 of a random frame matches the source within 1e-4 once the Nyquist component is subtracted (inverse FFT scaling is right, only that bin is removed); a sawtooth's mip level `m` has no spectral content above `1024 / 2^m` harmonics (measured with the FFT, below -100 dB); every guard sample equals its frame's first sample; frame counts above 256 or lengths that are not a multiple of 2048 are rejected.
- `WavetableOscillator` with the sine table: measured frequency (zero-crossing over many cycles) within 1 cent of the requested frequency at 55 Hz, 440 Hz, 4 kHz and at 44.1, 48 and 96 kHz; amplitude within 0.001 of 1; distance from an ideal sine below -90 dB; deterministic after `reset`; finite output including frequencies of 0 and above Nyquist.
- Aliasing: a sawtooth table played at about 4 kHz (44.1 kHz, a frequency chosen so the window holds whole cycles) has less than -80 dB of energy off the harmonic bins, while a naive sawtooth made straight from the phase measures above -40 dB (the control that proves the test can fail).
- Frame interpolation: with a 2-frame table (frame 0 all `-0.5`, frame 1 all `+0.5`) the output at `position = 0.25` is `-0.25` within 1e-4.
- Start phase: `reset(0.25)` makes the first sample of a sine equal 1 within 1e-4.

## 6. Out of scope

- Loading wavetables from disk and factory wavetables (Phase 4).
- Wavetable display, wavetable position modulation.
- Unison, FM or sync.
- Crossfading between mip levels.

## 7. Open questions

1. Osc 2 defaults to off. OK, or should it start on at a lower level?
2. `Phase` is a start phase at every note (resets each kick). A free-running option is not planned.

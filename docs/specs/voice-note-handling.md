# Spec: Voice and note handling

- Status: Implemented (behaviour taken from `docs/PLAN.md` Phase 2 and `docs/ARCHITECTURE.md` §4.1; the sound defaults in section 7 are still waiting for the product owner's review)
- Owner: nocturn4 (product), implemented by agent session
- Related: [oscillator-wavetable](oscillator-wavetable.md), [ARCHITECTURE §4.1, §4.4, §4.7](../ARCHITECTURE.md), [ADR 0005](../decisions/0005-engine-parameter-handoff.md)

## 1. What it does for the user

A MIDI note plays a kick. The kick starts with a very fast pitch drop (from four octaves above the note, down to the note's own pitch) and then rings out as a decaying tone. That drop is what gives the kick its punch; the pitch it lands on is the "bass" of the kick.

- **One voice (monophonic).** A new note always restarts the kick from the beginning, like retriggering a drum. Two notes at once never play two kicks on top of each other.
- **The MIDI note tunes the tail.** Play a different note and the kick settles on a different pitch. With **Key Track** off, the tail always settles on the **Root Note**, whatever key you play, so you can put the kick in any pattern without changing its tuning.
- **The kick always plays out.** Releasing the key does not cut the kick: it decays on its own, so a short MIDI note (for example a 1/16 note at a fast tempo) still gives the whole kick. Phase 3 revisits this when the amp envelope gets a sustain and a release.
- **Glide (optional).** With **Glide** on, the tail pitch slides from the previous note's pitch to the new one over **Glide Time**, instead of jumping. The pitch drop at the start of each kick is not affected: only the pitch it lands on slides.
- **Velocity.** **Velocity Sensitivity** sets how much the MIDI velocity changes the loudness: at 0 % every note is equally loud, at 100 % velocity 1 is silent-ish and velocity 127 is full level.
- **No clicks.** Retriggering while the previous kick is still sounding does not click: the old kick is faded out over about 1.5 ms while the new one starts at the exact sample the note arrives.
- **Safe output.** An output gain knob and a safety limiter that never lets the signal go above 0 dBFS, so a very loud kick cannot hurt speakers or ears.

Overlapping notes use last-note priority: if you hold note A, play note B, then release B while A is still held, the tail pitch goes back to A (sliding if Glide is on) without restarting the kick.

## 2. User interface

Phase 2 uses a generic parameter panel (one knob, slider or switch per parameter, grouped by `global`, `osc1`, `osc2`). The real panels from [UI_LAYOUT.md](../UI_LAYOUT.md) replace it in later phases. These controls end up in the **Global** panel.

## 3. Parameters

The fixed amp and pitch envelopes have no parameters in Phase 2 (hard-coded shapes, see section 4). Oscillator parameters are in [oscillator-wavetable](oscillator-wavetable.md).

| ID | Name | Range | Default | Unit | Skew/Curve | Smoothed | Automatable |
|---|---|---|---|---|---|---|---|
| `global.outputGain` (exists) | Output Gain | -48 to +12, step 0.1 (-48 is silence) | 0 | dB | linear | yes (20 ms) | yes |
| `global.limiterOn` | Safety Limiter | off / on | on | | | no (see 4.6) | yes |
| `global.keyTrack` | Key Track | off / on | on | | | no | yes |
| `global.rootNote` | Root Note | 12 to 84 (integer) | 36 (shown as C1) | MIDI note | | no | yes |
| `global.glideOn` | Glide | off / on | off | | | no | yes |
| `global.glideTime` | Glide Time | 1 to 1000 | 100 | ms | skewed, midpoint 100 ms | no | yes |
| `global.velocitySensitivity` | Velocity Sensitivity | 0 to 100 | 100 | % | linear | no | yes |

Note names follow the JUCE convention (MIDI note 60 is C3, so note 36 is C1).

## 4. DSP / implementation approach

### 4.1 Note handler (`fcm::dsp::NoteHandler`)

- Fixed-size stack of held notes (16 entries, no allocation). `noteOn` pushes (or moves to the top if the note is already held) and reports "retrigger"; `noteOff` removes the note and reports the note that is now on top, if any. Older entries are dropped when the stack is full. Note-off for a note that is not held is ignored. `allNotesOff` clears the stack.
- Velocity is stored with the note that triggered the kick; going back to an older held note does not change the level of a kick already playing.

### 4.2 Pitch glide (`fcm::dsp::PitchGlide`)

- Works on a fractional MIDI note number (semitones), so a slide is musically even (exponential in Hz).
- `setTarget(note, glideOn, glideMs)`: with glide off, or before the first note, jumps to the target. With glide on, a linear ramp in semitones that takes exactly `glideMs`, whatever the interval. A new target mid-slide starts a new ramp of `glideMs` from the current value.
- The settling pitch target is `keyTrack ? lastHeldNote : rootNote`. If the user changes Key Track or Root Note while the kick sounds, the target changes and follows the same glide rules.
- One value per sample, reached sample-accurately.

### 4.3 Voice (`fcm::dsp::Voice`)

Per sample, for each of the two oscillators:

```
noteHz   = 440 * 2^((settlePitch + pitchEnvOffset + oscSemitones - 69) / 12)
sample  += oscillator.process(noteHz) * oscLevel
output   = sample * ampEnvelope * velocityGain
```

- `pitchEnvOffset` is in semitones above the settling pitch, from the fixed pitch envelope.
- `velocityGain = 1 - sensitivity * (1 - velocity)` with `velocity` in 0..1 and `sensitivity` in 0..1, evaluated at note-on.
- On note-on both oscillators restart at their `phase` parameter and both envelopes restart. If the voice is still sounding, a copy of its state keeps rendering for `kRetriggerFadeMs` (1.5 ms) with a linear fade-out gain and is added to the new kick (a "ghost"). There are `kMaxGhosts` (4) ghost slots, so notes closer together than the fade, such as a chord sent at one instant, still fade out without a step; a kick that has not output a sample yet is simply replaced. A voice that has fully decayed is skipped to save CPU.
- The voice is silent and skipped once the amp envelope reaches silence.

### 4.4 Fixed envelopes (Phase 2 placeholders for Phase 3's `BreakpointEnvelope`)

Both envelopes restart on every note-on and ignore note-off.

- **Amp:** linear attack from 0 to 1 in `kAttackMs` = 1 ms, then an exponential decay of -60 dB in `kDecaySeconds` = 0.5 s. When the level falls below -80 dB the envelope snaps to 0 and the voice goes idle.
- **Pitch:** offset in semitones = `kAmountSemitones` (+48) times a normalized exponential fall that reaches exactly 0 after `kDropMs` = 90 ms. Shape: `(e^(-t/tau) - e^(-T/tau)) / (1 - e^(-T/tau))` with `T` = 90 ms and `tau` = `T / 5`. After `T` the voice runs at exactly the settling pitch.
- Both are computed with a per-sample running multiplication (no `exp` per sample).

### 4.5 Engine (`fcm::dsp::Engine`)

- `setParameters(const EngineParameters&)` is called once per block by the plugin ([ADR 0005](../decisions/0005-engine-parameter-handoff.md)).
- `process` walks the block's MIDI events in time order and renders the audio between events, so note timing is sample-accurate. It renders mono into channel 0, applies output gain and the safety limiter, then copies the result to the other channels.
- The engine creates its built-in sine wavetable in `prepare()`. Nothing allocates in `process`.

### 4.6 Output stage

- `Output Gain` (dB to linear, smoothed over 20 ms) then the safety limiter.
- The limiter is a soft clipper that leaves everything below 0.8 untouched and bends the signal smoothly toward, never above, 1.0: for `|x| > 0.8`, `y = sign(x) * (0.8 + 0.2 * tanh((|x| - 0.8) / 0.2))`, followed by a hard clamp to [-1, 1]. With the limiter off the signal passes through unchanged (non-finite samples are still replaced by 0). It is not smoothed: switching it while a very loud kick sounds can cause a click, which is acceptable for an advanced "off" option.

## 5. Tests

Catch2, in `tests/dsp/` (headless, `fcm_dsp_tests`):

- `NoteHandler`: last-note priority, release returns to the previous held note, release of an unheld note ignored, stack overflow drops the oldest, `allNotesOff`.
- `PitchGlide`: jump when glide is off or before the first note; ramp reaches the target after exactly `glideMs`; ramp length is independent of the interval; retarget mid-slide; linear in semitones.
- Envelopes: amp falls 60 dB in 0.5 s (within 0.2 dB), then snaps to idle within its stated maximum length; the attack is monotonic and starts near 0; pitch envelope starts at +48, is monotonic, and is exactly 0 after 90 ms; deterministic after reset.
- `Voice`: output is finite for all supported sample rates (44.1, 48, 96 kHz); a note-on produces non-zero output; **tail pitch within 1 cent** of the MIDI note frequency, measured after the pitch drop for notes C1, A1, C2 at 48 kHz; key track off uses the root note whatever note is played; velocity sensitivity scales the level (0 % equal, 100 % proportional); retrigger step size stays small (no click), and a control in the same test shows that a hard restart at the same instant would be a large step; a chord of notes at the same sample and notes a few samples apart also stay click-free (Engine); `reset` makes two runs bit-identical.
- `Engine`: MIDI timing is sample-accurate (silent before the note-on offset, sound from it); glide moves the tail pitch gradually between two consecutive notes and with glide off jumps; output never exceeds 0 dBFS with the limiter on, even for output gain +12 dB and two oscillators at full level; silence when no note has been played; a note-off does not cut the kick; allocation-free `process` is checked in Phase 7 with the debug guard (not available yet).
- Core (`fcm_core_tests`): every parameter ID is unique and prefixed `global.`, `osc1.` or `osc2.`; defaults of the layout map to the expected `EngineParameters`; ranges map correctly.

Manual check (Standalone or a DAW): play a MIDI note, hear the kick; play fast repeated notes without clicks; toggle Key Track and Glide.

## 6. Out of scope

- Editable envelopes, sustain and release (Phase 3).
- Polyphony or legato options.
- Tock, Punch/Tail balance, FX chain, oversampling (later phases).
- An on-screen keyboard or "audition" button in the plugin window: the Standalone needs a MIDI keyboard or a virtual MIDI source for now.
- A look-ahead limiter. The safety limiter is a soft clipper.

## 7. Open questions

Defaults the agent chose from the plan; the product owner can change any of them:

1. Pitch drop: +48 semitones in 90 ms with an exponential shape. Is this the right punch, or should it be faster/slower?
2. Amp decay: -60 dB in 0.5 s with no sustain. (Phase 3's "kick + bass" shape will replace this.)
3. Should the kick always play out when the key is released (current), or stop on note-off?
4. Default Root Note C1 (about 65 Hz) when Key Track is off.
5. Default Velocity Sensitivity 100 %. A fixed 0 % might suit kick programming better.
6. Default Glide Time 100 ms.

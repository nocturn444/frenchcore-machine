# 0011: Effect interface additions for 0.2: note context, conditional controls, default mix, more parameters

- Status: Accepted
- Date: 2026-10-16
- Extends: [0009](0009-effect-parameters-and-chain-latency.md)

## Context

The 0.2 effects need things the 0.1 `Effect` interface cannot describe:

- the EQ's Key Track ([fx-eq](../specs/fx-eq.md)) needs the pitch of the note the kick is playing;
- the Compressor shows different controls with Multiband off or on, the EQ shows different Gain and Q ranges with Extreme off or on, and the Distortion already has Bits and Rate that only matter for Bitcrush;
- a reverb at 100 % wet is a bad starting point, but every effect's Mix defaults to 100 %;
- the EQ grows to 36 parameters, over `kMaxEffectParameters` (24).

## Decision

1. **`EffectContext`**, set once per block before `process()` with a new virtual `setContext(const EffectContext&) noexcept` (default: ignored). It carries `noteSemitones`: the pitch the tail settles on (the note with Key Track on, the Root Note with it off, including glide), in semitones relative to MIDI note 60, for the first sample of the block; and `tempoBpm`. Effects never see MIDI.
2. **`ParameterSpec::shownWhen`**: optional, the suffix of a toggle or choice parameter of the same effect and a bit mask of the values for which this control is shown (empty: always shown). Only the UI uses it; the parameter exists and is automatable either way.
3. **`EffectDescriptor::defaultMix`** (0 to 1, default 1): the default of `<id>.mix` for every instance of that effect.
4. `kMaxEffectParameters` becomes 48.

## Consequences

- Existing effects compile unchanged: the new members have defaults that reproduce 0.1. Distortion gains `shownWhen` on Bits and Rate (UI only, no sound change).
- `defaultMix` must be chosen before an effect is released: changing it later would change the sound of projects that never touched Mix (they restore Mix from the saved value only if it was saved, and a saved state always describes everything, so in practice it is saved; the rule is kept anyway).
- An effect that follows the note gets block-rate updates; glides and fast pitch movements inside a block are followed with a block of delay, which is inaudible at normal block sizes.

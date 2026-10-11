# Architecture

This document describes how Frenchcore Machine is built: the modules, how audio flows through them, how state is stored, and the rules that keep the code modular and real-time safe. It is the reference every contributor (human or agent) reads before writing code.

If code and this document disagree, one of them is wrong. Fix whichever is wrong in the same pull request.

## 1. Goals that shape the design

1. **Modular and scalable.** New effects, oscillator features and envelope targets must be addable without editing unrelated code. The effects section is the main extension point.
2. **Real-time safe.** Nothing on the audio thread allocates memory, takes a lock, does file I/O or logs.
3. **Testable without a DAW.** All DSP lives in a library that has no dependency on the plugin wrapper or the UI, so it can be unit-tested headless.
4. **Stable automation and presets.** Reordering effects or adding new effects in later versions must never break saved projects or presets.

## 2. Technology stack

| Concern | Choice | Notes |
|---|---|---|
| Language | C++20 | Exceptions are allowed outside the audio thread only. See real-time rules. |
| Framework | JUCE 9 | Plugin wrappers, UI, MIDI, `juce_dsp`. |
| Build system | CMake ≥ 3.25 + [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) | CPM fetches JUCE and Catch2 at a pinned version. No git submodules. |
| Plugin formats | VST3, AU (macOS only), Standalone | CLAP may be added later via `clap-juce-extensions`. |
| Platforms | Windows (MSVC), macOS (Apple Clang, universal binary), Linux (GCC/Clang) | |
| Unit tests | Catch2 v3 | Tests target the `fcm_dsp` and `fcm_core` libraries. |
| Plugin validation | pluginval (strictness 10 in CI) | |
| CI | GitHub Actions | Build + test on all three platforms for every PR. |
| Formatting | clang-format (config in repo root) | Enforced in CI. |
| License | AGPL-3.0-only | Required by the free JUCE 9 license. See [ADR 0002](decisions/0002-license-agplv3.md). |

## 3. Repository layout

```
frenchcore-machine/
├── CMakeLists.txt            Top-level build: options, CPM, subdirectories
├── cmake/                    CPM.cmake and helper CMake modules
├── src/
│   ├── core/                 fcm_core: parameter IDs, state model, presets, wavetable I/O
│   ├── dsp/                  fcm_dsp: all audio processing, no UI, no plugin wrapper
│   │   ├── engine/           Voice, glide, tock sampler, engine orchestration
│   │   ├── oscillator/       Wavetable data, mipmaps, oscillator
│   │   ├── envelope/         Breakpoint envelope model and renderer
│   │   └── fx/               Effect interface, registry, chain, one folder per effect
│   ├── ui/                   fcm_ui: editor components (envelope editor, FX rack, ...)
│   └── plugin/               JUCE AudioProcessor / AudioProcessorEditor glue
├── resources/
│   ├── wavetables/           Factory wavetables (WAV), embedded via BinaryData
│   └── presets/              Factory presets (XML), embedded via BinaryData
├── tests/                    Catch2 unit tests, mirrors src/ layout
├── docs/                     All documentation (this file, plan, specs, ADRs)
└── .github/                  CI workflows, PR and issue templates
```

Libraries and their allowed dependencies (arrows mean "may depend on"):

```
plugin ──► ui ──► core ──► dsp
   │                 ▲
   └─────────────────┘
```

- `fcm_dsp` depends only on the standard library and `juce_audio_basics` / `juce_dsp`. It knows nothing about parameters, presets or UI.
- `fcm_core` owns the parameter layout, the state tree and serialization. It depends on `fcm_dsp` for type definitions (e.g. effect descriptors).
- `fcm_ui` reads and writes state through `fcm_core`. It never touches DSP objects directly.

How this is built: the four libraries are CMake `INTERFACE` libraries that carry their sources and include paths. Each final binary (the plugin, each test executable) compiles them together with the JUCE modules it links, because JUCE modules must be linked `PRIVATE` by the final target and must not be compiled twice into one binary. The `fcm_dsp` boundary is enforced by the `fcm_dsp_tests` executable, which links only `juce_dsp`: a dsp file that includes UI, APVTS or plugin headers fails to build there.

### Third-party dependencies

| Dependency | Version | Licence | Used for |
|---|---|---|---|
| JUCE | 9.0.3 | AGPLv3 (or commercial) | Framework, plugin wrappers, UI, DSP helpers |
| Catch2 | 3.16.1 | BSL-1.0 | Unit tests (not shipped) |
| CPM.cmake | 0.43.2 (vendored in `cmake/`) | MIT | Dependency fetching at configure time |
| pluginval | 1.0.4 | GPLv3 | Plugin validation in CI only (not shipped, not linked) |
- `plugin` wires everything together and is the only place that knows about the host.

## 4. Signal flow

```
MIDI ──► NoteHandler (mono, last-note priority, optional glide)
              │ pitch, gate, velocity
              ▼
        ┌──────────────────────────────┐
        │ Voice                        │
        │  Pitch envelope ─► pitch     │
        │  Oscillator 1 (wavetable)    │
        │  + Oscillator 2 (wavetable)  │
        │  + Tock (optional)           │
        │  × Amp envelope              │
        │  × Punch/Tail balance        │
        └──────────────┬───────────────┘
                       ▼
        FX chain (user-ordered slots, each with bypass and mix)
                       ▼
        Output gain ─► Safety limiter (soft clip, always last, not reorderable)
                       ▼
                    Host output
```

### 4.1 Voice and note handling

Details and tests in [voice-note-handling](specs/voice-note-handling.md).

- Monophonic. A new note retriggers both envelopes from the start (kick behaviour). Retrigger can be made legato-optional later.
- `NoteHandler` keeps a fixed-size stack of held notes (last-note priority). Releasing the sounding note while another is held moves the pitch back to the older note without retriggering. Releasing the last held key fades the amp envelope out with a short release (30 ms), so the note length sets the tail length.
- The MIDI note sets the pitch the pitch envelope settles on (the "bass" tail). A `Key Track` switch allows a fixed `Root Note` instead.
- Glide (portamento) is optional: `Glide On`, `Glide Time` (ms). `PitchGlide` slides the settling pitch only, not the pitch envelope's attack, with a linear ramp in semitones.
- Velocity can scale output level (`Velocity Sensitivity`, 0 to 100%, default 0% so every kick is equally loud).
- Click-free retrigger: when a note arrives while the voice is sounding, a copy of the previous voice state keeps rendering for 1.5 ms with a fade-out and is added to the new kick. The new kick starts at the exact sample of the note-on, so there is no added latency.
- Until the envelope editor (Phase 3), the amp and pitch envelopes are hard-coded shapes with constants in `src/dsp/envelope/`.

### 4.2 Wavetable oscillators

- Two identical oscillators (`osc1`, `osc2`). They are two instances of the same `WavetableOscillator` class with parameter IDs prefixed by the oscillator ID, so the count is a constant, not duplicated code.
- Each oscillator has: On, Level, Pitch (semitones), Octave, Fine tune (cents), Position, Phase (start phase at every note) and its own wavetable. Ranges and defaults in [oscillator-wavetable](specs/oscillator-wavetable.md). Both follow the same pitch envelope and note pitch.
- A wavetable is a sequence of frames of 2048 samples each (Serum/Vital-compatible WAV). Up to 256 frames.
- `Position` selects/interpolates between frames.
- Anti-aliasing: on load, each frame is converted to a set of band-limited mipmaps (one per octave, built with FFT by zeroing harmonics above Nyquist). Playback picks the mipmap for the current frequency and interpolates linearly between samples and between adjacent frames.
- Loading and mipmap generation happen on a background thread. The finished `WavetableData` is published to the audio thread with an atomic pointer swap; the old one is released on the message thread (see section 6).
- The source of the user's wavetable (file path and an embedded copy of the audio) is stored in the plugin state so projects survive moving files.

### 4.3 Tock (click layer with optional samples)

- Optional short transient (the "tock") mixed before the amp envelope. Two sources, selected with `Tock Mode`:
  - **Synth:** a generated click (short noise burst or pitched blip) with its own decay and filter.
  - **Sample:** a factory tock or a user-loaded WAV. User samples are embedded in the plugin state like wavetables, and loading follows the same background-load and atomic-publish pattern.
- Parameters: `Tock On`, `Tock Mode`, `Tock Sample`, `Tock Level`, `Tock Tune`, `Tock Start`, `Tock Decay`, `Tock Tone`.

### 4.4 Envelopes

Both envelopes use the same `BreakpointEnvelope` model:

- A list of points (time in ms, value 0..1) with a curvature value per segment (-1 concave, 0 linear, +1 convex).
- The first point is fixed at time 0; the last point marks the end of the envelope.
- **Amp envelope:** default shape is a "kick + bass" curve: instant attack, short punch hold, quick drop, rise back into a sustained body (the bass), then release. Optional sustain point holds while the note is held.
- **Knobs and points edit the same envelope.** Each envelope panel has 5 knobs that move its main points (amp: Attack, Hold, Decay, Sustain, Release; pitch: Amount, Attack, Decay, Curve, End). Dragging points in the display and turning knobs change the same model; extra points added by hand keep their position relative to the nearest main point.
- **Pitch envelope:** maps 0..1 to a pitch offset in semitones above the settling pitch (`Pitch Env Amount`, default +48 st). Default shape is a very fast exponential fall.
- The UI edits a copy of the model. When the user changes it, the UI builds a new immutable `EnvelopeShape` and publishes it to the audio thread with an atomic pointer swap.
- Envelope shapes are part of the plugin state, not automatable parameters. Global controls (`Amp Env Time Scale`, `Pitch Env Amount`, `Pitch Env Time Scale`) are automatable.

### 4.5 Punch / Tail balance

- Splits the voice output in time into a **punch** region (from note-on to the split point) and a **tail** region (after it).
- `Punch Tail Balance` (-100 to +100): negative favours the punch, positive the tail, 0 leaves both untouched. Implemented as two gains applied to the regions.
- `Punch Tail Split` (ms after note-on): where the punch ends and the tail begins, shown as a vertical marker on the amp envelope display. The gain change crossfades over a few milliseconds to avoid clicks.
- Both are automatable parameters.

### 4.6 FX chain (main extension point)

The effects system follows the "one slot per effect type" model used by Serum: each effect type exists exactly once, the user can enable it, bypass it and drag it to any position in the chain.

Why this model: every effect has a fixed set of parameters with fixed IDs, so host automation and presets stay valid no matter how the user reorders the chain. See [ADR 0003](decisions/0003-fx-chain-model.md).

Building blocks:

```cpp
// src/dsp/fx/Effect.h  (illustrative, not final code)
struct EffectDescriptor {
    std::string_view id;          // stable, never changes: "distortion", "eq", "stereo"
    std::string_view displayName;
    int version;                  // bumps when parameters change, for migrations
};

class Effect {
public:
    virtual ~Effect() = default;
    virtual const EffectDescriptor& descriptor() const = 0;
    virtual void prepare(const juce::dsp::ProcessSpec&) = 0;   // may allocate
    virtual void reset() = 0;                                  // no allocation
    virtual void process(juce::dsp::ProcessContextReplacing<float>&) = 0; // real-time safe
    virtual int latencySamples() const { return 0; }
};
```

- **`EffectRegistry`** holds one factory function per effect type plus its parameter-layout function. Each effect registers itself with a single line in `src/dsp/fx/RegisterEffects.cpp`. Nothing else in the codebase lists effects by name.
- **Parameters** for an effect are declared by the effect's own `createParameters()` function and are prefixed with the effect ID (`distortion.drive`, `eq.low.gain`). `fcm_core` asks the registry for all parameter groups when building the layout.
- **`EffectChain`** owns one instance of every registered effect and an order array. The order is stored as a lock-free packed value (e.g. up to 16 slot indices in an atomic) written by the message thread and read once per block by the audio thread. Reordering never allocates and never pauses audio.
- **Per-slot common controls** are added automatically by the chain for every effect: `<id>.enabled`, `<id>.mix` (dry/wet), and the slot's position.
- **Latency:** the chain reports the sum of enabled effects' latency to the host whenever it changes.
- **Oversampling:** effects that create harmonics (distortion) run inside `juce::dsp::Oversampling`. The factor (1x/2x/4x/8x, 2x default) is one global `Quality` parameter in the Global panel; each effect owns its oversampler and reads that factor in `prepare()`.

Adding an effect is documented step by step in [ADDING_AN_EFFECT.md](ADDING_AN_EFFECT.md).

MVP effects:

| ID | Effect | Key parameters |
|---|---|---|
| `distortion` | Distortion with selectable type: soft clip (tanh), hard clip, overdrive (asymmetric), foldback, bitcrush (bit depth + sample-rate reduction) | Type, Drive, Pre-gain, Post-gain, Tone |
| `eq` | 3-band parametric EQ: low shelf, peak, high shelf | Freq, Gain, Q per band |
| `stereo` | Multiband stereo: 3 bands split by Linkwitz-Riley crossovers, mid/side width per band | Crossover 1/2, Width per band, Mono-below toggle |

### 4.7 Output stage

- `Output Gain` followed by a fixed safety soft clipper/limiter that cannot be reordered or removed. It protects speakers from the very high gain distortion settings typical of frenchcore. It can be set to "off" by advanced users.
- The limiter is a soft clipper: untouched below 0.8, bending smoothly toward (never above) 1.0 beyond that. Exact formula in [voice-note-handling](specs/voice-note-handling.md) §4.6.

### 4.8 Parameter hand-off from `fcm_core` to `fcm_dsp`

`fcm_dsp` receives all voice and global parameters in a plain `EngineParameters` struct, set once per block with `Engine::setParameters`. `fcm_core::EngineParameterReader` fills it from the APVTS raw parameter pointers and is the only place that maps parameter IDs to DSP fields. See [ADR 0005](decisions/0005-engine-parameter-handoff.md).

## 5. State, parameters and presets

- **Automatable parameters** live in a `juce::AudioProcessorValueTreeState` (APVTS). Parameter IDs are defined in one place (`src/core/ParameterIds.h` plus each effect's own parameter function) and are **never renamed or reused** once released.
- **Non-parameter state** lives in child nodes of the same `ValueTree`:
  - `Wavetable`: source name, embedded audio (base64 or binary blob).
  - `AmpEnvelope`, `PitchEnvelope`: point lists.
  - `FxChain`: order of effect IDs.
- **Versioning:** the root state node carries `stateVersion`. Loading older state runs migrations in `src/core/StateMigrations.cpp`. Each effect also carries its descriptor `version`.
- **Presets** are the same state serialized to an `.fcmpreset` XML file. Factory presets are embedded. User presets go to the platform's user documents folder (`Frenchcore Machine/Presets`).
- Unknown effect IDs or parameters in a preset (e.g. a preset made with a newer version) are ignored with a warning, never a crash.

## 6. Threading and real-time rules

Three threads matter:

| Thread | Does | Never does |
|---|---|---|
| Audio thread | `processBlock`: DSP only | allocate, free, lock, file I/O, log, call into UI |
| Message thread | UI, parameter changes from UI, preset load, publishing new shapes | long blocking work |
| Background worker | Decode WAV files, build wavetable mipmaps | touch the audio objects directly |

Patterns used:

- **Atomic pointer swap + deferred release** for large immutable data (wavetables, envelope shapes): the producer builds a new object, publishes it with an atomic store, and the old object is handed to a release pool that frees it on the message thread later.
- **Atomic packed values** for small state (FX order).
- **APVTS raw parameter pointers** (`std::atomic<float>*`) are read once per block, and parameters that cause clicks are smoothed (`juce::SmoothedValue`).
- **Debug check:** in debug builds a scoped guard flags any allocation on the audio thread.

## 7. User interface

- Functional first; visual design will be done later and must not require structural changes. All drawing goes through a custom `LookAndFeel` so restyling is a separate task.
- Resizable editor with a fixed aspect ratio, laid out with `juce::FlexBox`/`Grid` relative to the window size.
- Main areas, as agreed in [UI_LAYOUT.md](UI_LAYOUT.md): preset bar, two oscillator panels, amp and pitch envelope panels, tock, punch/tail balance, global, and the FX rack. LFOs (post-MVP) and "Other" have reserved space.
- The UI never calls DSP code. It changes parameters through APVTS attachments and non-parameter state through `fcm_core` APIs.
- A JUCE 9 WebView-based UI is a possible future option for the visual redesign. It is not used in the MVP.

## 8. Testing strategy

- Unit tests (Catch2) for every DSP block: frequency and level checks, no NaN/Inf, silence in gives silence out where expected, reset is deterministic.
- State tests: save then load reproduces identical state; old state versions migrate.
- FX chain tests: every order permutation of MVP effects processes without allocation and produces finite output.
- pluginval at strictness 10 on every CI build.
- Manual test checklist per release in [TESTING.md](TESTING.md).

## 9. Build and continuous integration

- `cmake --preset <platform>` then `cmake --build`. Details in [BUILDING.md](BUILDING.md).
- GitHub Actions runs on every push and PR: configure, build (Release), run unit tests, run pluginval, upload plugin binaries as artifacts. Tagged releases attach zipped binaries to a GitHub Release.
- Code signing and notarization (macOS) are out of MVP scope; unsigned builds are documented.

## 10. Extension points summary

| You want to add | Where | Docs |
|---|---|---|
| A new effect | `src/dsp/fx/<name>/` + one line in `RegisterEffects.cpp` | [ADDING_AN_EFFECT.md](ADDING_AN_EFFECT.md) |
| A new distortion type | `src/dsp/fx/distortion/` waveshaper table | spec in `docs/specs/` |
| A new envelope target | `fcm_dsp` modulation target + state node | spec in `docs/specs/` |
| A new factory wavetable or preset | `resources/` | [CONTRIBUTING.md](../CONTRIBUTING.md) |

## 11. Decision records

Significant decisions are written as Architecture Decision Records in [docs/decisions/](decisions/). Changing a decision means adding a new ADR that supersedes the old one, not editing history.

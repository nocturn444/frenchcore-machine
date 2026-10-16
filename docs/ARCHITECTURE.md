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
- `plugin` wires everything together and is the only place that knows about the host.

How this is built: the four libraries are CMake `INTERFACE` libraries that carry their sources and include paths. Each final binary (the plugin, each test executable) compiles them together with the JUCE modules it links, because JUCE modules must be linked `PRIVATE` by the final target and must not be compiled twice into one binary. The `fcm_dsp` boundary is enforced by the `fcm_dsp_tests` executable, which links only `juce_dsp`: a dsp file that includes UI, APVTS or plugin headers fails to build there.

### Third-party dependencies

| Dependency | Version | Licence | Used for |
|---|---|---|---|
| JUCE | 9.0.4 | AGPLv3 (or commercial) | Framework, plugin wrappers, UI, DSP helpers |
| Catch2 | 3.16.1 | BSL-1.0 | Unit tests (not shipped) |
| CPM.cmake | 0.43.2 (vendored in `cmake/`) | MIT | Dependency fetching at configure time |
| pluginval | 1.0.4 | GPLv3 | Plugin validation in CI only (not shipped, not linked) |

## 4. Signal flow

```
MIDI ──► NoteHandler (mono, last-note priority, optional glide)
              │ pitch, gate, velocity
              ▼
        ┌──────────────────────────────────────────────┐
        │ Voice (one mono stem per source)             │
        │  Pitch envelope ─► pitch of Osc 1 and Osc 2  │
        │  Osc 1, Osc 2, Noise  × Amp env × Punch/Tail │
        │  Tock × Punch/Tail (× Amp env if Amp Env on) │
        └──────┬──────────┬──────────┬──────────┬──────┘
               ▼          ▼          ▼          ▼
        Mixer: per source Destination (Main / Bus A / Bus B), Mute, Solo
               │                 │                  │
               ▼                 ▼                  ▼
        Bus A rack        Bus B rack        (direct, delayed to match)
        region, level     region, level             │
               └────────────┬────┴──────────────────┘
                            ▼
        Main rack (user-ordered effects, each with On and Mix)
                            ▼
        Output gain ─► Safety limiter (soft clip, always last, not reorderable)
                            ▼
                        Host output
```

Version 0.2 replaced the single FX chain with this mixer and three racks ([ADR 0010](decisions/0010-effect-instances-buses-and-mixer.md), [mixer-and-buses](specs/mixer-and-buses.md)). Until stage 2 of the 0.2 plan is merged, the code still has the 0.1 flow (one mono sum into one chain).

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
- Each oscillator has: On, Wavetable (a choice; nine factory tables, [wavetable-factory](specs/wavetable-factory.md), [ADR 0008](decisions/0008-wavetable-selection-and-publishing.md)), Level, Pitch (semitones), Octave, Fine tune (cents), Position, Phase (start phase at every note) and its own wavetable. Ranges and defaults in [oscillator-wavetable](specs/oscillator-wavetable.md). Both follow the same pitch envelope and note pitch.
- A wavetable is a sequence of frames of 2048 samples each (Serum/Vital-compatible WAV). Up to 256 frames.
- `Position` selects/interpolates between frames.
- Anti-aliasing: on load, each frame is converted to a set of band-limited mipmaps (one per octave, built with FFT by zeroing harmonics above Nyquist). Playback picks the mipmap for the current frequency and interpolates linearly between samples and between adjacent frames.
- Loading and mipmap generation happen on a background thread. The finished `WavetableData` is published to the audio thread with an atomic pointer swap; the old one is released on the message thread (see section 6).
- A user wavetable is embedded in the plugin state (a `UserWavetable1`/`UserWavetable2` node with the display name and the frames as base64 floats) so projects survive moving files. The file path is deliberately not stored ([wavetable-user](specs/wavetable-user.md), section 12).

### 4.3 Tock (click layer with optional samples)

- Optional short transient (the "tock"). In 0.1 it is mixed before the amp envelope; from 0.2 an `Amp Env` switch decides (off for new sounds, on for 0.1 states), and the Tock has its own mixer channel ([tock](specs/tock.md) section 8: trimming, Detect, Pogo, 10-second samples). Two sources, selected with `Tock Mode`:
  - **Synth:** a generated click (short noise burst or pitched blip) with its own decay and filter.
  - **Sample:** a factory tock or a user-loaded WAV. User samples are embedded in the plugin state like wavetables, and loading follows the same background-load and atomic-publish pattern.
- Parameters: `Tock On`, `Tock Mode`, `Tock Sample`, `Tock Level`, `Tock Tune`, `Tock Start`, `Tock Decay`, `Tock Tone` (details, ranges and the choices made in [tock](specs/tock.md)). The Tock is added to the oscillator sum before the amp envelope, and the selected sample is published to the voice like a wavetable ([ADR 0008](decisions/0008-wavetable-selection-and-publishing.md)).

### 4.3b Noise oscillator (version 0.2)

- A third source like Serum's noise oscillator ([noise-oscillator](specs/noise-oscillator.md)): eight generated noises or a user WAV (`UserNoiseSample` node), Level, Pitch, Key Track, One Shot, Phase, Random. It follows the amp envelope and the Punch/Tail levels but not the pitch envelope, and has its own mixer channel. The selected sample is published like the Tock's ([ADR 0008](decisions/0008-wavetable-selection-and-publishing.md)). Code in `src/dsp/noise/`.

### 4.4 Envelopes

Details, ranges and tests in [envelope-breakpoint](specs/envelope-breakpoint.md); the decision record is [ADR 0006](decisions/0006-envelope-model-and-publishing.md).

Both envelopes use the same `BreakpointEnvelope` model:

- A list of points (time in ms, level 0..1, at most 32, at most 2000 ms in total) with a curvature value per segment (-1 fast first, 0 linear, +1 slow first). Points are flagged **main** (the ones the knobs move) or **extra** (added by hand).
- The first point is fixed at time 0; the last point marks the end of the envelope. On the amp envelope the last point is also the sustain level.
- **Amp envelope:** default shape is "Kick + Bass": 1 ms attack, 25 ms punch, quick drop to 40 % by 80 ms, rise to a 70 % body by 140 ms held while the key is down. Releasing the key fades the sound out linearly from where it is over the `Release` time (default 30 ms). A held level of 0 makes a one-shot that ends by itself.
- **Knobs and points edit the same envelope.** Each envelope panel has 5 knobs that move its main points (amp: Attack, Hold, Decay, Sustain, Release; pitch: Amount, Attack, Decay, Curve, End). Dragging points in the display and turning knobs change the same model; extra points keep the same fraction of the section they sit in when a knob changes the section's length.
- **Pitch envelope:** maps 0..1 to a pitch offset in semitones above the settling pitch (`Pitch Env Amount`, default +48 st). Default shape ("Classic Drop") is a fast exponential fall that lands exactly on the note in 90 ms. The pitch envelope ignores note-off.
- The editable model is a plain value type (`BreakpointEnvelope`, owned by `core::EnvelopeState`). After every edit it builds an immutable, fixed-size `EnvelopeShape` and publishes it to the audio thread through `PublishedPointer` (hazard-pointer deferred release, ADR 0006). The engine acquires the current shapes once per block; `EnvelopeRenderer` plays them per sample.
- Envelope shapes are part of the plugin state (child nodes `AmpEnvelope` and `PitchEnvelope`), not automatable parameters. Global controls (`Amp Env Time Scale`, `Pitch Env Amount`, `Pitch Env Time Scale`) are automatable; time scale is read at each note-on.
- Shape presets (`EnvelopePresets`) are a data table per envelope kind; the first is the default.
- **Tempo sync** ([ADR 0007](decisions/0007-envelope-tempo-sync.md)): a shape may remember the tempo its times were written at (`syncRefBpm`); at each note-on the voice plays it scaled by reference / current tempo. The current tempo is the host tempo in a plugin, the audition strip's BPM in the Standalone, 120 otherwise; it reaches the engine in `EngineParameters` and the editor through `core::TempoState`. Knobs show musical figures (`MusicalTime`) while synced.

### 4.5 Punch / Tail split and levels

Details and tests in [voice-punch-tail](specs/voice-punch-tail.md).

- Splits the voice output in time into a **punch** region (from note-on to the split point) and a **tail** region (after it).
- `Punch Tail Split` (ms after note-on, default 80): where the punch ends and the tail begins, shown as a dashed vertical marker on the amp envelope display. The gain changes with a 4 ms smoothstep centred on the split, counted from each kick's own note-on.
- `Punch Level` and `Tail Level` (dB, default 0) are the gains of the two regions, applied after the amp envelope.
- All three are automatable parameters and are smoothed over 10 ms.

### 4.6 FX chain (main extension point)

> **Version 0.2:** [ADR 0010](decisions/0010-effect-instances-buses-and-mixer.md) supersedes the "one slot per effect type" model below: every effect type has four copies with fixed IDs (`distortion`, `distortion2` ... `distortion4`), and three racks (Bus A, Bus B, Main) each hold up to eight of them in a user order, with add, remove and duplicate. One rack works like the chain described here (per-slot dry compensation and mix), padded to a constant latency. [ADR 0011](decisions/0011-effect-interface-additions.md) adds `setContext(EffectContext)` (the note pitch, for Key Track), `ParameterSpec::shownWhen`, `EffectDescriptor::defaultMix` and raises the parameter limit to 48. The 0.2 effects are Filter, Compressor (with Multiband), Transient shaper and Reverb ([specs](specs/)). This section describes 0.1 until stage 2 of the 0.2 plan is merged.

The effects system follows the "one slot per effect type" model used by Serum: each effect type exists exactly once, the user can enable it, bypass it and drag it to any position in the chain.

Why this model: every effect has a fixed set of parameters with fixed IDs, so host automation and presets stay valid no matter how the user reorders the chain. See [ADR 0003](decisions/0003-fx-chain-model.md).

Building blocks (the code is in `src/dsp/fx/`; the full description is [fx-chain](specs/fx-chain.md), the decisions are [ADR 0003](decisions/0003-fx-chain-model.md) and [ADR 0009](decisions/0009-effect-parameters-and-chain-latency.md)):

```cpp
// src/dsp/fx/Effect.h (shortened)
struct ParameterSpec { /* idSuffix, name, kind (toggle/choice/integer/number), range, step, default, skew, unit, choices */ };
struct EffectDescriptor { std::string_view id; std::string_view displayName; int version; };

class Effect {
public:
    virtual const EffectDescriptor& descriptor() const = 0;      // stable id: "distortion", "eq", "stereo"
    virtual std::span<const ParameterSpec> parameters() const = 0; // plain data, no APVTS in fcm_dsp
    virtual void prepare(const EffectSpec&) = 0;                  // may allocate
    virtual void reset() noexcept = 0;
    virtual void setParameters(std::span<const float> values) noexcept = 0; // once per block
    virtual void setOversampling(int factor) noexcept;            // when Quality changes
    virtual void process(juce::AudioBuffer<float>&) noexcept = 0; // real-time safe, in place
    virtual int latencySamples(int oversamplingFactor) const noexcept;
};
```

- **`EffectRegistry`** holds one factory function per effect type. Each effect registers itself with a single line in `src/dsp/fx/RegisterEffects.cpp`, whose order is the default chain order and the effect's index. Nothing else in the codebase lists effects by name.
- **Parameters** for an effect are described by its `parameters()` (plain data) and prefixed with the effect ID (`distortion.drive`, `eq.low.gain`). `fcm_core` turns the specs of every registered effect into APVTS parameters when it builds the layout, and `EngineParameterReader` hands the values to the chain as a flat array in spec order.
- **`EffectChain`** owns one instance of every registered effect and an order. The order is a packed value (4 bits per position, up to 16 effects) in one `std::atomic<uint64_t>`, written by the message thread (`core::FxOrderState`, also saved as the `FxChain` node) and read once per block by the audio thread. Reordering never allocates and never pauses audio.
- **Per-slot common controls** are added by the layout code for every effect: `<id>.enabled` (default off) and `<id>.mix` (dry/wet, default 100 %). A slot that is off and faded out is skipped.
- **Latency:** every slot delays its dry signal by the effect's latency for the current Quality whether it is on, bypassed or mixed, so timing never changes when something is toggled, mixed or reordered. The chain reports the sum over all registered effects to the host whenever Quality changes.
- **Oversampling:** effects that create harmonics (distortion) run inside `juce::dsp::Oversampling`. The factor (1x/2x/4x/8x, 2x default) is one global `Quality` parameter in the Global panel (`global.quality`); the chain tells each effect through `setOversampling`.
- **Where it runs:** the voice renders mono, which is copied to the output channels; the chain runs on the whole block; then the output gain and the safety limiter run per sample on every channel.

Adding an effect is documented step by step in [ADDING_AN_EFFECT.md](ADDING_AN_EFFECT.md).

MVP effects:

| ID | Effect | Key parameters |
|---|---|---|
| `distortion` | Distortion with selectable type: soft clip (tanh), hard clip, overdrive (asymmetric), foldback, bitcrush (bit depth + sample-rate reduction) | Type, Drive, Pre-gain, Post-gain, Tone |
| `eq` | 4-band parametric EQ: each band a bell, shelf or cut | On, Type, Freq, Gain, Q per band |
| `stereo` | Multiband stereo: 3 bands split by Linkwitz-Riley crossovers, mid/side width per band | Crossover 1/2, Width per band, Mono-below toggle |

### 4.7 Output stage

- `Output Gain` followed by a fixed safety soft clipper/limiter that cannot be reordered or removed. It protects speakers from the very high gain distortion settings typical of frenchcore. It can be set to "off" by advanced users.
- The limiter is a soft clipper: untouched below 0.8, bending smoothly toward (never above) 1.0 beyond that. Exact formula in [voice-note-handling](specs/voice-note-handling.md) §4.6.

### 4.8 Parameter hand-off from `fcm_core` to `fcm_dsp`

`fcm_dsp` receives all voice and global parameters in a plain `EngineParameters` struct, set once per block with `Engine::setParameters`. `fcm_core::EngineParameterReader` fills it from the APVTS raw parameter pointers and is the only place that maps parameter IDs to DSP fields. See [ADR 0005](decisions/0005-engine-parameter-handoff.md).

## 5. State, parameters and presets

- **Automatable parameters** live in a `juce::AudioProcessorValueTreeState` (APVTS). Parameter IDs are defined in one place (`src/core/ParameterIds.h` plus each effect's own parameter function) and are **never renamed or reused** once released.
- **Non-parameter state** lives in child nodes of the same `ValueTree`:
  - `UserWavetable1`, `UserWavetable2`: display name and embedded frames (base64), see [wavetable-user](specs/wavetable-user.md).
  - `UserTockSample`: display name, sample rate and the embedded sample (base64), see [tock](specs/tock.md).
  - `AmpEnvelope`, `PitchEnvelope`: point lists.
  - `FxChain`: order of effect IDs (state version 1; replaced in version 2 by `FxRacks`: three `Rack` children with ordered instance IDs, [mixer-and-buses](specs/mixer-and-buses.md)).
  - `UserNoiseSample` (version 0.2): like `UserTockSample`, see [noise-oscillator](specs/noise-oscillator.md).
- **Versioning:** the root state node carries `stateVersion`, always written on save (`core::kStateVersion`, currently 1; version 0.2 makes it 2 with a migration that turns the 0.1 effect order into the Main rack and keeps every 0.1 sound, [ADR 0010](decisions/0010-effect-instances-buses-and-mixer.md)). A missing or damaged value counts as version 1. Loading older state runs the steps of `stateMigrations()` in `src/core/StateMigrations.cpp` in order (the list is empty while only version 1 exists); a state whose version is newer than this build is still loaded: the controls this build knows are applied and the rest is ignored. Each effect also carries its descriptor `version`.
- **One load path** (`core::ProjectState`, [project-state](specs/project-state.md)): it gathers the parameter tree, both envelopes, the effect order and the user wavetable and Tock nodes, and offers `capture()` and `restore()` (and the XML form). The processor, the presets and the tests all use it. A saved state describes everything: a parameter it does not mention becomes its default.
- **Loading is always sanitized** (`core::loadState` in `src/core/StateLoader.cpp`, spec [state-safe-loading](specs/state-safe-loading.md)): the root type must match; unknown or value-less `PARAM` entries are dropped; every parameter value that is not a plain finite number becomes the parameter's default, and every other value is clamped to the parameter's range and snapped to its step. `EngineParameterReader` and the oscillator repeat these guards as a second layer, so a bad value can never index outside memory on the audio thread. State blobs above `core::kMaxStateBytes` (32 MiB) are rejected before parsing.
- **Presets** ([presets](specs/presets.md)) are the same state wrapped in a small XML file with a name and a `presetFormat` version (`.fcmpreset`, `core::PresetFile`). Factory presets are a table in code (`core::FactoryPresets`: parameter overrides plus envelope shape names) applied over the defaults, so they follow new controls and need no embedded files. User presets go to the platform's user documents folder (`Frenchcore Machine/Presets`) through `core::PresetLibrary`; `core::PresetManager` presents factory then user presets as one list with next/previous and save. Preset files are untrusted input (§12): size cap, nesting-depth scan before XML parsing, strict file-name whitelist.
- Unknown effect IDs or parameters in a preset (e.g. a preset made with a newer version) are ignored with a warning, never a crash.

## 6. Threading and real-time rules

Three threads matter:

| Thread | Does | Never does |
|---|---|---|
| Audio thread | `processBlock`: DSP only | allocate, free, lock, file I/O, log, call into UI |
| Message thread | UI, parameter changes from UI, preset load, publishing new shapes | long blocking work |
| Background worker | Decode WAV files, build wavetable mipmaps | touch the audio objects directly |

Patterns used:

- **Atomic pointer swap + deferred release** for large immutable data (wavetables, envelope shapes): the producer builds a new object and publishes it with an atomic exchange (`PublishedPointer`); the audio thread records the pointer it reads as a hazard pointer, and the old object is freed on the publishing thread once it is no longer that one ([ADR 0006](decisions/0006-envelope-model-and-publishing.md)).
- **Atomic packed values** for small state (FX order; from 0.2 one packed value per rack, [ADR 0010](decisions/0010-effect-instances-buses-and-mixer.md)).
- **APVTS raw parameter pointers** (`std::atomic<float>*`) are read once per block, and parameters that cause clicks are smoothed (`juce::SmoothedValue`).
- **Debug check:** in debug builds a scoped guard flags any allocation on the audio thread.

## 7. User interface

- Functional first; visual design will be done later and must not require structural changes. All drawing goes through a custom `LookAndFeel` so restyling is a separate task.
- Resizable editor with a fixed aspect ratio. The editor is drawn on one fixed design grid (1500 points wide) and scaled as a whole to the window, so it never rearranges ([ui-grid-layout](specs/ui-grid-layout.md)). Styling lives in `ui/Style.h` and `ui::FcmLookAndFeel`.
- Main areas, as agreed in [UI_LAYOUT.md](UI_LAYOUT.md): preset bar, two oscillator panels, amp and pitch envelope panels, tock, punch/tail levels, global, and the FX rack. LFOs (post-MVP) and "Other" have reserved space. From 0.2 the LFO box holds the Noise panel and the Effects box has tabs Mixer / Bus A / Bus B / Main; the window keeps its size.
- The UI never calls DSP code. It changes parameters through APVTS attachments and non-parameter state through `fcm_core` APIs.
- A JUCE 9 WebView-based UI is a possible future option for the visual redesign. It is not used in the MVP.
- **Standalone-only audition strip** (on-screen keyboard and auto-play loop, [spec](specs/standalone-audition.md)). It exists only when `AudioProcessor::wrapperType` is `wrapperType_Standalone`. Its settings are not parameters and not state: `core::AuditionState` holds atomics and a lock-free `dsp::NoteQueue` (message thread to audio thread), and the audio thread turns them into MIDI events (`dsp::AuditionSequencer` for the loop) inside a pre-allocated MIDI buffer. `MidiKeyboardState` (which locks) stays on the message thread.

## 8. Testing strategy

- Unit tests (Catch2) for every DSP block: frequency and level checks, no NaN/Inf, silence in gives silence out where expected, reset is deterministic.
- State tests: save then load reproduces identical state; old state versions migrate.
- FX chain tests: every order permutation of MVP effects processes without allocation and produces finite output.
- pluginval at strictness 10 on every CI build.
- Manual test checklist per release in [TESTING.md](TESTING.md).

## 9. Build and continuous integration

- `cmake --preset <platform>` then `cmake --build`. Details in [BUILDING.md](BUILDING.md).
- GitHub Actions runs on every push and PR: configure, build (Release), run unit tests, run pluginval, upload plugin binaries as artifacts. Tagged releases attach zipped binaries to a GitHub Release.
- **Supply-chain pinning policy.** Nothing that runs in CI or is built into the plugin is referenced by a movable name:
  - Every GitHub Action in `.github/workflows/` is pinned to the full commit SHA of a release, with the version in a trailing comment. The workflow token is read-only (`permissions: contents: read`).
  - `cmake/Dependencies.cmake` pins JUCE and Catch2 to the full commit SHA of their release tag (version in a comment), still fetched with `GIT_REPOSITORY` (archive URLs are blocked in the cloud environment). `GIT_SHALLOW` is off for them because shallow clones by SHA are not reliable everywhere.
  - Downloaded tools are checked against a SHA-256 stored in the repository before they are extracted or run (pluginval, one hash per platform zip in the workflow matrix).
  - The Windows build sets up MSVC with its own step (vswhere and `vcvarsall.bat`), not a third-party action.
  - To update a pin, change the tag, look up the new commit SHA (`git ls-remote --tags <repo>`), recompute hashes, and update the version table above. Dependabot proposes weekly updates for the Actions.
- Security reports go through GitHub private vulnerability reporting, see [SECURITY.md](../SECURITY.md).
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

## 12. Untrusted input

Everything that comes from outside the process is untrusted: the state blob a host hands to `setStateInformation`, and (when they arrive) user WAV wavetables and `.fcmpreset` files, which people share. A hostile or merely damaged file must never crash the host, hang it, or write outside our own folders.

Rules for any code that reads such data:

- **Size limits first.** Check the byte size before reading or parsing (state blobs: `kMaxStateBytes`). Wavetable WAVs: cap file size, channel count, sample rate and frame count (at most 256 frames of 2048 samples, [oscillator-wavetable](specs/oscillator-wavetable.md)). Presets: cap file size and the number of nodes and properties.
- **Shape limits before parsing XML.** JUCE's XML parser and `ValueTree::fromXml` recurse once per level and expand DTD entities, so the host state blob and preset files go through `core/UntrustedXml.h` first: no `<!DOCTYPE` or `<!ENTITY`, at most `kMaxUntrustedXmlDepth` (64) levels. The host blob is also read by `parseHostStateBlob` instead of JUCE's `getXmlFromBinary`: JUCE's header, a declared length that fits, and valid UTF-8 are checked before any `juce::String` is built.
- **Never trust a stored size.** Audio embedded in a state (user wavetables, the user Tock sample) is decoded by `core::decodeStoredFloats`, which checks the byte count written in front of the base64 text against the allowed maximum and the text's real length before anything is allocated (`juce::MemoryBlock::fromBase64Encoding` resizes to whatever the text claims).
- **Validate every number.** Reject or replace non-finite samples (NaN, infinity) and out-of-range values; never trust a value to be a valid index, size or enum. Parse numbers with the strict reader in `StateLoader.cpp`, not with `getDoubleValue`, which turns `nan` and `inf` into real NaN and infinity.
- **Decode off the audio thread.** Parsing, decoding and the wavetable mipmap build run on a background or message thread and are published by atomic pointer swap ([section 6](#6-threading-and-real-time-rules)). The audio thread only sees data that has already been validated.
- **Never write to a path taken from the data.** A preset or state may name a file, but nothing in it decides where we write. User presets are saved only inside the presets folder, with a file name built from sanitized characters (no separators, no `..`, no reserved names) and a length limit; existing files are not overwritten silently.
- **Unknown content is ignored, not fatal.** Unknown parameters, effect IDs and nodes are skipped; a newer `stateVersion` loads what is known.
- **Tests with hostile files.** Each loader ships Catch2 tests with corrupt, truncated, oversized and NaN-filled input, asserting no crash, no non-finite output and no allocation on the audio thread. The state loader's tests are in `tests/core/StateLoaderTests.cpp`.

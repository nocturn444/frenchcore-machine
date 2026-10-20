# Glossary

Plain-language explanations of the technical terms used in this project's documents.

| Term | Meaning |
|---|---|
| **ADR** | Architecture Decision Record. A short document recording an important technical decision and why it was made. |
| **AGPLv3** | The open-source license of this project. Anyone can use and change the code, but if they distribute a modified version they must publish its source code too. |
| **Aliasing** | Harsh, unwanted frequencies that appear when a digital sound contains frequencies too high for the sample rate. Avoided with band-limited wavetables and oversampling. |
| **APVTS** | JUCE's system for storing plugin parameters so the DAW can automate and save them. |
| **Attack / Hold / Decay / Sustain / Release** | The five stages of the volume envelope: how fast it rises, how long it stays at full level, how fast it falls, the level it keeps while the key is down, and how fast it fades after the key is let go. |
| **AU** | Audio Unit, Apple's plugin format (Logic Pro, GarageBand). macOS only. |
| **Audio thread** | The part of the program that computes audio in real time. If it waits for anything, you hear clicks or dropouts. |
| **Band-limited / mipmaps** | Pre-computed versions of a wavetable with high harmonics removed, one per pitch range, so high notes do not alias. |
| **Breakpoint envelope** | An envelope drawn as points connected by curves, like Serum's or Vital's. |
| **Bus** | A separate row of effects that some sources of the sound are sent to before they join the main row. Frenchcore Machine has Bus A, Bus B and Main (from 0.2). |
| **CI** | Continuous Integration. GitHub automatically builds and tests the plugin on Windows, macOS and Linux every time code changes. |
| **CMake / CPM** | The build tool that turns source code into the plugin, and the helper that downloads libraries automatically. |
| **Compressor** | An effect that turns loud parts down (and, in the OTT style, quiet parts up), making the level more even and the sound denser. |
| **DSP** | Digital Signal Processing: the math that generates and transforms sound. |
| **Flanger** | An effect that mixes the sound with a copy delayed by a few milliseconds and moves that delay, giving a "jet" sweep or, when still, a metallic tuned ring. |
| **Formant** | A resonance of the voice. Each vowel has its own set of formants; a formant filter imitates them so a sound seems to say a vowel. |
| **Glide** | Smooth pitch slide between consecutive notes (portamento). |
| **JUCE** | The C++ framework used to build the plugin for all formats and platforms. |
| **Key track** | When on, the MIDI note decides the pitch of the kick's tail. When off, the pitch is fixed. Some controls have their own key track (the EQ bands and the noise, from 0.2): they follow the note you play in the same way. |
| **LFO** | Low-frequency oscillator: a slow wave that moves something by itself, over and over (a filter, the volume). In Frenchcore Machine the effects' LFOs restart at every note. |
| **Linkwitz-Riley crossover** | A filter that splits audio into frequency bands that add back together cleanly. Used by the multiband stereo effect. |
| **Mixer** | The part of the plugin where each sound source (Osc 1, Osc 2, Noise, Tock) gets a volume, mute, solo and a destination bus (from 0.2). |
| **MVP** | Minimum Viable Product: the first version that is genuinely useful. |
| **OTT** | A famous free multiband compressor by Xfer Records (the maker of Serum), used to make sounds dense and bright. The Compressor's Multiband mode works in that style. |
| **Oversampling** | Processing audio internally at a higher sample rate to reduce aliasing from distortion. |
| **Phaser** | An effect that digs a few notches into the sound and sweeps them up and down, giving a swirling, metallic movement. |
| **pluginval** | A tool that stress-tests a plugin the way DAWs use it, to catch crashes before users do. |
| **Pogo** | A bend of the pitch of the Tock while it plays, up or down (the name comes from kick plugins such as Nimble Kick). |
| **PR (pull request)** | A proposed change to the code, reviewed before it is merged into the main branch. |
| **Punch / Tail** | The first moments of a kick (the hit) and the bass that rings after it. The Split time divides the kick between them, and a Punch Level and a Tail Level set the volume of each. |
| **Rack** | A row of effects that the sound goes through in order. Each bus has one (from 0.2). |
| **Release (envelope)** | The part of a sound that happens after you let go of the key. Here it is a short fade-out, so the length of the MIDI note decides how long the kick's tail rings. |
| **Semitone** | One step on a piano: 12 semitones make an octave. The pitch envelope's Amount is measured in semitones. |
| **Solo / Mute** | Mute silences one source; Solo silences every source except the soloed ones. |
| **Spec** | A short document describing a feature before it is built. |
| **Standalone** | The plugin running as its own application, without a DAW. |
| **State version / migration** | A number saved inside every project and preset. When a new version stores things differently, a migration converts old projects on load so they keep sounding the same. |
| **Transient** | The very first instant of a sound, the hit or click. A transient shaper is an effect that makes it stronger or softer. |
| **VST3** | Steinberg's plugin format, supported by almost every DAW on Windows, macOS and Linux. |
| **Wavetable** | A set of single-cycle waveforms (frames). Moving the position morphs between them. |
| **XY pad** | A square you drag a dot in: left-right sets one control and down-up another, at the same time. |

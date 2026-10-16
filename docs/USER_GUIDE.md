# User Guide

> This guide describes the plugin as it is. Each feature's section is kept up to date by the change that touches it.

## Installation

Download the zip for your system from the [Releases page](https://github.com/nocturn444/frenchcore-machine/releases), unzip it and copy the plugin into the folder your DAW scans. There is no installer. Each zip also has `LICENSE.txt` (the licence) and `README.txt` (what is in the zip and where the source code is).

| System | Format | Copy it to |
|---|---|---|
| Windows | VST3 (`Frenchcore Machine.vst3`) | `C:\Program Files\Common Files\VST3` |
| macOS | VST3 | `/Library/Audio/Plug-Ins/VST3` (all users) or `~/Library/Audio/Plug-Ins/VST3` (only you) |
| macOS | AU (`Frenchcore Machine.component`) | `/Library/Audio/Plug-Ins/Components` or `~/Library/Audio/Plug-Ins/Components` |
| Linux | VST3 | `~/.vst3` |

The **Standalone** app (the version that runs by itself) needs no installing: unzip it and open it from wherever you like.

Then rescan plugins in your DAW. The plugin is an **instrument**: put it on a MIDI track.

The plugins are **not signed** yet, so your system warns you the first time:

- **macOS** says the developer cannot be verified. Open System Settings, Privacy & Security, scroll down and press **Open Anyway** for it. If the DAW still refuses the plugin, open Terminal and run `xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Frenchcore Machine.vst3"` (and the same for the AU and the Standalone you installed), then rescan.
- **Windows** (SmartScreen) shows "Windows protected your PC" when you open the Standalone: press **More info**, then **Run anyway**. If a DLL such as `VCRUNTIME140.dll` is reported missing, install the "Microsoft Visual C++ Redistributable" (x64) from Microsoft.

**Linux**: the build needs a recent distribution, with glibc 2.38 or newer (Ubuntu 24.04, Fedora 39, Debian 13 or newer); on older ones it does not open. It also needs the ALSA, FreeType and Fontconfig libraries, which desktop distributions normally have (on Ubuntu: `libasound2t64`, `libfreetype6`, `libfontconfig1`).

**Checking a download** (optional): the release page has a `SHA256SUMS.txt` with the checksum of each zip, and each zip has a build provenance attestation signed by GitHub that proves it was built from this repository. With the [GitHub CLI](https://cli.github.com/): `gh attestation verify FrenchcoreMachine-Windows.zip --repo nocturn444/frenchcore-machine`.

Your own presets live in `Documents/Frenchcore Machine/Presets` (see [Presets](#presets)).

## Quick start: a kick in one minute

1. Load Frenchcore Machine as an instrument on a MIDI track, or open the Standalone app and use its on-screen keyboard (see [Standalone](#standalone-playing-without-a-daw)).
2. Play a MIDI note. You hear a kick: a sine (or the wavetable you choose) that starts four octaves higher and falls to the pitch of your note in a fraction of a second, with a punch and a bass body that you can reshape in the envelopes. That fast fall is what gives the kick its punch.
3. Play a lower or higher note to retune the kick, or turn **Key Track** off to make every note play the same pitch (the **Root Note**).
4. Make the note longer or shorter in the piano roll to control how long the tail rings.

Everything is on one screen (see [The window](#the-window)).

## The window

The whole plugin is one screen, laid out like this:

- **Top strip**: the preset bar (see [Presets](#presets)).
- **Left, top**: the two oscillators side by side. Each has a header (On, the Wavetable menu, a Load button), Pitch, Octave and Fine, the wavetable display, and the Position, Level and Phase knobs.
- **Left, middle**: the Amp Envelope and the Pitch Envelope side by side, each with its display, five knobs for the shape and a Time Scale knob.
- **Right**: the Effects, one strip each from top to bottom in the order of the sound.
- **Bottom row**: the LFO box (kept free for a later version), the Tock, Punch / Tail, Global, and the Other box, which for now only shows the plugin version, its licence (free software, AGPL-3.0, no warranty) and where its source code is.
- **Standalone only**: the keyboard and Auto-play strip at the very bottom.

You can resize the window. Everything scales together, so nothing moves or gets squeezed: the window only gets smaller or bigger, keeping its shape. Double-click any knob to bring it back to its default value.

## Standalone: playing without a DAW

The Standalone app (the version that runs by itself, without a DAW) has a strip at the bottom of the window so you can hear the kick right away. The VST3 and AU versions do not have it, because your DAW already gives you notes and tempo.

- **Keyboard.** Click the keys with the mouse, or play them with your computer keyboard: the letters `A W S E D F T G Y H U J K O L P ;` work like piano keys starting from C1 (the home row `A S D F G H J K L ;` are the white keys, the row above them are the black keys). `Z` moves the letters one octave down and `X` one octave up. The computer keys work as long as the window is active, except while you are typing a number in the BPM box.
- **Auto-play.** Switch it on and the kick repeats by itself, one per beat, so you can change the sound while it loops. **BPM** sets the tempo (60 to 300, default 200); drag it or type a number, and double-click it to go back to 200. Each note is held for 80 % of the beat, so the tail rings almost until the next kick.
- While Auto-play is on, pressing a key does not play a note: it chooses the pitch of the loop, which changes at the next kick. The loop starts on C1 until you choose another note.
- Auto-play and the BPM are not saved: every time you open the Standalone it starts with Auto-play off and 200 BPM.

## Oscillators and wavetables

There are two identical oscillators, **Osc 1** and **Osc 2**. Osc 1 is on and Osc 2 is off when you start.

| Control | What it does |
|---|---|
| **On** | Switches the oscillator on or off, without clicks. |
| **Wavetable** | Chooses the waveform family this oscillator plays, from the nine built-in ones (list below). |
| **Level** | How loud this oscillator is. |
| **Pitch**, **Octave**, **Fine** | Tune this oscillator against the kick's pitch, in semitones, octaves and cents (hundredths of a semitone). For example, set Osc 2 to Octave +1 to add a layer one octave up. |
| **Position** | Slides through the waveforms inside the wavetable, from the first (0) to the last (1), blending smoothly. Sine and Soft Triangle have only one waveform, so Position does nothing for them. |
| **Phase** | Where in its cycle the oscillator starts at every note. The same value every time means every kick starts exactly the same way. |

The built-in wavetables, in menu order:

| Wavetable | Character |
|---|---|
| **Sine** | The clean sine. The default. |
| **Soft Triangle** | A little brighter than a sine, still round. |
| **Warm Drive** | Sine towards a softly saturated sine as Position goes up. |
| **Hard Clip** | Sine towards a hard-clipped wave, close to a square. |
| **Wave Fold** | The sine folds back on itself more and more. |
| **Odd Stack** | Sine towards a square-like wave by adding odd harmonics. |
| **Saw Rise** | Sine towards a saw-like wave by adding all harmonics. |
| **Hardcore Punch** | An asymmetric, heavily driven sine: the aggressive bite. |
| **Rumble Core** | A biased, overdriven sine with a lot of even harmonics, for gritty body. |

Harder waves carry more energy than the sine at the same Level, so they sound louder: lower **Level** if needed. Changing the Wavetable while a kick is sounding changes the sound from the next moment on and can click, so set it up before you play. 

### The wavetable display

Under the Pitch, Octave and Fine strip of each oscillator a display shows what that oscillator plays: the wavetable's name, a faint stack of its waveforms and, in bright blue, the waveform at the current **Position**. Move the Position knob and the bright line moves with it. It only shows; there is nothing to click.

### Your own wavetable

Each oscillator has a **Load...** button in its header, next to the Wavetable menu. Pick a WAV file and, if it is usable, that oscillator switches to the **User** entry of its Wavetable menu and plays it. A line of text in the preset bar says what was loaded, or why a file was refused.

- A wavetable is a WAV whose length is a whole number of 2048-sample frames (1 to 256 frames, the Serum and Vital format), or a single cycle of 64 to 16384 samples, which is stretched to one frame. Only the first channel is used, and the sample rate does not matter.
- The sound is centred and scaled so its loudest point is full scale, keeping the level differences between frames.
- A file that cannot be used (not a WAV, too long, silent, damaged) is refused and the oscillator keeps what it had.
- The wavetable is saved **inside your project**, so it still plays if you move or delete the file. Only the file name is kept, to show it; the folder is not.
- Choosing **User** with nothing loaded plays the Sine. Each oscillator has its own.

## Tock

The Tock is a very short click or knock played at the start of every kick, on top of the oscillators. It is the attack that helps the kick cut through a mix. It is **off by default**; switch it on with **Tock On** (the last column of the controls).

| Control | What it does |
|---|---|
| **Tock On** | Turns the Tock on or off. |
| **Tock Mode** | **Synth** makes the click from scratch (a short tone burst plus a burst of noise). **Sample** plays one of the built-in tock sounds. |
| **Tock Sample** | The built-in sounds used in Sample mode: **Click** (a dry, tiny click), **Knock** (a short low thud), **Snap** (a quick noisy snap), **Tick** (a short bright tick). |
| **Tock Level** | How loud it is, from silent to +12 dB (default -12 dB). |
| **Tock Tune** | Its pitch, in semitones. In Synth mode it moves the tone of the click; in Sample mode it plays the sound faster or slower. |
| **Tock Start** | A delay of 0 to 20 ms between the note and the Tock. A small delay puts the click just after the start of the pitch drop. |
| **Tock Decay** | How fast it fades out, 2 to 200 ms (the time to fall by 60 dB). |
| **Tock Tone** | Dull to bright: at 0 the Tock is dark, at 1 it is wide open. |

The Tock follows the velocity, the Punch and Tail levels and the amp envelope like the rest of the kick. Start, Tune, Decay, Tone, Mode and Sample are read when a note starts, so changing them affects the next kick; Level changes at once. If the Tock sounds too soft, raise Start a little or shorten the amp envelope's attack, because the amp envelope also shapes the Tock. 

### Your own tock sample

The **Load sample...** button in the Tock box opens a WAV file. If it is usable, **Tock Sample** switches to its **User** entry and the Tock plays it in Sample mode (remember to switch Tock Mode to Sample).

- The file must be short: at most 2 seconds, at any sample rate from 8 kHz to 192 kHz. Only the first channel is used.
- It is centred, scaled to full scale and given a 2 ms fade-out at the end, so it never ends with a click.
- An unusable file is refused with a message in the line next to the button, and the Tock keeps what it had.
- The sample is saved inside your project; only its name is kept, not the folder.
- Choosing **User** with nothing loaded plays the first built-in sample (Click).

## Punch / Tail

A frenchcore kick has a **punch** (the hit at the start) and a **tail** (the bass that rings after it). This panel sets the volume of each part on its own, without redrawing the envelope.

- **Split** is the time, in milliseconds after the note starts, where the punch ends and the tail begins (default 80 ms). It only decides which part each level applies to. A dashed red line on the Amp Envelope display shows it.
- **Punch Level** sets the volume of the part before the split, from -48 dB (silence) to +12 dB, 0 dB by default.
- **Tail Level** sets the volume of the part after the split, with the same range.
- With both levels at 0 dB nothing changes, wherever the split is. The change from one part to the other takes about 4 ms, so it never clicks, and the split is counted from the start of each kick, also when you retrigger.

## Envelopes

Two envelopes shape every kick, and you can draw and tune both. Each has a display, five knobs and a menu for ready-made shapes. Everything you change is heard right away, also while a kick is playing (turn on Auto-play in the Standalone to hear it loop).

### Amp envelope (volume)

It is the volume of the kick over time. The default shape is **Kick + Bass**: it starts instantly (in 1 ms, so there is no click), holds a strong punch for 25 ms, drops quickly to 40 %, and rises again to a bass body at 70 % that stays for as long as the key is down. When you let go of the key the sound fades out (30 ms by default), so the length of the note in the piano roll sets the length of the tail.

| Knob | What it does |
|---|---|
| **Attack** | Time to reach full level (1 ms at the least, so the kick never starts with a click). |
| **Hold** | How long it stays at full level. |
| **Decay** | Time from the end of the hold to the sustain level. |
| **Sustain** | The level kept while the key is down (the last point of the line). At 0 the kick is a one-shot that ends by itself, whatever the note length. |
| **Release** | Fade-out time after you let go of the key. |

**Time Scale** (small slider under the knobs) makes the whole envelope longer or shorter: 2 doubles every time, 0.5 halves them. It applies from the next note.

### Pitch envelope

It is how far above the note's pitch the kick starts and how it falls onto the note. This fast fall gives the kick its punch.

| Knob | What it does |
|---|---|
| **Amount** | How many semitones above the note the sweep starts (0 to 96, default +48). |
| **Attack** | Time to rise to the full amount. At 0 the kick starts at the top. |
| **Decay** | Time to fall to the end level (default 90 ms). |
| **Curve** | How the fall bends: -1 falls fast first and then slowly, 0 is a straight line. |
| **End** | Where the fall lands, as a share of Amount. At 0 it lands exactly on the note; above 0 it stays above the note. |

The pitch envelope always finishes its fall, even if you let go of the key. It also has a **Time Scale**.

### Tempo sync

Each envelope has a **Sync** switch next to its **Load shape...** menu. With Sync on, the envelope follows the tempo and its milliseconds are replaced by musical figures:

- The time knobs (Attack, Hold, Decay and Release on the amp; Attack and Decay on the pitch) step through note values instead of milliseconds: `1/64, 1/32, 1/16T, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1 bar` (**T** is a triplet, **D** is dotted). The first step is `0` where the knob can be zero, or `Min` (the shortest time, 1 ms) where it cannot. Only figures that fit the knob are offered.
- The grid of the display is labelled with fractions of a bar (`1/16`, `1/8`, `1/4`...) instead of milliseconds, and the points you drag snap to 1/32 notes in time.
- The envelope **keeps its musical lengths when the tempo changes**: a Hold of 1/16 is a sixteenth note at any tempo. The change applies from the next note.
- The tempo is your DAW's tempo. In the Standalone it is the **BPM** control of the strip at the bottom. If the DAW does not give a tempo, 120 BPM is used.
- Turning Sync on or off does not change how the kick sounds. The knobs show the closest figure at first; the first time you turn a time knob, that time becomes an exact figure.
- Time Scale still works on top of Sync. The Punch / Tail **Split** stays in milliseconds.

The Sync switch is saved with the shape, but a DAW cannot automate it.

### Drawing on the display

- **Drag a point** to move it in time and level (in time it snaps to 1/32 notes when Sync is on). A point cannot go past its neighbours. The first point stays at time 0 (and at level 0 on the amp envelope).
- **Double-click on empty space** to add a point on the line, between the first and the last point (up to 32 points).
- **Double-click a small white point** to delete it. The big gold points are the ones the knobs move and cannot be deleted.
- **Drag a segment up or down** to bend it: dragging up makes the line bulge upward.
- The knobs and the points are the same line: turning a knob moves the gold points, and the white points you added stretch along with the section they are in.
- **Load shape...** opens a menu of ready-made shapes. For the amp: Kick + Bass, Short Punch and Long Tail. For the pitch: Classic Drop, Short Punch and Long Tail. Loading one replaces the current shape.

The shapes are saved with your project, so the kick comes back exactly as you left it. The two envelopes cannot be automated point by point, but Time Scale and Amount can.

## Mixer and buses

The **Effects** box has four tabs at its top: **Mixer**, **Bus A**, **Bus B** and **Main**. (A new window opens on Main.)

- **Mixer**: one strip for each sound of the kick (Osc 1, Osc 2 and the Tock; the Noise joins later in 0.2). Each strip has the sound's **Level** (the same control as on its own panel: turning one turns the other), **Destination** (**Main**, **Bus A** or **Bus B**, default Main), and **Mute** and **Solo**. Solo on any strip silences every strip that is not soloed. A sound that is switched off is drawn dimmed. Switching is smooth, with no clicks.
- **Bus A and Bus B** (on the right of the Mixer): each has a **Level** (-48 to +12 dB, default 0; -48 is silence) and **Apply To**: **Whole** (default), **Punch** or **Tail**. All of the sound goes into the bus; with Punch, the bus's effects are heard only in the punch part of the kick (before the Split time of the Punch / Tail box) and after it you hear what went in, untouched; with Tail it is the other way round. The change at the Split takes 4 ms. A reverb on a Punch bus therefore stops at the Split.
- **Bus A, Bus B and Main tabs**: each is a row of effects like the one described below: **+ Add effect**, **++**, **x**, drag to reorder. Bus A and Bus B end in Main; Main has its own row and then goes to the Output gain and the safety limiter.

Examples: send everything to **Bus A**, set **Apply To** to **Tail** and put a Distortion there, to distort only the tail; send **Osc 2** alone to **Bus B** and put an EQ there, to EQ only that layer.

The delay your DAW is told about (32 samples at the default Quality) never changes with what you route, switch on or add.

## Effects

After the oscillators and the Tock, the kick goes through a row of effects, which starts with Distortion, EQ and Stereo. They live in the **Effects** box on the right, and every one works the same way:

- **On**: each effect has its own switch, **off by default**, so the sound does not change until you switch one on.
- **Mix**: how much of the effect you hear against the untouched sound (0 to 100 %, default 100 %).
- **Order**: the effects can be dragged into any order. Each effect is a strip, in the order the sound goes through them from top to bottom; grab the handle bar at the top of a strip (the three lines and the name) and drag it up or down, and the others slide to make room. Its controls keep working with automation wherever it sits. The order is saved with the project.
- **Add, duplicate, remove**: the **+ Add effect** button at the end of the row opens a menu with every effect. A new effect goes at the end, switched on, with its default settings. Each strip has **++** to duplicate it (the copy goes right after it, with the same settings) and **x** to remove it. You can have up to four copies of each effect in all (a menu entry is greyed out when its four are in use) and up to eight effects in the row (the button is greyed out when it is full). The second Distortion is called **Distortion 2** in the window and in your DAW's automation list ("Distortion 2 Drive"). When the row is longer than the box, scroll it with the mouse wheel over a strip's name bar or its scroll bar. Removing an effect takes it out at once.
- Switching an effect on or off, changing its Mix or moving it never clicks and never shifts the timing of the kick.
- The effects work on both channels. Projects and presets from version 0.1 open with Distortion, EQ and Stereo in this row, in the order and with the settings they were saved with, and sound the same.
- They sit before the output gain and the safety limiter, so the limiter always protects the final sound. The effects work on both channels.

### Distortion

The first effect. Switch it on with **Distortion On** (the Distortion column of the controls).

| Control | What it does |
|---|---|
| **Type** | **Soft Clip**: smooth, warm saturation. **Hard Clip**: slices the wave flat, bright and aggressive. **Overdrive**: lopsided saturation that adds even harmonics, thicker and tube-like. **Foldback**: the wave folds back when it goes too far, metallic and buzzing. **Bitcrush**: fewer levels and fewer samples per second, gritty and digital. |
| **Drive** | How hard the sound is pushed into the distortion, 0 to 100 % (up to 48 dB of gain). Default 25 %. |
| **Pre-gain** | Extra gain before the distortion, -24 to +24 dB. |
| **Post-gain** | Volume after the distortion, -48 to +12 dB (default -6 dB): distortion makes things louder. |
| **Tone** | How bright the result is. 100 % (default) filters nothing; lower values take off the high end, down to a low-pass at 1 kHz. |
| **Bits** | Bitcrush only (shown only when Type is Bitcrush): bit depth, 2 to 16. |
| **Rate** | Bitcrush only (shown only when Type is Bitcrush): rate reduction, 1x (none) to 32x. |
| **Mix** | How much distorted sound you hear against the clean kick. |

The clipping types are oversampled according to **Quality** to avoid harsh aliasing; Bitcrush is not, because its roughness is the point. Changing **Type** while a kick rings can click, so set it before you play. With effects in the plugin, your DAW is told about a small delay (32 samples at the default Quality: the plugin keeps room for four copies of every effect, and every sound passes two rows of that size) that it compensates; nothing shifts when you switch an effect on or off.

### EQ

A parametric equaliser with four bands to shape the kick after (or before) the distortion. Switch it on with **EQ On**.

Every band is the same and free: you choose what it does, where and how much.

| Control | What it does |
|---|---|
| **Type** | **Bell** boosts or cuts around a frequency. **Low Shelf** and **High Shelf** boost or cut everything below or above it. **Low Cut** and **High Cut** remove everything below or above it (12 dB per octave). |
| **Freq** | Where the band acts, from 20 Hz to 20 kHz. |
| **Gain** | How much, from -18 to +18 dB. 0 does nothing. Not used by the two cuts. |
| **Q** | How wide. For a bell, a high Q is a thin notch and a low Q a wide sweep. 0.71 on a shelf or cut is the plain, flat shape. |
| **On** | Switches the band off without losing its settings. |
| **Extreme** | For microwave and hardtekk peaks. With it on, the band's Gain goes from -36 to +36 dB and its Q up to 50, for very thin, screaming peaks. The other bands keep the normal ranges. When you switch it on, the extreme Gain and Q start at the band's current ones, so the sound does not jump. The display then shows 36 dB up and down. |
| **Key Track** | 0 to 100 %. At 0 % the band stays where you put it. At 100 % the band's frequency moves one octave for every octave of the note you play, so a peak tuned to the kick on C stays at the same musical place on every note. **Freq** is the frequency at C3; in between, the band moves part of the way. It follows the note the tail settles on (with glide), not the pitch drop. With the plugin's own Key Track off, the band stays on the Root Note. The display shows the curve at the note being played. |

The bands start as a low shelf at 80 Hz, bells at 400 Hz and 2.5 kHz and a high shelf at 8 kHz, all flat. In the EQ strip the display shows the combined curve with one numbered dot per band: drag a dot sideways for Freq and up or down for Gain, turn the mouse wheel over it for Q, and double-click it to switch the band on or off. With every Gain at 0 and no cut in use the EQ changes nothing, and it adds no delay. A very loud Extreme peak is left to the safety limiter at the output: the EQ does not lower the level by itself.

### Filter

A classic synth filter to carve the kick: take the top off a distorted tail, thin out the low end of a layer or isolate a band. Add it with **+ Add effect**; it has the usual **On** and **Mix** and adds no delay.

| Control | What it does |
|---|---|
| **Type** | **Low Pass** keeps the lows, **High Pass** keeps the highs, **Band Pass** keeps a band around the cutoff, **Notch** removes a band around the cutoff. |
| **Slope** | **12 dB** (gentle) or **24 dB** (steep) per octave. |
| **Cutoff** | Where the filter acts, 20 Hz to 20 kHz (default 20 kHz: wide open). |
| **Resonance** | A peak at the cutoff, from none to a strong whistle. It never turns into a runaway tone. |
| **Drive** | Pushes the sound into a soft saturation inside the filter. 0 % is clean; it thickens the sound without a big change of level. |

With the defaults (Low Pass, fully open, no resonance, no drive) switching it on changes almost nothing until you move **Cutoff**.

### Stereo

A multiband stereo effect. It splits the sound into three bands (low, mid, high) and sets the stereo width of each. Switch it on with **Stereo On**.

| Control | What it does |
|---|---|
| **Crossover 1** | Where the low band ends (40 to 400 Hz, default 120 Hz). |
| **Crossover 2** | Where the high band starts (400 Hz to 8 kHz, default 2.5 kHz). The mid band is between the two. |
| **Low Width**, **Mid Width**, **High Width** | The width of each band: 0 % is mono, 100 % leaves it as it comes in, up to 200 % very wide. |
| **Mono Below** | On by default: forces the low band to mono, whatever Low Width says. The safe setting for a kick. |

The kick is generated in mono, so there is nothing to spread in the ordinary sense. Above 100 % the effect adds a *decorrelated* copy of the band to the sides (same loudness and tone, shifted in time in a way that differs from one frequency to the next), which sounds like a different signal in each ear. At 100 % or less nothing is added, so a mono kick stays exactly mono. It adds no delay, and with a mono output it does nothing.

- **Quality** (in the Global controls): how much the effects that make harmonics are oversampled (1x, 2x, 4x or 8x, default 2x). Higher is cleaner and uses more CPU. Changing it changes the small delay the plugin reports to your DAW.

## Presets

A preset is a complete sound: all the knobs, both envelope shapes, the order of the effects, and any wavetable or Tock sample you loaded yourself.

The **preset bar** is the strip at the top of the window:

- Click the preset name to open the list: the **Factory** presets first, then **Yours**. Pick one to load it. Loading a preset replaces the whole sound; anything the preset does not set goes back to its default.
- The **<** and **>** buttons go to the previous and next preset in the list and wrap around.
- To keep your own sound, type a name in the field and press **Save** (or Enter). If a preset with that name already exists you are asked before it is replaced. Names can use letters, numbers, spaces and `- _ . ( )`; anything else is dropped.
- **Init** (the first factory preset) is the plain default sound.
- A message appears in the bar when a preset cannot be loaded or saved. A damaged or unknown file is refused and your current sound stays as it was.

Your presets are files in your Documents folder, `Frenchcore Machine/Presets`, named `<name>.fcmpreset`. You can copy them to another computer or send them to someone; to delete one, delete the file. A preset or a DAW project carries a copy of any wavetable or Tock sample you loaded yourself (the sound and its file name, never the folder it came from), so sharing one shares that audio too.

The name shown in the bar is the last preset you loaded or saved in this session of the plugin. It is not stored in the DAW project (the project stores the sound itself).

## Global: output, limiter, quality, glide

| Control | What it does |
|---|---|
| **Output Gain** | Overall volume, from -48 dB (silence) to +12 dB. |
| **Safety Limiter** | On by default. It keeps the sound from ever going above 0 dBFS (the loudest level your DAW can carry), bending loud peaks softly instead of clipping them hard. Switch it off only if you want the raw signal. |
| **Key Track** | On: the MIDI note sets the pitch the kick settles on. Off: every note settles on the **Root Note**. |
| **Root Note** | The pitch used when Key Track is off. Default C1 (about 65 Hz). |
| **Glide** and **Glide Time** | With Glide on, the pitch the kick settles on slides from the previous note to the new one over Glide Time instead of jumping. The fast pitch drop at the start of each kick is not affected. |
| **Quality** | How much the effects are oversampled to avoid harsh aliasing: 1x, 2x (default), 4x or 8x. Higher is cleaner and uses more CPU. |
| **Velocity Sensitivity** | How much MIDI velocity changes the loudness: at 0 % (the default) every note is equally loud, at 100 % a soft note is much quieter than a hard one. |

How notes behave:

- The plugin plays one note at a time. A new note restarts the kick, and it does so without clicks even if the previous kick is still sounding.
- The length of the note sets the length of the tail. When you let go of the last key the kick fades out over the amp envelope's Release (30 ms by default), so a long note in the piano roll gives a long tail and a short note a short one. A shape with a Sustain of 0 (like Short Punch) is a one-shot that ends by itself.
- If you hold one note, press another and release the second, the pitch goes back to the first note without restarting the kick.

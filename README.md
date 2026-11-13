# Tessitura

A digital audio workstation for Windows PCs: multitrack audio and MIDI recording,
a piano roll, a mixer, VST3 plug-in hosting and a clean, Mac-style interface.

Built on [JUCE](https://juce.com) and [Tracktion Engine](https://github.com/Tracktion/tracktion_engine).
*Tessitura* is a working name.

> Status: **early development (v0.1)**. Core recording, editing, mixing and export work.
> See [Roadmap](#roadmap) for what is not built yet.

## What works today

| Area | Features |
|---|---|
| Project | New / Open / Save / Save As (`.tessproj` + `Audio/` folder per project), unsaved-changes prompts, undo/redo for every edit |
| Transport | Play, stop, record, cycle (drag in the top strip of the ruler), metronome, LCD with bars/beats + clock, drag or type tempo, type time signature |
| Tracks | Instrument tracks (built-in synth, built-in sampler or any VST3 instrument) and audio tracks; mute, solo, record-enable; rename by double-click |
| Recording | MIDI from any connected keyboard (the selected instrument track follows your keyboard), audio from your interface, live recording preview |
| Arrange | Regions: move (also between tracks), trim either edge, split at playhead, delete, mute; snap to grid (hold Alt to bypass); zoom (Ctrl+wheel / pinch); drag-and-drop audio files from Explorer or the Library |
| Piano roll | Click to draw, drag to move/resize, marquee select (Shift-drag), velocity, transpose (↑/↓, Shift = octave), quantise with grids from 1/4 to 1/32 including triplets |
| Mixer | Channel strips with instrument slot, up to 6 insert effects, pan, fader, stereo peak meters, master strip; built-in EQ, compressor, reverb, delay, chorus, phaser, filter, pitch shift |
| Plug-ins | VST3 instruments and effects (Kontakt, Spitfire, Arturia, Serum, etc.), scanned in a separate process so a faulty plug-in can't crash the app |
| Export | Mixdown to WAV (48 kHz / 24-bit default) including effect tails |

## Keyboard shortcuts

| Key | Action | Key | Action |
|---|---|---|---|
| Space | Play / Stop | R | Record |
| C | Cycle on/off | K | Metronome |
| Home / Enter | Go to beginning | P | Piano roll |
| X | Mixer | Y | Library |
| Q | Quantise | Delete | Delete region / notes |
| Ctrl+T | Split region at playhead | Ctrl+Z / Ctrl+Shift+Z | Undo / Redo |
| Ctrl+Alt+N | New instrument track | Ctrl+Alt+A | New audio track |
| Ctrl+S | Save | Ctrl+E | Export mixdown |
| Ctrl+, | Audio & MIDI devices | Ctrl+= / Ctrl+- | Zoom in / out |

## Building on Windows

Requirements: Visual Studio 2022 or newer (Desktop development with C++), Git.

```powershell
git config --global url."https://github.com/".insteadOf "git@github.com:"   # the engine's JUCE submodule uses an SSH URL
git clone --recurse-submodules -b claude/funny-carson-i8auti https://github.com/Matt-Bob-B/blank-app.git C:\Dev\tessitura
cd C:\Dev\tessitura
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
build\windows\Tessitura_artefacts\Release\Tessitura.exe
```

Every push also builds a Windows zip in GitHub Actions (**Actions → Build → Tessitura-windows-x64** artifact).

### ASIO (optional, recommended for low latency)

WASAPI works out of the box. For ASIO, download Steinberg's ASIO SDK and configure with:

```powershell
cmake --preset windows-release -DTESSITURA_ENABLE_ASIO=ON -DASIO_SDK_DIR=C:/SDKs/asiosdk
```

Check the ASIO SDK licence terms before distributing a build that includes it.

## Building and testing on Linux (development)

```bash
sudo apt-get install ninja-build libasound2-dev libfreetype-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxcomposite-dev libxext-dev libgl1-mesa-dev libcurl4-openssl-dev
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

The test suite renders audio offline and checks the results (MIDI through the synth, audio regions at the
right level, quantise and undo, time-signature maths, project save/reload, Save As moving recorded audio).

To generate a demo project for manual testing:

```bash
cd /some/folder && /path/to/build/TessituraTests --no-skip -tc="Generate demo project"
```

## Manual test checklist (on the target PC)

1. **Settings → Audio & MIDI Devices**: choose your interface (ASIO or Windows Audio), confirm the latency shown is sensible (under 10 ms at 128 samples).
2. Play your MIDI keyboard: you should hear the selected instrument track straight away.
3. Press **R**, play a phrase, press **Space**: a MIDI region appears; double-click it to edit in the piano roll.
4. Select the audio track, press **R** and record from a microphone or instrument; the take lands in `<project>/Audio`.
5. **Settings → Plug-in Manager → Options → Scan for new or updated VST3 plug-ins**, then click a track's instrument slot in the mixer (**X**) and load Kontakt or another VST3 instrument.
6. Add an insert effect, adjust the fader and pan, confirm the meters move.
7. Save, quit, reopen: everything (including plug-in settings) comes back.
8. **File → Export Mixdown**: play the WAV in another player.
9. Check the UI at 100% and 150% Windows display scaling.

## Working with Claude Code on your PC

`CLAUDE.md` briefs a local Claude Code session on the architecture, build commands and rules.
Start it from the repo folder (`claude`, or `claude remote-control` to follow it from the Claude app).
Record hardware results in `docs/windows-test-log.md`.

## Project layout

```
src/engine/   Engine layer (no UI): EngineHost, TrackOps, MidiTools, Exporter
src/ui/       Views: theme, control bar, arrange, piano roll, mixer, library, plug-in windows
src/app/      Application entry point and main window
tests/        Headless doctest suite for the engine layer
external/     Tracktion Engine (git submodule, includes JUCE)
```

## Roadmap

Next milestones, in order:

1. **SFZ and SF2 sample libraries** in the built-in sampler (via sfizz and TinySoundFont), with Library browsing of `.sfz`/`.sf2`.
2. **Mixer routing**: aux sends and returns, buses, track output routing.
3. **Automation lanes** for volume, pan and plug-in parameters.
4. **Audio editing**: fades, crossfades, clip gain, time-stretch to project tempo for loops.
5. **Track inspector** panel, track colours and icons, track reordering.
6. **Installer** (Inno Setup) and file association for `.tessproj`.
7. MIDI file import/export, count-in, punch in/out, take comping.

## Licence

This project is licensed under GPLv3 because it builds on Tracktion Engine (GPLv3/commercial) and JUCE (AGPLv3/commercial).
That suits personal or open-source use. **Selling it as closed-source software would require commercial licences
from both Tracktion and JUCE.** The original Apache-2.0 `LICENSE` file from the repository template should be
replaced with GPLv3 before any distribution.

Tessitura is an independent project. It is not affiliated with, and does not use the name, artwork or content of, any other DAW.

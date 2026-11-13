# Tessitura — notes for Claude

A Windows DAW (audio + MIDI recording, piano roll, mixer, VST3 hosting) in C++20 on
JUCE 8 + Tracktion Engine 3.2. Owner: Matthew (Matt). Personal use, GPLv3 dependencies.
Australian English in UI text, docs and commit messages.

## Build and test

Windows (Visual Studio 2022 or newer with the C++ workload):

```
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
build\windows\Tessitura_artefacts\Release\Tessitura.exe
```

Linux: same commands with `linux-release` (needs the apt packages listed in `.github/workflows/build.yml`).

- First configure and full build take 5–15 minutes (the whole engine compiles into `tessitura_core`). Incremental builds are fast.
- Demo project for manual testing: run `TessituraTests --no-skip -tc="Generate demo project"` from any folder; it writes `Demo/Demo.tessproj` there. Open it with `Tessitura.exe Demo\Demo.tessproj`.
- The JUCE submodule uses an SSH URL. If submodule fetches fail: `git config --global url."https://github.com/".insteadOf "git@github.com:"`.

## Layout

```
src/engine/   Engine layer, no UI. Everything testable headlessly lives here.
  Tess.h          single include for Tracktion (namespace alias te::)
  EngineHost      owns te::Engine + current te::Edit; new/open/save/save-as; recording paths
  TrackOps        instrument vs audio tracks (kind stored as "tessKind" on track state), regions, arming
  MidiTools       note entry, snap, quantise
  Exporter        offline WAV mixdown
  UIBehaviour.h   headless te::UIBehaviour (runs render jobs inline)
src/ui/       JUCE components: theme/, transport/, arrange/, pianoroll/, mixer/, library/,
              MainComponent (layout, menus, shortcuts), AppUIBehaviour (plug-in windows), Commands
src/app/      Main.cpp: application, main window, child-process plug-in scan entry
tests/        doctest suite; renders audio offline and checks the output
external/     tracktion_engine submodule (includes JUCE). Do not edit.
```

## Rules that prevent real bugs

- **Edit lifetime:** every view holds references into the current `te::Edit`. Views must drop them in
  `EngineHost::Listener::editAboutToBeReplaced()` (called synchronously before the old Edit is deleted).
  Never rely on async notifications for this; it caused a use-after-free on project open.
- **Undo:** call `edit.getUndoManager().beginNewTransaction ("Name")` before any user-initiated change.
- **Exports:** `Renderer` needs `tracksToDo` set and the live playback context freed first (see `Exporter.cpp`).
  The default `te::UIBehaviour::runTaskWithProgressBar` does nothing, so any new UIBehaviour must run the job.
- **Gain defaults:** pan law is linear (centred stereo = unity) and new projects start the master at 0 dB.
  Tests assert both.
- **Tests:** the shared test `EngineHost` must be destroyed before JUCE shuts down (`destroyTestHost()`).
- Engine features get a headless test in `tests/EngineTests.cpp` before or with the UI work.
- Don't copy another DAW's name, artwork, icons or bundled content. The look is "Mac-style", but original.

## Data boundary

This repo lives in a dedicated folder (e.g. `C:\Dev\tessitura`) **outside OneDrive and away from any
client-matter folders**. Never read, search, copy or reference files outside the repo, the audio/sample
folders the user points to, or the VST3 folder (`C:\Program Files\Common Files\VST3`). If a task seems to need
anything else, ask first.

## Working with the cloud session

Matt also uses a Claude Code cloud session on the same branch (`claude/funny-carson-i8auti`).
`git pull` before starting work and push when a piece of work is done, so the two don't conflict.
The local (Windows) session owns: MSVC builds, running the app, audio/MIDI hardware, VST3 plug-ins, UI checks.
Record hardware test results in `docs/windows-test-log.md`.

## Current status and next steps

Done: engine core, arrange, piano roll, mixer with inserts, VST3 hosting, library panel, WAV export.
In order next:
1. Confirm the Windows build and run the README's manual test checklist on real hardware; fix what fails.
2. SFZ / SF2 support in the built-in sampler (sfizz + TinySoundFont), with Library browsing.
3. Mixer routing: aux sends/returns, buses.
4. Automation lanes.
5. Fades, crossfades, clip gain, tempo-synced loops.
6. Installer (Inno Setup) and `.tessproj` file association.

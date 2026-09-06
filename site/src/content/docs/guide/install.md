---
title: Install
description: Getting midi-sink on macOS, Windows, Linux, iPad and Android — and the marble canvas in your browser.
---

midi-sink is one engine with six shells. Pick yours.

## In the browser — nothing to install

Open **[/marble/](/marble/)**. It needs WebGPU in a secure context: Chrome or
Edge, Safari 26, Firefox 141+, over `https://` (or `localhost`). Marble mode
only — tap, drag, twist, pinch, pen — with a MIDI instrument playing straight
in on Chrome and Edge through Web MIDI. Settings live in the panel at the top
left and persist in your browser. [More about the web canvas →](../web/)

## Desktop — macOS, Windows, Linux

**macOS** — Homebrew, from the vibetuned tap:

```sh
brew install --cask vibetuned/tap/midi-sink
```

or download `midi-sink-<version>-macos-universal.dmg` from the
[GitHub Releases page](https://github.com/vibetuned/midi-sink/releases) and
drag the app to Applications. One universal build for Apple silicon and
Intel, macOS 12 Monterey or later, signed and notarized — it opens without a
Gatekeeper warning. The version in About is the release tag.

**Windows** — winget:

```powershell
winget install Vibetuned.MidiSink
```

or download `midi-sink-<version>-windows-x64-setup.exe` from the
[GitHub Releases page](https://github.com/vibetuned/midi-sink/releases) —
a per-user install (no admin prompt) with a Start-menu entry and a normal
uninstaller; your settings in `%APPDATA%\midi-sink` survive uninstalling
unless you say otherwise. Prefer no installer at all? The
`…-windows-x64-portable.zip` is the same self-contained exe — unzip and run.
Until the builds are code-signed, the first launch of a new version shows a
SmartScreen notice: choose **More info → Run anyway** (once per version).
MIDI arrives through WinMM — plug the instrument in and it appears in the
settings window within a second.

**Linux** builds attach to the same Releases page as their lane lands; the
version in every artifact comes from the release tag — there is nothing to
check by hand.

Or build it yourself:

```sh
git clone https://github.com/vibetuned/midi-sink && cd midi-sink
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
open ./build/desktop/midi-sink.app        # macOS
./build/desktop/midi-sink                 # Windows / Linux
```

CMake ≥ 3.24, Ninja, and a C++20 compiler (Xcode, MSVC 2022, gcc or clang).
Linux needs the GL/X11/Wayland dev packages GLFW wants plus the ALSA headers
(Debian/Ubuntu: `libgl1-mesa-dev xorg-dev libwayland-dev libxkbcommon-dev
libasound2-dev`). On Linux, `cmake --install build --component
desktop-integration --prefix ~/.local` gives the app its icon and name in the
dock.

The app launches to a playable instrument: connect a MIDI device and it shows
up in the settings window (⌘ , on macOS, Ctrl , elsewhere).
[The desktop app →](../desktop/)

## iPad

midi-sink for iPadOS ships through TestFlight during the beta wave and then
the App Store; the link appears here when the beta opens. It is built for
iPadOS 16+ and is happiest on an iPad with an Apple Pencil — the Pencil Pro's
squeeze and barrel roll are used when present.

From source: build `libsumi.a` for iOS with CMake, generate the Xcode project
with `xcodegen` in `ios/`, and build the `midi-sink` scheme — the README in the
repository has the exact commands.

## Android

midi-sink for Android reaches the Play Store through a closed-testing wave
first; the opt-in link appears here when it opens. Targets tablets with a
stylus (developed on the Galaxy Tab S8 Ultra with the S-Pen) but runs on any
Android 8+ device with OpenGL ES 3.

From source: `cd android && ./gradlew assembleDebug` with SDK 36 and NDK r27 —
Gradle drives the same CMake tree as the desktop build.

## What you need to hear it

midi-sink is a visualizer and, on the tablets, a controller: it makes no sound
of its own. Plug an instrument in to see it play ([devices](../devices/)), or
on a tablet enable an outbound transport and point a synth or DAW at "midi-sink
Play Surface" ([Play mode](../play-mode/)).

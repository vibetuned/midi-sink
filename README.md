# midi-sink

A suminagashi (Japanese ink-marbling) visualizer driven by expressive MIDI.
The core engine (`libsumi`, C-ABI, sokol_gfx) is platform-portable by design;
this repo builds the desktop harness on macOS (Metal), Windows (D3D11) and
Linux (OpenGL 4.1 core), plus a SwiftUI iPad app (Metal) and a Jetpack
Compose Android app (GLES3).
Full specification: [docs/PROJECT_SPEC.md](docs/PROJECT_SPEC.md);
implementation decisions: [docs/DECISIONS.md](docs/DECISIONS.md); history:
[docs/CHANGELOG.md](docs/CHANGELOG.md) and [docs/ROADMAP.md](docs/ROADMAP.md).

## Build & run

```sh
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
./build/desktop/midi-sink                              # Windows / Linux
open ./build/desktop/midi-sink.app                     # macOS (a real .app bundle)
```

The desktop app launches to a playable instrument: connect a MIDI instrument
and it appears in the **settings window** (opens beside the canvas; close it
any time and bring it back with **⌘ ,** on macOS or **Ctrl ,** elsewhere).
Every setting lives there — layout, palette, the expression routings
(note bend, aftertouch, CC 74, pinch style, vortex profile), the ripple, the
CC map editor, the MIDI input list with its rescan status, the paper dip and
print export, and About (version, commit, engine). Settings persist in the
platform config directory (`~/Library/Application Support/midi-sink`,
`%APPDATA%\midi-sink`, `~/.config/midi-sink`). The version shown comes from
the git tag (`-DSUMI_APP_VERSION=…` in CI; `git describe` locally) — no
version is ever edited by hand.

**`--dev`** enables the lab bench: the debug keys listed below, the scripted
DONE tests and `--field-dump`. Without it the app accepts only `--help` and
`--version`, and the keyboard does nothing but the settings chord.

On Windows run the same commands from an **x64 Native Tools** prompt (or any
shell where `vcvars64.bat` has been applied) with CMake ≥ 3.24 and Ninja on
PATH; MSVC 2022 is the supported toolchain. `build_win.bat <command...>` is a
convenience wrapper that sets that environment up first. MIDI arrives through
WinMM; for a scripted/virtual source, create a loopback port with
[loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and feed it
with `build\tests\mpe_stress_win.exe` (see tests/mpe_stress_win.cpp).

On Linux install the distro GL/X11/Wayland dev packages GLFW needs plus the
ALSA headers for libremidi (Debian/Ubuntu: `libgl1-mesa-dev xorg-dev
libwayland-dev libxkbcommon-dev libasound2-dev`); gcc or clang both work. The
harness creates the GL 4.1 core context itself (spec §5.1: on GL the host
owns the context) and MIDI arrives through ALSA. Scripted/virtual sources
need no extra tooling — `build/tests/mpe_stress_alsa` and
`build/tests/wind_breath_alsa` create their own `snd_seq` virtual ports,
which the harness's 1 Hz rescan opens automatically.

All connected MIDI inputs (hardware and virtual, hotplugged) are opened
automatically. Mouse: left click = ink drop, left drag = tine, right drag =
vortex (profile from settings), Shift+left drag = pinch (drag distance =
strength delta, drag angle = fold axis), middle drag = stylus wake (scroll
wheel adjusts the tip radius).

Lab bench keys (**`--dev` only**): `1`–`6` viscosity / ink feed / roughness,
`7` palette, `8`/`L` layout, `9` paper dip, `B` bpm, `V` vortex profile,
`K` ripple live/bake, `C` pinch variant, `P` pressure routing, `M` note-bend
routing, `O` ripple angle, `R`/`T` ripple amplitude and `F`/`G` frequency (as
CC 102/103 through the real ctl path), `X` stamps the crossed-tine pinch
prototype (DECISIONS.md Part III #32), `J`/`W`/`E` the swirl test voice. The
§4.6 field regression is `midi-sink --dev --field-dump <file>`.

### iOS (SwiftUI shell)

```sh
cmake -B build-ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DBUILD_TESTING=OFF && cmake --build build-ios     # libsumi.a for iOS
cd ios && xcodegen                                       # project.yml -> .xcodeproj
xcodebuild -project midi-sink-ios.xcodeproj -scheme midi-sink \
           -destination 'generic/platform=iOS' -allowProvisioningUpdates build
```

Swift imports the pure-C core directly (`import SumiCore` via
`core/include/module.modulemap` — no Objective-C wrapper, spec §5.4). MIDI
arrives through CoreMIDI (wired, network, and Bluetooth — pair instruments
from the in-app settings sheet); hotplug is notification-driven. The settings
sheet also picks the pitch layout and toggles sim_scale (defaults 1.0 on
iPad-class GPUs, 0.75 below).

**Marble mode** — tap = drop, one-finger drag = tine, two-finger twist =
vortex, two-finger pinch = fold.

**Play mode** (settings → Mode, on the Chromatic grid, Jankó or Piano grid) —
the same virtual MPE instrument as on Android (below): finger joysticks on
the lattice, the Apple Pencil playing per-cell legato with real-force
velocity, its barrel roll deepening vibrato and its squeeze (Pencil Pro)
acting as the sustain pedal, and the floating control strip. The stream goes
to the loopback visualizer and out over the virtual CoreMIDI source (also
sent to a USB-tethered Mac), the MIDI network session and BLE, each under
its own rate policy. Settings → Evidence captures the screen and flushes the
byte/latency/session logs into the app's Documents folder (pull them with
`xcrun devicectl device copy from --domain-type appDataContainer`); the same
`tools/midi_asserts.py` / `tools/pen_trace.py` analyse them.

### Android (Compose shell)

```sh
cd android && ./gradlew assembleDebug      # needs SDK 36 + NDK r27 (local.properties)
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Gradle's externalNativeBuild points CMake at the repo root (the NDK toolchain
defines `ANDROID`: core + hostmpe + the one JNI lib in `android/cpp`, static
archives only). All `sumi_*` calls run on a dedicated render thread owning the
EGL context (§5.4; `surfaceDestroyed` blocks until the surface is released —
the hard teardown contract). MIDI arrives through AMidi; BLE-MIDI instruments
(ROLI) pair via the in-app Bluetooth entry. sim_scale defaults 0.75, drops to
0.6 under THERMAL_STATUS_SEVERE (recovers at MODERATE), and the EGL surface is
capped at phone-class pixels on oversized panels (DECISIONS_2 #31).

**Marble mode** — tap = drop, one-finger drag = tine, two-finger twist =
vortex, two-finger pinch = fold.

**Play mode** (settings → Mode, on the Chromatic grid, Jankó or Piano grid) —
the virtual MPE instrument: every touch is a joystick on the lattice (X bends
in semitones, Y up feeds ink, Y down stirs the swirl), the S-Pen plays
per-cell legato with a real-pressure velocity and trails a dipolar wake, and a
floating control strip (Pitch spring, Mod latch, two assignable wheels,
Sustain) rides the MPE master channel. The generated stream goes to the
loopback visualizer AND out three sinks, each under its own rate policy:

* **USB-MIDI (primary)** — set *Use USB for* to MIDI in the system USB
  preferences; the host then sees a class-compliant USB-MIDI device
  (`amidi -l` on Linux), lowest latency of the three.
* **Virtual device** — "midi-sink Play Surface" in any on-device Android DAW.
* **BLE-MIDI advertise** — the tablet advertises as a BLE-MIDI peripheral for
  a desktop DAW to connect to (budget-limited, ~300 msg/s).

Debug extras via `adb shell am start -n com.vibetuned.midisink/.MainActivity`:
`--es fieldDump 1` (the §4.6 dump — run it from a FRESH start, the script does
not reset the field), `--ei stressMinutes N` (in-process Osmose feeder),
`--es hostmpeTests 1` (the full hostmpe + normalizer suites on-device →
`files/selftest.txt`), `--ei layout N`, `--es playMode 1`,
`--es transports usb,virtual,ble`, `--ei stormSeconds N` (10-voice storm
through the whole pipeline), `--es resync 1`, `--es panic 1`,
`--es flushLogs 1` (writes `files/midi_log.csv` + `files/latency_log.csv`).
Analyse those with `tools/midi_asserts.py` and `tools/pen_trace.py`; capture
the wire side on Linux with `build/tests/midi_capture_alsa`.

## App icon

All platforms' icons derive from `images/midi-sink.jpg`; regenerate them with

```sh
python3 tools/gen_icons.py     # needs pillow + numpy
```

which writes the Android mipmaps/adaptive icon, the iOS asset catalog, the
harness's compiled-in window icon, the Windows `.ico`, the macOS Dock PNG
header, and the Linux XDG icon theme. The desktop icons are the square artwork with rounded corners
(shells do no masking of their own); iOS and Android get full-bleed and
keyed-foreground forms respectively, since both mask the icon themselves —
see DECISIONS_2 #36–40.

On Linux the desktop entry is what gives the app its icon and name in the
dock, app grid and alt-tab — a Wayland compositor takes them from the
`.desktop` file matching the window's app_id, never from the client:

```sh
cmake --install build --component desktop-integration --prefix ~/.local
```

(The `--component` matters: without it CMake also installs every FetchContent
dependency's headers and libraries into the prefix. The install refreshes the
XDG desktop and icon caches itself.)

The entry's `Exec`/`TryExec` are written as absolute paths at install time,
which is required rather than tidy: GIO drops any desktop entry whose `Exec`
binary is not in PATH, and gnome-shell's PATH does not include `~/.local/bin`
— with a relative `Exec` the shell never loads the file and the window shows a
generic icon (DECISIONS_2 #39c).

On macOS the harness is a bare executable (no `.app` bundle / `.icns`), so
the Cocoa glue sets the Dock tile at runtime from a compiled-in PNG
(`desktop/src/app_icon_macos.h`) — the macOS analog of the runtime window
icon on X11/Windows.

## Input modes (auto-detected, override via `sumi_set_input_mode`)

- **MPE** (ROLI Seaboard/Piano, Expressive E Osmose): one voice per member
  channel — strike paints a drop, press grows it continuously, glide drags it
  along the pitch axis, slide (CC74) modulates its ink selector (or, with
  `slide_mode = 1`, pinches the water), per-note pressure stirs a Lamb–Oseen
  swirl; a lift simply stops the feed.
- **Wind** (Aerophone, Travel Sax): a single wandering ink brush — breath
  (CC2 / CC11 / channel pressure) feeds the line's thickness, legato note
  changes migrate the brush with a wake.
- **Classic** (any keyboard): notes are drops on the circle of fifths
  (velocity → size), pitch bend shears the bath, sustain pedal dips the paper
  (classic mode only — in MPE the pedal is a musical control, DECISIONS.md
  Part III #67).

Layouts (key `L` cycles live): circle of fifths, chromatic grid (C1–B7),
Jankó (each note stamps all three rows of its parity), and two BPM-driven
piano rolls (horizontal / vertical) whose field scrolls at
`(bpm/60) × roll_speed` canvas-lengths per second (default roll_speed 0.0625:
16 beats — 4 bars of 4/4 — of history span the canvas) — key `B`/`Shift-B`
nudges BPM ±5 for syncing against a metronome.

## CC routing (global field controls)

Any CC can drive any global dimension at runtime via the C ABI:
`sumi_map_cc(inst, channel /*0xFF = any*/, cc, target)`;
`sumi_clear_cc_map(inst)` removes all routes (including these defaults).
A channel-specific route overrides an any-channel route. CC64 (paper dip) and
CC74 on MPE member channels (slide) are reserved and not routable.

Default bindings (Airwave dimensions are user-assigned on the device side —
match them to this table or remap):

| CC | Target (`sumi_ctl_t`) | Intended source |
|----|------------------------|-----------------|
| 1  | `SUMI_CTL_VORTEX_STRENGTH` | mod wheel |
| 2  | `SUMI_CTL_INK_FLOW` (breath) | wind instruments |
| 7  | `SUMI_CTL_INK_FLOW` (breath alias) | wind instruments (volume) |
| 11 | `SUMI_CTL_INK_FLOW` (breath alias) | wind instruments (expression) |
| 20 | `SUMI_CTL_VORTEX_STRENGTH` | Airwave left-hand Raise ("wind over the water") |
| 21 | `SUMI_CTL_VORTEX_X` | Airwave Glide (vortex center drift) |
| 22 | `SUMI_CTL_VORTEX_Y` | Airwave Glide (vertical) |
| 23 | `SUMI_CTL_VISCOSITY` | Airwave right-hand Tilt (damping) |
| 24 | `SUMI_CTL_PAPER_ROUGHNESS` | Airwave Flex |
| 25 | `SUMI_CTL_PALETTE_MORPH` | Airwave Flex (alternate) |

The v0.4 ripple dimensions (`SUMI_CTL_RIPPLE_AMP`, `SUMI_CTL_RIPPLE_FREQ`)
ship **unmapped** — CC 1 stays the vortex mod wheel (DECISIONS.md Part III
#32). Bind
them per setup with `sumi_map_cc` (the desktop harness maps CC 102/103 for
its R/T and F/G keys; the iOS strip's assignable wheels take them on-device).

Vortex strength/center and viscosity act immediately; paper roughness and
palette morph are tracked and smoothed but only take visible effect once the
washi/palette composite lands (see DECISIONS.md).

Harness test flag: `--map-cc <cc>:<target>` applies one any-channel route at
startup (target = numeric `sumi_ctl_t`, e.g. `--map-cc 30:0` routes CC30 to
vortex strength).

## License

midi-sink is free software, licensed under the GNU Affero General Public
License v3.0 — see [LICENSE](LICENSE).

# Screenshots (author input — real sessions, not mock-ups)

App Store Connect needs iPad screenshots for the 13" and 12.9" displays; one
set is reused for smaller iPads. Capture on the iPad at native resolution
(hardware buttons, or Settings → Evidence for a timed full-screen burst that
carries Metal content) and drop the PNGs here as `NN-<slug>.png`:

| Slot | Shows | Mode / layout |
|---|---|---|
| 01 | dense marbling under a ROLI chord — the "what it looks like" shot | Marble · circle of fifths |
| 02 | Play mode lattice with three finger joysticks and their indicators | Play · chromatic grid |
| 03 | Pencil legato across the piano grid, wake visible | Play · piano grid |
| 04 | the control strip, sustain lit by a squeeze | Play · Jankó |
| 05 | settings sheet: layouts, routings, transports | — |
| 06 | a paper print (the saved PNG) | Marble · Indigo palette |

Sizes ASC accepts (portrait / landscape): 13" iPad 2064×2752 / 2752×2064;
12.9" iPad 2048×2732 / 2732×2048. Landscape matches how the app is played.
No device frames, no marketing text on the image — the caption fields in
ASC carry the words.

---

## What is captured (v1.0.0, 2026-09-10)

Captured from the tagged `v1.0.0` build (`1.0.0 (56)`) driven through the real
UI — real gestures, real sessions, nothing mocked.

| File | Shows | Palette · layout |
|---|---|---|
| `ipad-13/01-marble-sumi.png` | drops combed into a suminagashi sheet, the hero | Sumi black |
| `ipad-13/02-play-chromatic-indigo.png` | Play mode: chromatic lattice, the control strip (Pitch/Mod/CC 23/CC 24/Sus), a finger down with its joystick ring | Indigo · Chromatic grid |
| `iphone-6.9/01-marble-sumi.png` | the same instrument on a phone | Sumi black |

**Which device these came from.** App Store Connect accepts only the **iPad
13"** sizes (`2064×2752` / `2752×2064`) and **iPhone 6.9"**
(`1320×2868` / `2868×1320`). The author's physical iPad is an **iPad Air
11-inch (M4)** at 1640×2360, which matches neither and cannot be rescaled
without distorting, so these come from the **iPad Pro 13-inch (M5)** and
**iPhone 17 Pro Max** simulators. A physical 13" iPad would be the only way to
capture these with a real Apple Pencil.

**Running the app in the Simulator.** The shipped shader table has no
`SG_BACKEND_METAL_SIMULATOR` entry, so `sumi_renderer_create` dereferences a
null shader desc and the app segfaults on launch in the Simulator (device
builds are unaffected). No repo change is needed to work around it —
`SUMI_SHDC_SLANG` is a CMake cache variable, so add `metal_sim` at configure
time for a simulator-only build:

```sh
cmake -B build-ios-sim -G Ninja -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
  -DSUMI_APP_VERSION="$(git describe --tags --always --dirty | sed 's/^v//')" \
  -DSUMI_SHDC_SLANG="metal_macos:metal_ios:metal_sim:hlsl5:glsl410:glsl300es:wgsl"
cmake --build build-ios-sim
```

then build the app against `build-ios-sim/{core,hostmpe}` via
`LIBRARY_SEARCH_PATHS`.

**Counts.** ASC requires a minimum of **one** screenshot per required slot
(iPad 13" and, because `TARGETED_DEVICE_FAMILY` is `1,2`, iPhone 6.9"), up to
ten each. The set above meets that with two iPad and one iPhone.

**Still to capture (optional).** A Play-mode shot on iPhone, and an App
Preview video (uploaded to ASC as a file at the slot's own resolution — a
YouTube link is Play-only).

---

## 2.0 — the slots to add (author input, from real sessions)

The 1.0 set stands; 2.0 wants the new things seen. Same rules: real
sessions, no frames, no words on the image, the caption fields carry them.

| Slot | Shows | Mode / layout |
|---|---|---|
| 07 | Anod: a discharge mid-performance, the glow and the water's grid — the second hero | Marble · Anod · electric blue |
| 08 | the trumpet layout, the three valve pads at the side, a partial lit | Play · trumpet (arc) |
| 09 | the Sound page: the source row, the Suzu patch picker, the instrument list | — |
| 10 | the strings layout under a chord shape, every string's echo | Play · strings (standard guitar) |
| 11 | a replay playing: the banner naming the source device | Marble · replay |
| 12 | the prints ledger with three dips and the export sizes | — |

## What is captured (2.0, 2026-10-14)

Captured from the 2.0 build (`2.0.0-rc.5`, the tree at the Phase-10 fold)
on the **iPad Pro 13-inch (M5)** and **iPhone 17 Pro Max** simulators —
real sessions, replayed: the three scenes are recordings (`.sumireplay`)
of a scripted MPE performance played over USB into the Galaxy Tab
(`tools/store_screenshots/perform.py`), copied into each simulator's
`Documents/Replays/` and run with `--replay-file`, the viewer's session
(`Library/Application Support/last_session.json`) set to the recording's
medium and palette. The strip and the lattice are the live overlay; the
water is the recording, drawn by this device's Metal.

| File | Shows | Medium · palette · layout |
|---|---|---|
| `ipad-13/03-anod-janko-midi.png` | the discharge after the Jankó performance — the second hero | Anod · electric blue · Jankó |
| `ipad-13/04-replay.png` | the same replay mid-way, the banner naming the source device | Anod · electric blue · Jankó |
| `ipad-13/05-play-strings-midi.png` | the strings under the performance, every string's echo | Sumi · indigo · strings (standard guitar) |
| `iphone-6.9/02-anod-janko-midi.png` | the discharge on the phone | Anod · electric blue · Jankó |
| `iphone-6.9/03-replay.png` | the replay banner on the phone | Anod · electric blue · Jankó |
| `iphone-6.9/04-play-strings-midi.png` | the strings on the phone | Sumi · indigo · strings |

**Orientation.** These are PORTRAIT (`2064×2752`, `1320×2868` — both
accepted sizes). The Simulator application could not be launched from the
session that made them (only `simctl` was reachable), and `simctl` cannot
rotate a device; the 1.0 set is landscape. If App Store Connect refuses a
mixed set, rotate the simulator (Simulator → Device → Rotate Left, ⌘←) and
re-run the same launches — the recordings and the session files are what
matter, the commands are in `tools/store_screenshots/assemble20.py`'s
header and the DECISIONS entry (`DECISIONS_9 #18`).

**Not captured on iOS.** The Sound page and the trumpet with its panel:
the simulator takes no touches from this session, and the iOS lab's
fingering demo draws nothing in the simulator (a lab-only matter, carried);
both exist in the Android set, and on a real iPad they are a tap away.

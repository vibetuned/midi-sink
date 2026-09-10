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

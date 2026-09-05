# Evidence — Step 25: WebGPU backend & marble web

PHASE5 §5 (whole section); PROJECT_SPEC §4.6 (the web tier joins the field
regression). Decisions: `_work/DECISIONS_4.md` #15–#20. Machine: the author's
Mac (Chrome 152, Apple Metal-3 adapter through Dawn; Emscripten 6.0.9 via
Homebrew, CI pins 4.0.15). The one permitted core change of the phase — the
WebGPU seam — landed as: `core/src/swapchain_webgpu.cpp` (the fourth
swapchain TU), `SUMI_BACKEND_WEBGPU` + `sumi_webgpu_surface_t` in the ABI
(version 0.5.0, additive), the WGSL dialect in the shader pipeline, and the
readback seam (`prepare_image`/`release_image`, `read_field_begin`/`poll`)
every backend implements.

## What landed

* **Core:** `swapchain_webgpu.cpp` — surface from a canvas selector,
  configured in the canvas's preferred format, per-frame surface texture,
  resize reconfigure, readback via injected CopySrc textures + copy +
  `mapAsync` (spontaneous callbacks). `emcmake cmake` builds `libsumi.a` for
  wasm with SOKOL_WGPU; the shader headers now carry `wgsl` for every build.
* **Web shell (`web/`):** `sumi_web.cpp` export glue (the C-ABI is the export
  surface; the glue builds the config struct and exposes flat param
  accessors + the field-dump hooks) and `web/site/` — `index.html`,
  `sumi-host.js` (WebGPU device → `Module.preinitializedWebGPUDevice`,
  RAF loop, DPR-aware resize, pointer/touch/pen gestures: tap = drop,
  drag = tine, two-finger twist = Rankine vortex, two-finger pinch = fold,
  pen = wake with pressure, mouse right/middle/Shift-drag as on desktop;
  WebMIDI input on Chrome/Edge with silent gestures-only elsewhere; paper dip
  and PNG print export; a vendored lil-gui settings panel mirroring the
  desktop settings window at the top left — its title is the brand, there is
  no top bar — persisted in localStorage — #21), `scenes.js` (nine operator
  scenes whose sliders are the formulas' symbols; the scene card sits top
  right), the `?scene=…&param=…&embed=1` contract, and `?fielddump=1` for the
  §4.6 web tier. Output: `sumi.wasm` 266 KB, `sumi.js` 56 KB.
* **Tooling:** `tools/web_gate.mjs` — serves the build, drives headless
  Chrome, receives the field dump / console / canvas PNGs over POST, runs the
  comparator; modes: field gate, `--scenes` sweep, `--shots` canvas renders,
  `--fullshots` full-page captures (canvas + DOM) through the DevTools
  protocol in the WebGPU-capable headless mode.
* **Spine:** the `web` lane in `release.yml` (first lane: `dist-web`
  artifact `midi-sink-<version>-web.tar.gz`, Pages deploy under `/marble/`
  guarded by `dry_run`); a wasm compile-check job in `build.yml`.

## DONE verification

| DONE criterion | Result | Evidence |
|---|---|---|
| Web tier passes §4.6 within its documented tolerance | **PASS — at the DESKTOP tolerance, not the mobile tier.** WebGPU dump vs the committed Metal fixture: max\|Δ\| 9.77e-4 (ink), 4.88e-4 (u, v), 0 (aux); mean 7.9e-9 — inside 1e-2 / 1e-4 with 10× headroom (#20) | `field_gate_webgpu.txt`, `fielddump_page_console.txt` |
| Every operator scene works via query params | **PASS** — headless sweep of all nine scenes (`drop tine vortex rankine wake pinch ripple lamb_oseen scroll`), each running its deterministic script to completion with zero console errors, plus canvas renders captured from the WebGPU path for five of them | `scenes_sweep.txt`, `webgpu_scene_*.png` |
| First marble < 5 s on a mid-range phone (Lighthouse) | **PASS** — Lighthouse 13.4 mobile emulation, simulated throttling: performance score 1.0, FCP 1.3 s, TTI 1.7 s, TBT 0 ms; the page's `first-marble` user-timing mark at **252 ms** | `lighthouse_mobile.txt` |
| Mouse-playable on desktop Chrome | **Verified through the engine, not a hand:** the gesture handlers map to the same ABI calls the scenes exercise (drop/tine/vortex/wake/pinch all rendered above); the pointer path itself is the user's check — the Mac was locked overnight, so no headed session was possible here | `webgpu_scene_*.png` |
| Touch-playable on iPad Safari; the ROLI drives it over WebMIDI in Chrome; Safari gestures-only with no error banner | **YOURS** — serve with `python3 tools/web_serve.py` and open `https://<mac-ip>:8443/` on the iPad (WebGPU needs a secure context — plain http over the LAN hides it; accept the dev certificate once). Safari 26 has WebGPU, no WebMIDI → the MIDI line in the info panel reads "not available in this browser — gestures only", nothing else appears. Android Chrome: same URL, same rule | — |
| Pages deploy joins the spine as the web lane | **PASS (authored; runs on your push)** — `web` job with `needs: [version, gates]`, `dist-web`, Pages upload/deploy guarded by `dry_run`; `publish` now `needs: web` | `.github/workflows/release.yml` |
| Headless suites / desktop unaffected | **PASS** — ctest 4/4 after the seam; desktop `--dev --field-dump` still bitwise-identical to the fixture; the app version check reads libsumi 0.5.0 | build log |

## Renders (from the WebGPU path, captured in-page)

`webgpu_scene_rankine.png` — the ring cluster after a rigid Rankine core
rotation: intact, unblurred rings (the property itself; a rotation of circles
is invisible by design). `webgpu_scene_wake.png` — the dipolar wake: the
forward bulge ahead of the tip and the trailing cusp, threading the rings.
Also `drop`, `pinch`, `lamb_oseen`.

## Page captures (`--fullshots`, canvas + DOM, 900×700 headless window)

`page_main_settings.png` — the free-play page: lil-gui settings panel top
left (every folder open except Ripple/About), the hint card clear of it,
no top bar. `page_scene_drop.png` — a `?scene=drop` page: settings panel
plus the scene card top right over the rendered rings. `page_scene_wake_embed.png`
— `?scene=wake&embed=1`: the settings panel and hint hidden, scene card only.

## Found on the way (DECISIONS_4 #20)

The port never exports `Module.WebGPU`; the device bridge is
`preinitializedWebGPUDevice` + `emscripten_webgpu_get_device()`. Map
callbacks must be `AllowSpontaneous` (ProcessEvents on the TU's own instance
never delivered the imported device's completions — 1,700 frames polled). A
POST_BUILD copy of the host page did not run on JS-only changes (now its own
always-run target). Chrome's `--screenshot` headless pipeline has no WebGPU
(hence the in-page canvas capture in the gate tool). After the user's first launch: the "no WebGPU"
overlay painted over a working canvas because `.overlay { display: grid }`
outranked the UA's `[hidden]` rule — every early "no WebGPU" sighting was
this, fixed with `.overlay[hidden] { display: none }`; and `navigator.gpu`
is secure-context-only, so LAN testing uses `tools/web_serve.py` (HTTPS).

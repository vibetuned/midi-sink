# Step 66 — Web marble: the instruments as overlays, replay in the browser — the evidence

`DECISIONS_8 #25` (the overlays and the state-carrying probe shim), `#26`
(replay playback in the browser and the gates). Spec: INSTRUMENT §4
(overlays only — Play stays web-deferred), QOL §1 (the gallery's "watch it
again"). Machine: the author's Mac, headless Chrome (WebGPU over Metal),
the web tier 2.5e-2 / 1e-3. No core change: libsumi stays 1.5.0.

## What changed in the tree

* `web/sumi_web.cpp` — the probe passes the engine's layout state and returns
  the cell's flags; `sumi_web_layout_state`; the two Phase-9 params by id
  (`trumpet_arc` 43, `string_tuning` 44); the layout setter no longer folds
  ids modulo eight; the replay shim (`sumi_web_replay_open/close/banner/
  stat/begin/step/rebucket`). `web/CMakeLists.txt` links `sumi_replay`; the
  root CMake adds `replay/` to the web branch.
* `web/site/index.html`, `style.css` — the `#overlay` canvas over the water.
* `web/site/sumi-host.js` — the five layouts in the picker, the overlay
  (`drawOverlay`: the probe sweep, the theremin's axis, the brass HUD), the
  panel's brass arrangement / string tuning / overlay toggle / valve and
  slide controls, `?layout= ?overlay= ?valves= ?slide=`, replay playback
  (`?replay= ?replaydump= ?replaywall= ?pace=0`, "Replay a recording…",
  "Stop the replay", the input guards, the dips' prints consumed before the
  gate's field dump).
* `tools/web_gate.mjs` — `--replay`, `--replay-field`, `--wall-hz`.
* `replay/src/sumi_replay.c` — `gmtime` for the strict-C11 wasm build.
* `tools/README.md`, `replay/FORMAT.md`, `docs/CHANGELOG.md`, `CLAUDE.md`,
  `_work/DECISIONS_8.md` #25–#26.

## The gates (`gates/`)

| gate | result |
|---|---|
| `web_gate.mjs` — the §4.6 field dump vs the Metal fixture at the web tier | PASS: max 9.8e-4, mean 3.9e-9 (`field_gate_webgpu.txt`) |
| `--scenes` | 17/17 ran clean (`scenes.txt`) |
| `--preset` | the page's own form back through the page: 2104 → 2104 bytes, BYTE-IDENTICAL; the desktop's session differs by its Suzu block (no Voxo in the browser) and the file's formatting (`preset_roundtrip.txt`) |

## The overlays (`overlays/`)

`--fullshots` at 1200×800, `?layout=N&overlay=1`: the trumpet (8) and the
trombone (9) as columns of partials with their names, the trombone's
`slide 0.00` HUD; `valves=1` — every trumpet partial two semitones lower
(B♭5→A♭5, F5→E♭5, D5→C5 …): the probe carries the state; `slide=0.5` on the
trombone; Wicki–Hayden (10) as the hex field; the strings (11) under the
standard tuning, six rows; the theremin (12) as the axis with C4…C7; Jankó
(2) for the keyed layouts of before. The panel in the same captures shows
the rows that follow the layout.

## Replay in the browser (`replay/`)

`web_gate.mjs --replay <file> --replay-field <dump> --compare … --max-tol
2.5e-2 --mean-tol 1e-3`: the page at `?replay=/replay.sumireplay&replaydump=1
&pace=0`, the canvas held at the recording's size, the field after the last
frame posted; then the same re-bucketed by wall time at 60 Hz.

| recording | the browser's replay vs the recording's field | re-bucketed at 60 Hz | verdict |
|---|---|---|---|
| the Mac's step-65 demo (512×320, 480 frames) | max 1.95e-3, mean 3.0e-7 — PASS | max 1.41, mean 3.0e-3 — diverged | GREEN |
| the Pixel's lab (640×287, 2 396 frames) | max 1.03e-2, mean 6.3e-5 — PASS (its native figure on the Mac) | ink max 0.67 — diverged | GREEN |
| the iPad's lab (640×445, 1 223 frames) | max 5.5e-2, mean 8.2e-3 — the gap the Mac shows natively | diverged | RED, as #24 predicts |

The browser on the Mac sides with the Mac: one more reading for the iPad's
open question (#24).

## For the author

* Phase 9's last step. The tag `v2.0.0-alpha.4` and the fold of
  `_work/DECISIONS_8.md` into `docs/DECISIONS.md` Part VIII are yours, the
  day you say; the evidence folders of steps 60–66 leave the tree at the
  fold, as before.
* The gallery's links (Phase 10's docs): `/marble/?replay=<url of a
  .sumireplay>`; `&overlay=1&layout=N` draws the layout over it.
* A desktop session with a Suzu block does not round-trip byte-identically
  through the page (no Voxo there): the page keeps everything else.

| tool | what it did |
|---|---|
| `emcmake cmake -B build-web`, `cmake --build build-web` | the wasm with `replay/` |
| `node tools/web_gate.mjs` (field, scenes, preset, fullshots, replay ×3) | every gate above |
| `cmake --build build`, `ctest` | the desktop after the `gmtime` change: 11/11 |

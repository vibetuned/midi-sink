# Step 67, the app fixes — the fitted sounds in the app, the orbit trace on the tablets (`DECISIONS_9 #10–#11`)

## Built

* **Voxo 0.16.0** — the patch table (`voxo_suzu_patch_count/_name/_patch`,
  `voxo/src/suzu_patches.cpp`): the tablets' five and the six VCSL fits,
  field for field from `presets/*_vcsl.json`.
* **`trace/`** — the orbit trace as a host library: the desktop's step-59
  `orbit_trace.{h,cpp}` moved under a pure-C surface (`sumi_trace.h`, the
  Swift module `SumiTrace`), linked by the desktop, the iPad and the Tab.
* **Desktop**: *Suzu patch* + *Load patch* in the Sound section, a *Suzu
  attack (s)* knob (INI `suzu_attack`).
* **iPad**: the picker lists the table; *Synth trace* rows (ink, canvas
  scope); the trace runs before each live tick's update; the ink segments
  reach the recorder; lab args `--trace-ink`, `--trace-canvas`.
* **Tab**: the same one for one (`nativeSuzuPatchNames`,
  `nativeTraceConfigure`, the render loop's `sumi_trace_frame`); lab extras
  `--ei traceInk`, `--ei traceCanvas`.
* **Presets**: the synth block's mirror gained `jet_area`, `jet_offset`.

## Gates

| Gate | Result |
|---|---|
| `ctest --test-dir build` (Debug, this Mac) | 11/11 — gate 28 pins the table to the six files; the ABI test at 0.16.0 |
| `cmake --build build-ios` + `xcodegen` + `xcodebuild` (Debug, the iPad) | BUILD SUCCEEDED, `libsumi_trace.a` linked |
| `./gradlew assembleDebug` | BUILD SUCCESSFUL (1 m 10 s) |
| `npm run build` (the site) | 66 pages, 0 problems |

## Device runs — DONE (both tablets, the fingering demo on the trumpet layout, Suzu the source)

| Capture | Device | Patch | Routes |
|---|---|---|---|
| `ipad_trumpet_trace_over.png` | iPad Air M4 | Trumpet | ink on, scope over the water, scale 1.0 |
| `ipad_tubular_bells_scope_over.png` | iPad | Tubular bells (VCSL) | ink on, scope over the water, scale 1.0 — the amber portraits at the partial cells over the stirred ink |
| `ipad_tubular_bells_scope_alone.png` | iPad | Tubular bells (VCSL) | scope alone: the portrait on the dark glass at its cell |
| `tab_trumpet_trace_over.png` | Galaxy Tab S8 Ultra | Trumpet | ink on, scope over — the Tab's session is in Anod: the charge glows at every partial the demo strikes |
| `tab_tubular_bells_scope_over.png` | Tab | Tubular bells (VCSL) | ink on, scope over the glass |
| `tab_tubular_bells_scope_alone.png` | Tab | Tubular bells (VCSL) | scope alone: the portraits at three cells |

The patch names reached both pickers from the table (`[voxo] source 1,
suzu patch 7 (Tubular bells (VCSL))` in the Tab's log; the iPad's lab
argument `--suzu-patch 7` resolved the same name). The first run, at the
desktop's default scale 0.25, showed the modal voices' portraits as dots:
the *Trace scale* row was added to both Sound pages and the captures above
are at 1.0.

# Evidence — Step 44a: the iOS shell (medium, palettes, substrate, presets, prints)

ROADMAP_5 Step 44 (the author's 44a); MEDIUM §1, QOL §1–§4 and §6 (copy).
Decision: `_work/DECISIONS_5.md` #73. Machine: the author's Mac; the device
is the author's iPad Air 11-inch (M4), iPadOS, a Debug build installed with
`devicectl`.

## What landed

* **One session** (`ios/Sources/Session.swift`): every core param, the custom
  palette, the CC map, the input dialect, the routed controls and the strip's
  wheel assignments, persisted through the shared C serializer
  (`import SumiPreset`, `presets/include/module.modulemap`) —
  `last_session.json` restored at launch, named presets in
  Documents/Presets (visible in Files). The 1.0 @AppStorage rows migrate once.
* **The canvas applies the session whole** (`SumiCanvas.swift`
  `applySession`), replacing the 1.0 per-field setters.
* **Settings pages** (`ios/Sources/SettingsPages.swift`): Palette (library,
  load into custom, stop editor, depth / drift / clear), Substrate (paper or
  glass & glow), Presets (save, load, delete, share, import from Files,
  export the session), Prints (the ledger), Operators (Chladni, burst,
  spark, Chirikov); the sheet's "Medium & look" and "Expression routing"
  sections carry the medium switch and the 1.1 modes.
* **The print ledger** (`ios/Sources/PrintLedger.swift`): dips keep their
  field; re-export at Screen / 2K / 4K / 8K, Anod over alpha, to
  Documents/Prints and the share sheet; the newest print straight to Photos.
* **The copy**: "Dip the paper — keep the print" / "Clear the canvas —
  discard".
* **The play surface follows the medium** (the author's first look on the
  iPad): on Anod the joysticks, lattice, echo highlight and hover ghost draw
  white over a dark halo and the control strip turns to translucent smoke
  with white marks — they were black, invisible on the glass.

## Every new setting is reachable on the iPad

| Setting (Phase 6) | Where |
|---|---|
| medium | Canvas sheet → Medium & look (segmented) |
| active palette, library, custom editor (stops, depth curve / floor, hue drift, drift colour, clear water) | Medium & look → Palette |
| paper tint (presets + custom), paper preset, roughness, fiber scale | Medium & look → Substrate (Sumi) |
| glass darkness, phosphor grain, glow bloom, glow reach, glow scale, grid lines, strike charge | Medium & look → Substrate (Anod) |
| presets: save / load / delete / share / import / export | Medium & look → Presets |
| print ledger: export size, Anod over alpha, export, save to Photos | Canvas → Prints |
| Chladni stir / balance / mode / cell size; burst age / life / order; spark shear / decay / octaves / profile / frequency; Chirikov K max / periods / drift / throw | Medium & look → Operators |
| bend (6 choices incl. Chladni stir), channel pressure (4), slide (4), vortex profile (3), torsion sweep | Expression routing |
| CC map targets 14–19 and the default handles 104–109 | CC map |

## DONE (step 44a)

| Criterion | Result |
|---|---|
| every new setting is reachable on the iPad | the table above; the build installed on the author's iPad |
| a preset made on the desktop imports and renders the same | the author's desktop session (`desktop-anod.json`) loaded on the iPad and written back **byte-identical**; the same six strikes at 2360×1640 — `desktop_print.png` / `ipad_print.png`: glass 0.44 / 0.47, glow RGB (101,145,106) / (101,146,107), the same tiles lit (`compare.log`). Filament shapes differ by the frame clock (iPad real-time 60 Hz vs the harness's scripted 1/120 s) |
| the byte path is untouched | no diff in `hostmpe/`, `MidiSource`, `MidiOutputs`, the play overlay or the strip view; `ctest` 5/5 (`hostmpe_tests`, `normalizer_tests`, `preset_tests`) |

The comparison used temporary launch/harness hooks (an iPad
`-evidencePreset` launch argument, a desktop `--preset-render`), removed from
the tree after the capture.

## Flagged

* ROADMAP_5 numbers the steps 44 iOS / 45 Android / 46 web; the author's
  numbering is 44a iOS, 44b web marble, 45a Linux, 45b Android, 46 Windows.
* ROADMAP_5's "About shows libsumi 1.0.0": About shows `sumi_version()`,
  now 1.1.0.

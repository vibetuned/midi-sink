# Step 77 — The Boehm flute, the split surface and preset-next (2026-10-18)

libsumi **1.12.0**, additive — THE LAST CORE-OPEN STEP of the v3.0 arc; the
core is frozen from step 78 to step 92 (`DECISIONS_11 #4`;
`_work/ROADMAP_6.md` step 77; `_work/TO_PROJECT_SPEC.md` §14.7 and §15.4 as
shipped, §5.3's 1.12.0 line).

## What shipped

* **`SUMI_LAYOUT_BOEHM` = 13** (`core/src/layouts.cpp`): nine keys in
  `sumi_layout_state_t.buttons` bits 0..8 — L1, L2, L3 on the valves' CC
  110–112, the thumb, the G♯ lever, R1, R2, R3 and the D♯ key on
  `SUMI_CC_KEY_*` 114–119 (`SUMI_LAYOUT_KEYS` 9), decoded by the normalizer
  under the valves' global rule; three REGISTER cells on the brass geometry
  (the column or the arc, `trumpet_arc`) sounding the chart note + 0 / 12 /
  24; the chart **`sumi_layout_boehm_note`** — the idealised flute's vent
  rule with the mechanism's two linkages (L2 and R1 close the B♭ hole: the
  "1 and 1" B♭) and the one cross fingering (R3 alone under an open F
  hole: F♯), a total function of the 512 states onto D4..C♯5; the
  embouchure bend the brass's +x axis.
* **The split surface**: `params.split_mode` / `split_layout` /
  `split_note` — two Play layouts side by side at half width, the notes
  divided at the split note (the left below, the right from it); the probe
  by the touch's half (a cell across the split refused), the placement and
  the axis by the note (the axis's x halved back into the full frame), the
  cells both halves'; a roll or FIFTHS on either side switches it off. The
  public queries wrap unsplit bodies (`probe_one`, `position_one`,
  `cells_one`, `delta_one`) that call each other, never the public names.
* **hostmpe**: `hostmpe_strip_key_press / _release / _keys / _key_cc`
  (`HOSTMPE_STRIP_KEYS` 9; the valves are keys 0..2), the announce of
  fifteen (the nine keys then the slide), the reset lifting every key;
  **preset-next** `hostmpe_strip_preset_set / _count / _next`
  (`HOSTMPE_PRESET_MAX` 32) in the quick-switch's shape; `HOSTMPE_QUICK_MAX`
  14.
* The desktop: the layout combo of fourteen ("Boehm flute (keys)"), the
  *Split surface* checkbox with the *Right half* combo and the *Split note*
  slider, "Registers on an arc" under the flute, the INI's `split_*`, the
  preset's three fields; the web: the layout in the list, nine key toggles,
  the overlay's "keys" line, `?layout=13`; the iPad's and the Tab's pickers
  name it *(playable)* — their key panels, split surfaces and preset pads
  are their own sub-steps.
* The book: the instruments guide's *Boehm flute* (the chart) and *split
  surface* sections, the layouts table's row 13, the settings reference's
  rows, the MIDI chart's sentence and a conditional row for CC 114–119, the
  control strip's key pads and *Preset* row.

## Gates

| Gate | Result | File |
|---|---|---|
| `normalizer_tests` — the Boehm goldens: the twelve standard fingerings, the linkages, the cross fingering, the 512 states in D4..C♯5, three register cells on the column and arc goldens at two aspects, every fingering × every register through the probe and back through the placement, a NULL state = C♯5, the unreachable note on the nearest register, the key CCs on the master and refused on a member; the split: the placement by the note, the probe by the touch and refused across the split, the cells of both halves, the axis halved, a roll switching it off | 46 820 checks passed | `normalizer_tests.txt` |
| `hostmpe_tests` — the keys (change-only, the valves' mask the first three bits, a tenth key refused), the announce of fifteen, the reset, preset-next's cycle, head and empty | 3 175 checks passed | `hostmpe_tests.txt` |
| `abi_c_compile` — 33 symbols at 1.12.0; `SUMI_LAYOUT_BOEHM` 13, the CC constants, the chart at the two ends, the probe under two fingerings | OK | `abi_c_compile.txt` |
| `hostmpe_c_compile` — the seven step-77 symbols, the constants | in ctest | `gates/ctest.txt` |
| ctest | 14/14 | `gates/ctest.txt` |
| the frozen paths | the field fixture within its tier; the Sumi composite and the Anod golden bitwise on Metal; the web gate green | `gates/` |
| `chart_check.py` | green, no undocumented outputs; the CC 114–119 row conditional | `chart_check.txt` |

## Found in the building (`DECISIONS_11 #4`)

* The brass column's radius is half the smaller of the row and the touch
  width: three rows are taller than 0.25, so the flute's cells read 0.125,
  and the step-60 golden gained the cap.
* A public layout call inside a layout body applied the split twice (the
  grids' cells quartered their x, the axis too): the bodies now call the
  unsplit `*_one` functions.

## Open for the author

* The phase close: the commit, the fold of `_work/DECISIONS_11.md` as Part
  XI, the tag `v3.0.0-alpha.2`.
* Carried: the layouts' microtonal mapping (§14.6); a two-zone split (the
  MPE lower/upper zones as the two hands) as a 4.0 item; the tablets'
  panels, split surfaces and preset pads; the chart row's log.

# Step 9 DONE evidence — layout system: chromatic grid & Jankó

Date: 2026-09-01. Spec: PROJECT_SPEC_2.md §3.4/§5.3. All GUI runs
`METAL_DEVICE_WRAPPER_TYPE=1`, zero validation output.

New: core/src/layouts.cpp (pure (note, params, aspect) -> 1..3 echo
positions, SUMI_MAX_ECHOES = 3, + dormant field-motion hook),
SUMI_LAYOUT_CHROMA_GRID (1 echo) + SUMI_LAYOUT_JANKO (fully-fed triplets:
all three parity rows), params v0.2 (sumi_layout_t, bpm, roll_speed),
sumi_version() -> 0.2.0, layout-derived glide axis (shortest-neighbor),
harness key L cycles layouts. Echo-set rules per spec §3.4: voices own their
echo set, dynamics fan out, counter ticks once per VoiceBegin, budget
reservation is all-or-none per set (DECISIONS_2 #8b).

| DONE check | Result | Evidence |
|---|---|---|
| Golden-position tests pass | **PASS** — independent spec-coded reference vs layouts.cpp: 128 notes × 3 layouts × 2 aspects, exact match + on-canvas + purity (2 820 total checks green); spot checks: C1 top-left, B7 bottom-right, edge-row clamping keeps pitch class, Jankó stagger/whole-tone geometry, rolls fall back to fifths | tests `test_layout_golden_positions` |
| Live switching never crashes/teleports voices | **PASS** — `--cycle-visuals` cycles all 3 layouts every ~1.8 s through 13 s of osmose MPE stress: 98.3 fps, 0 dropped, no crash; unit test proves a voice begun under fifths glides from its ORIGINAL position after switching to Jankó mid-note, and a new note places per the new layout | `run_live_switch.log`; test `test_layout_glide_axis_and_live_switch` |
| Chromatic scale: clean raster in CHROMA_GRID, stagger in JANKO | **PASS** — C1..B7 sweep paints a left-to-right top-to-bottom 7×12 raster (`raster_chroma_grid.png`); in Jankó the sweep lights the FULL 6-row lattice (three echo bands per parity, `raster_janko.png`); a sparse-interval run shows the discrete staggered triplet lattice (`raster_janko_sparse.png`; column merging at chromatic density per DECISIONS_2 #8) |  |
| Held Jankó note: 3 aligned drops, shared parity/hue, lockstep growth, shared bend drag | **PASS** — one held note swelled then bent: three vertically aligned lobes, identical band and hue, identical size, identical drag wakes along the lattice vector (`janko_triplet.png`); unit-tested: counter ticks once, feeds/tines emit in multiples of 3, budget all-or-none (budget 2 -> nothing, raise -> full triplet), 3 lift rings | `run_janko_voice.log`; test `test_janko_echo_sets` |
| Osmose stress in JANKO (3× passes) holds 60 fps, budget merges within echoes | **PASS** — step-5 stress script under layout 2: **98.3 fps**, 0 dropped, no partial echo sets (echo emissions always multiples of the set size; atomicity unit-tested) | `run_janko_stress.log` |
| Glide follows the local pitch axis per layout | **PASS** — grid layout, notes on different rows bent oppositely: each drop travels horizontally ALONG ITS ROW with a wake (`glide_along_row.png` — dip-print export; the on-screen route was used until the display slept). Unit tests assert horizontal axis in grid incl. the B-note row-wrap case (shortest-neighbor rule) | `run_glide_grid.log` |

**Found on the way** (DECISIONS_2 #9): the original glide demo bent two notes
on the SAME grid row — their colinear infinite-line tines cancel almost
exactly. Not a bug: lateral tine locality comes from alpha; longitudinal
locality does not exist in the Jaffer form.

ABI: version 0.2.0 gates the grown params struct; abi_c_compile exercises the
new fields and enum in strict C11.

Regressions: 6 241 headless checks green; osmose stress 98.3 fps / 0 dropped;
fifths layout byte-identical behavior (golden + fifths glide print).

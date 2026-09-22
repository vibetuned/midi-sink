# Step 42 — the binding tables as shipped, and the ITERATE ledger

MEDIUM §4's table, as the core resolves it (`eff_modes`, DECISIONS_5 #51).
**Unsigned:** the author signs the Anod column by eye after the hour on the
ROLI Piano + Airwave; every cell is a default the CC map and the explicit
modes override.

| Dimension | Sumi default (as 0.x) | Anod default (shipped) | Override |
|---|---|---|---|
| strike (note-on) | the drop | the spark composition: the drop + a burst of core = the drop radius, lobes along the note's pitch axis, order from the pitch class (`burst_order_by_class`, naturals 2 / accidentals 3), + the spark shear episode | none — the medium's |
| press feed (0xD0) | boundary growth (`press_mode` 0) | the torsion sweep FEED (`press_mode` 2): pressure spends torsion deltas around the note, 1.2 rad/s at full pressure, its own phase clock | `press_mode` 0 / 1 |
| swirl (0xA0 poly pressure) | Lamb–Oseen swirl | the Chladni stir: the loudest voice's pressure sets `SUMI_CTL_CHLADNI_A` | a CC mapped to CHLADNI_A |
| per-note bend | glide (`bend_mode` 0) | the torsion's wavenumber (`bend_mode` 2): ±1.5 semitones span `SUMI_CTL_TORSION_K` | `bend_mode` 0 / 1 / 3 |
| CC 74 (slide) | aux hue (`slide_mode` 0) | the spark's frequency (`slide_mode` 2) | `slide_mode` 0 / 1 |
| mod wheel / breath | the vortex | the Chirikov throw: the VORTEX_STRENGTH dimension's deltas, the vortex quiet | the CC map (route the wheel elsewhere) |
| master bend | the shear tine | the shear tine (unchanged; see the ledger) | — |

## The `[ITERATE]` ledger (MEDIUM §1, §3, §4)

| Where | Question | Resolution |
|---|---|---|
| §1 | the medium's name | Anod (#1) |
| §1 | live switch: a feature, or a dip forced between media? | **A feature** (#53): the field is untouched by the switch (bitwise, measured), the re-read is the point; the dip stays one key away for whoever wants a fresh sheet |
| §3 | substrate design (glass grain, phosphor speckle) | the washi's simplex grain, screen-locked, on near-black — a phosphor speckle at the `paper_roughness` strength; the darkness/grain knobs are step 43's (#50) |
| §3 | long-exposure look, a strain accumulation buffer? | **carried to the Phase-9 beta with its question** (#54): the field already accumulates the map; a separate buffer would be a second history. Decide after playing whether a print wants a time integral |
| §4 | press feed: the torsion sweep feed, or not | shipped as the sweep feed; **the hour of playing decides** (#51) |
| §4 | master bend: a scroll-compatible shear | the tine as it is: it composes with the scroll already (it is exact and shears along rows); nothing scroll-specific was needed (#51) |
| §4 | the table most likely to change after the first hour | expected; the modes' "Medium default" keeps the user's overrides separate from the table's revisions (#51) |
| step 42 | the drop-edge question: every ring boundary and seam will glow | rings' rims **glow by design** (the compression is real strain; a lone drop's interior stays dark, its rim reads 107 against a 27 substrate); seams **do not** (the ingress mask, #52). Taste to be confirmed by the author |

## Revised at step 43 (DECISIONS_5 #70, 2026-09-23)

After the hour of playing the author swapped two rows of the Anod column;
the table above is what step 42 shipped, this is what stands:

| Dimension | Anod default (from step 43) | Override |
|---|---|---|
| per-note bend | the Chladni stir (`bend_mode` 4): the bend's distance sets `SUMI_CTL_CHLADNI_A` (±1.5 semitones saturate), its sign the sense — a vibrato stirs back and forth | `bend_mode` 0 / 1 / 2 / 3 |
| swirl (0xA0 poly pressure) | the torsion's and the spark's wavenumbers from their rest at mid-range up (k = ½ + ½·pressure), each unless the bend, the slide or a CC owns it — so under the default column the torsion's alone (the slide holds the spark's) | a CC mapped to TORSION_K / SPARK_K; `slide_mode` |
| strike (note-on) | the SPARK (#71): a charge of `anod_drop` × the Sumi drop (default a third) + the spark shear episode on the full Sumi radius — the burst left the composition (`anod_strike_before.png` / `_after.png` in `step43/`) | `anod_drop`, `spark_shear` |

**#72 (the same day):** "a CC mapped to the dim overrides" is withdrawn on the stir and the wavenumbers — the desktop maps CC 106 / 104 / 108 to them by default, which had left the routes dead. Last writer wins; pressure holds the wavenumbers and gives them back at release; the stir stills when the last voice lifts or the mode flips away.

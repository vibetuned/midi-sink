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

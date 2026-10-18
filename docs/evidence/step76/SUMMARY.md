# Step 76 — Anod finished: the deterministic seed, the strain buffer, the Chirikov grid and the spark reading (2026-10-14)

libsumi **1.11.0**, additive (`DECISIONS_11 #1`; `_work/ROADMAP_6.md` step 76 —
Phase 12, the carry-overs, moved directly after Kessho by `DECISIONS_10 #13`;
`_work/TO_PROJECT_SPEC.md` §10.10 marked with what shipped; the spark reading
from `_work/specs/ROADMAP_6_delta_phase_order_and_spark.md` §B).

## What shipped

* **The seed** (`DECISIONS_7 #25` answered): `sumi_set_seed` / `sumi_get_seed`
  — the mapper's strike-phase generator, one word; the set also rebases the
  mapper's gesture clocks (the torsion sweep's phase, the stir's pending
  rotation, the running episodes, the smoothers), measured necessary. The
  replay header's `seed` line (schema 1, additive), applied by the player;
  the desktop's and the bench's recorders write it.
* **The strain buffer** (SPEC §10.4's carried question): an RGBA16F side
  target stepped once a frame by `composite.glsl`'s third program — σ, the
  fast activity (the strain's rate over 50 ms), the slow activity (over
  `spark_afterglow`), the exposure EMA. The stencil `kessho_strain` is one
  `@block` with three readers; the plasma keeps its inline copy, bitwise.
  Kessho's Layer A reads the exposure.
* **The spark reading**: `params.anod_glow_mode` (plasma / spark),
  `spark_afterglow`; the ridges of σ across ê_ψ⊥, the leader from the fast
  activity, the afterglow from the slow, the bloom on the leader, the
  flicker from the screen-locked speckle; the branch trick retired at the
  viewing (`DECISIONS_11 #2`) — branches are the leader stroke's, step 76b.
  Charge only.
* **The Chirikov grid**: `--chirikov-grid <dir>`, 27 renders and an index
  (`chirikov_grid/`), the author's choice pending.
* The bench's `--seed-test`, `--spark-reading-test` (`--ab-out`),
  `--chirikov-grid`, `--dump-medium`; the Anod golden
  `tests/fixtures/composite_512_metal_anod.rgba` and `composite_gate.py
  --medium`; `sumi_debug_set_spark_nms`, `sumi_debug_read_strain`; the
  desktop's *Glow reading*, *Spark afterglow*, *Spark branches*; the INI, the
  preset (`SCHEMA.md`); the Anod page's spark section, two citations, the
  settings rows, the replay guide's line, the minerals page's exposure line.

## Gates

| # | Gate | Result | File |
|---|---|---|---|
| 1 | an identical gesture script → bit-identical Anod AND Kessho fields | the script twice from seed 7: Sumi, Anod, Kessho each 2 097 152 bytes, 0 differ; the red (no reseed) differs in 1.07 M bytes in Anod; the word read back and set back reruns bitwise | `seed_test.txt` |
| 2 | the `--replay` gates hold | the bench's recording (480 frames, with `seed`) replays GREEN on the desktop tier, the 60 Hz re-bucketing diverging; the mobile tier is the tablets' sub-steps' | `replay_gate.txt` |
| 3 | the Chirikov grid, the author's choice | 27 renders at K 0.3 / 0.6 / 1.0 × periods 2 / 4 / 8 × ε 0.15 / 0.3 / 0.6; the choice pending | `chirikov_grid/` |
| 4 | the strain buffer feeds Kessho Layer A with no change to Layer A's gates | Layer A reads the exposure; **open** — the Kessho bench gates were not rerun against it (they need a settle of three time constants), `[ITERATE]` | — |
| 5 | plasma bitwise; the reading a reader | the plasma prints bitwise with the suppression switched off (0 bytes); the field under the spark reading bitwise with the suppression on and off (0 bytes; the print differs in 14 014) — since 76b the strike under mode 1 strikes its stroke beside the shear episode, so the plasma run's field differs from the spark run's (7.9 M bytes); the Anod golden bitwise | `../step76b/spark_amended/spark_test.txt`, `gates/composite_gate_metal_anod.txt` |
| 6 | thinness | **the stroke's since 76b** (`DECISIONS_11 #2`); the swirl scene's figure kept for the record: 2.48 texels, the suppression off 6.0 | `../step76b/` |
| 7 | sparsity ≤ 5 %; the plasma beside | the spark lights 0.99 % of the canvas above the bright threshold, the plasma reading of the SAME field 1.79 % (≤ 5 %, ≤ 0.6× the plasma) — the strokes' charged channels since 76b | `../step76b/spark_amended/spark_test.txt` |
| 8 | the afterglow decays to 1/e in `spark_afterglow` ± 5 % | 10.54 at the press end, 3.78 one time constant later: 0.3584 (1/e 0.3679) — rerun at 76b after the strain pass's half-precision fix (`DECISIONS_11 #3`) | `../step76b/spark_amended/spark_test.txt` |
| 9 | branches; red: the grain off | **retired at the fold of 2026-10-15** (`DECISIONS_11 #2`): the trick cannot make endpoints on a ridge it did not have — the first build measured 11.6 → 11.7 per 100 texels with a dither, 16.2 with a cut; branches are geometry, step 76b's | `spark_first_build/` |
| 10 | the grain screen-locked under a 120 BPM scroll with the flicker on | the glass prints bitwise (0 bytes), the grid off | `spark_test.txt` |
| 11 | the A/B viewing | **the three-way viewing of 76b** (`../step76b/leader_ab/`); the first build's `spark_first_build/` kept; viewed by the author 2026-10-18: plasma stays the default | `spark_first_build/`, `../step76b/leader_ab/` |
| 12 | the plateau, the thread's red control | the press scene's blobs under the spark reading keep 47.7 % of their lit texels with the suppression on (511 off), 100 % within 2 texels of the plasma silhouette; raw σ (max − mean)/max = 0.53 — terraces (the fold's note) | `../step76b/spark_amended/spark_test.txt` |
| 13 | the torsion scene thinned | the rings 1.91 texels under the reading (3 072 over a skeleton of 1 608); the suppression off 3.9 (≥ 2×) | `../step76b/spark_amended/` |
| — | the frozen paths | the field and the Sumi composite fixtures bitwise on Metal; ctest 14/14; the web gate green | `gates/` |

## Open for the author

* The viewing (2026-10-15): the strikes read the same under both readings — a strike's σ is a plateau; the line is written by step 76b (the delta folded, `DECISIONS_11 #2`).
* The Chirikov grid's choice (the defaults follow it).
* The spark's constants: the reference 1.5·`anod_glow`, the leader's 0.01, the afterglow's 0.3 and the channel's gain 0.4 (0.55 here; lowered at 76b for the leader's contrast), the flicker 15 % at 7 Hz — all `[ITERATE]`.
* Whether Kessho's Layer A should read the exposure at all (a time constant on the fringes), and its gates rerun against it.
* The tablets' recorders' `seed` and their Glow-reading rows.

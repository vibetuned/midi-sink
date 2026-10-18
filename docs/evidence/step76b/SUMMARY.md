# Step 76b — The leader stroke (2026-10-15; the author's viewing folded 2026-10-18)

libsumi **1.11.0** (the same additive version as step 76: `sumi_add_leader`,
`SUMI_DEFORM_LEADER` = 17; `DECISIONS_11 #3` and its amendment;
`_work/ROADMAP_6.md` step 76b, the delta `ROADMAP_6_delta_76b_leader_stroke.md`
folded by `DECISIONS_11 #2`; `_work/TO_PROJECT_SPEC.md` §10.10 as shipped).

## What shipped

* **The operator** `SUMI_DEFORM_LEADER` (`core/src/displacement.h`,
  `leader_fs` in `deform.glsl`, `pip_leader`): one sub-step of up to eight
  segments, each a slip along its own direction in a C² quintic window
  across (half-width 0.003 canvas heights) and along (ramps of 2ℓ past both
  ends), composed last-first in one lookup; class sub-stepped — across a
  pure shear, along the ramps' compression det J ≥ 0.53 at the per-pass
  slip ≤ ramp/4, which at these constants is one sub-step at every
  velocity. **The channel is charged:** within the half-width of the path
  the pass writes the strike's ink band and aux — the leader is the
  ionized path (the first build walked into dark water: the reading lights
  charge only).
* **The walk** (`sumi_voice_mapper_add_leader`): ℓ = 0.012, ±30° about the
  strike's pitch axis, p_b = 0.15 at ±45° with half the parent's count,
  depth 3, N = 8 + 32·v; a 32-bit LCG on the strike's draw from the seed.
  Under `glow_mode` 1 the Anod strike (`add_spark`) strikes one **beside
  its blast and shear episode** (the author's viewing: replacing the
  episode lost the impact); mode 0 keeps the strike bitwise. **The
  episode:** 120 sub-steps a second (a fractional credit on dt), every
  alive branch together, one pass per stroke per frame; a refused budget
  waits.
* `sumi_add_leader(inst, x, y, dir_x, dir_y, energy, seed)` on the gesture
  route; `sumi_debug_add_leader_seg` / `_set_leader_pb` /
  `_set_leader_charge` / `_leader_count`; `SPARK_AFTER_GAIN` 0.55 → 0.4.
* **The strain pass fixed** (a step-76 bug found by gate 5): σ compared in
  the buffer's own half precision — a float σ against its half-rounded self
  left a permanent activity floor on strained texels.
* **The desktop's *Drop & tine shape* section**: the drop's habit and the
  tine's profile for every medium, out of Kessho's Mineral panel (the
  author's).
* The bench: `--soak leader`, `--leader-test` (`--ab-out`), `--leader-pb`;
  the amended step-76 gates (12) the plateau and (13) the torsion scene in
  `--spark-reading-test`; `tests/abi_c_compile.c` lists `sumi_set_seed`,
  `sumi_get_seed`, `sumi_add_leader` (32 symbols).
* The book: `operators/leader.mdx` (sidebar, index); the Anod page's Spark
  section rewritten around the stroke with the plateau as the "why" and
  the FTLE line kept beside it; the settings reference's row and its new
  *Drop & tine shape* section.

## Gates

| # | Gate | Result | File |
|---|---|---|---|
| 1 | `--soak leader`, the sub-stepped class | (b) det min 0.763, mean 1.00000 everywhere after one sub-step; 500 pairs 0.15 texel of pre-image drift, mass −0.88 %/+0.00 %; (c) the stream +20.6 % over 6000 passes, 3.4·10⁻⁵ a pass (the far ramps' magnification; ≤ 5·10⁻⁵, the class's bar); (d) erosion −3.4·10⁻⁵ against the control's 1.15·10⁻⁵ — the charge off under the soak (a deposit, excluded by nature) | `soak_leader.txt` |
| 2 | determinism with strokes | the press scene twice from seed 7: 16 588 800 bytes, 0 differ; the plasma strikes' field differs in 7.9 M (the stroke beside the episode under mode 1) | `leader_test.txt` |
| 3 | thinness ≤ 3 texels; red: the plateau | one stroke's lit ridges beyond its strike's blob (2.5 r) 1.70 texels (1 582 over a skeleton of 929); the blob under the same reading keeps 31.9 % of its lit texels with the suppression on (1 518 off, 1 698 under plasma) — the roadmap's 80 % was a guess before the measure; a thinned line keeps a few percent | `leader_test.txt`, `leader_ab/blob_under_spark.png` |
| 4 | branches; the single leader; red: the blobs | on the charge's skeleton of six strokes ALONE (the gesture route), spurs pruned: p_b 0.15 ends in 3.17 tips a stroke, 1.83 more than p_b 0's 1.33 (a single leader's two ends, less what the thinning takes; 0.62 per 100 texels of channel — the roadmap's 4 per 100 was the lit mask's, which counts the flicker's dashes); the strikes' torn charge under plasma 0.09 endpoints per 100 texels | `leader_test.txt`, `leader_ab/grid_pure_pb015.png`, `grid_pure_pb0.png`, `grid_pb015.png` |
| 5 | the afterglow on a stroke; the leader ≥ 3× the channel | on a stroke alone: the slow activity 3.42 where it peaks at frame 80, 1.23 one time constant later: 0.3589 (1/e 0.3679; 0.44 before the strain pass's fix); at frame 30 the leader 0.723 against the channel's crest 0.097 in linear light, the bloom off: ×7.5 (×2.6 at the gain 0.55 on the first measure); the stroke's passes a frame at most 1 | `leader_test.txt`, `leader_ab/stroke_pure_f40.png` |
| 6 | velocity → reach, charted | v32 22 frames / 78 charged texels beyond the blob, v64 31 / 2 520, v96 47 / 4 944, v127 51 / 5 939 — both monotone | `leader_test.txt`, `leader_ab/stroke_v127.png` |
| 7 | the pass budget at fifteen voices | 15 alive at once; over 320 frames the storm costs 3 697 passes against the same storm's 2 985 under plasma: the strokes' 712, one pass per stroke per frame of its life (15 × 40 … 15 × 55); the first frames 60 / 45 / 45 / 45 against 45 / 30 / 30 / 30; both storms peak at frame 31 (90 and 60: the shear episodes' burst, the strokes' 15 and the budget's re-timing on top); the last stroke walked out at frame 55 | `leader_test.txt`, `leader_ab/storm_f30.png` |
| 8 | the three-way viewing | `leader_ab/plasma_f40.png` / `_f100`, `leader_f40` / `_f100`, `leader_pb0_f40` / `_f100` on the press scene at the defaults; `stroke_one.png` the figure (a strike, its blast and streamers, its stroke); **viewed by the author 2026-10-18: the walk kept, plasma stays the default** | `leader_ab/` |
| 12 (76) | the plateau | the press scene's blobs keep 47.7 % with the suppression on (511 off), 100 % within 2 texels of the plasma silhouette; raw σ (max − mean)/max = 0.53 — terraces | `spark_amended/spark_test.txt` |
| 13 (76) | the torsion scene thinned | the rings 1.91 texels under the reading (3 072 over 1 608); the suppression off 3.9 | `spark_amended/spark_test.txt`, `torsion_spark.png`, `torsion_nms_off.png` |
| — | the amended step-76 set | `--spark-reading-test` 7/7: 5a, 5b (the reading a reader: the field bitwise with the suppression on and off), 7 (the plasma reading of the same field: 0.99 % against 1.79 %), 8 (0.358 after the strain fix), 10, 12, 13 | `spark_amended/` |
| — | the frozen paths | the field fixture within its tier, the Sumi composite and the Anod golden bitwise on Metal; ctest 14/14; the web gate green — all rerun on the final build | `gates/` |

## Open for the author

* The constants ℓ, h, p_b, the rate, the gain, the ramp (a gentler 3ℓ
  would halve the stream's growth) — `[ITERATE]`; DBM a 4.0 item, the walk
  kept at the viewing.
* The roadmap's gate figures that the measures corrected (`DECISIONS_11
  #3`): the plateau's 80 %, the branches' 4 per 100.

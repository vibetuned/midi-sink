# ROADMAP_6 delta — Step 76b: the leader stroke (2026-10-15)
**Applies to** `_work/ROADMAP_6.md` in place: **Step 76 amended** (what the first build taught), **Step 76b new**, the ABI block, the decisions list. Supersedes B.3 of `ROADMAP_6_delta_phase_order_and_spark.md` (the Lichtenberg operator is no longer recorded-not-built: it is the fix). Core-open window unchanged, 71–77; 76b lands inside it.

---

## Step 76 — AMENDED after the first spark build (2026-10-15)

### What the build showed (`docs/evidence/step76/spark_first_build/`)
The three captures — plasma, spark, spark + branches — have identical silhouettes. Non-maximum suppression thins only a **peaked** profile; the Anod note-on's shear episode has piecewise-constant σ across its width, a **plateau**, so every texel of it is a local maximum and the whole blob passes. The branch threshold against σ + κ·grain then speckles a plateau that is already above threshold. The reader found no ridge because the field had none at those places. **A spark is a 1-D object; the reader can thin a line that is in the field and cannot invent one from a blob. The line has to be written.**

### What stays, what changes
* **Stays, unchanged:** the seed, the strain buffer, the Chirikov grid; the spark *reading* (NMS along ê_ψ⊥, the γ transfer, leader + afterglow from the buffer, the flicker). They were right; they had nothing thin to act on.
* **Changes:** the branch trick (σ + κ·grain) is **retired** — the branches are geometry now (76b); `params.spark_branch` is dropped before it ships. `glow_mode` keeps its two values but the strike *shape* now follows it (76b): a plasma strike is the blob episode, a spark strike is the stroke.
* **Gates, amended:** (6) thinness and (9) branches move to 76b (they are properties of the stroke); (7) sparsity stays; add **(12) the plateau cross-section, the red control of this whole thread:** raw σ across a blob episode is flat (max − mean over the interior < 5 % of max) and spark mode leaves its silhouette within 2 texels of plasma mode — i.e. the first build is reproduced as the documented failure; **(13)** spark mode on the 2.0 torsion scene, where σ *is* ridged, thins the annuli to ≤ 3 texels — the reader proven correct on a ridged field before 76b begins.

Phase 12 now closes after 76b and 77.

---

## Step 76b — The leader stroke (`SUMI_DEFORM_LEADER`) (Linux box) — NEW

* **Class sub-stepped** — the class of the existing windowed local spark (§10.2 class table); no new class debt. The Anod note-on in `glow_mode` 1: instead of the blob shear episode, a **stroke along a jagged tree** from the strike point. **The path:** a biased random walk — step length ℓ ≈ 10 texels `[ITERATE]`, heading jitter ±30° about a drift direction set by the layout (down the key column, or the velocity vector `[ITERATE]`), branch probability p_b ≈ 0.15 per step at ±45° with the length halved per generation, depth 3, N_steps from velocity (soft 8 … hard 40); **deterministic from the step-76 seed** (strike index × seed → the walk's stream), so the gesture-ABI replay is bit-identical. **The shear:** each segment a short windowed shear along its own direction, perpendicular window 2 … 4 texels (C², zero outside), displacement from velocity, sub-stepped per the class rule (per-pass step ≤ a/4 against the window width a, as the local spark does); segments composed in path order, branches after their parent. The σ of a stroke is a genuine **ridge** along the path — the reading of step 76 thins it, the buffer holds the channel, the branches are real. **DBM** (dielectric breakdown, a Laplacian solve per strike) stays `[ITERATE]` — taken only if the walk reads as too regular at the viewing. The blob episode survives as the plasma strike (`glow_mode` 0, bitwise v2). Gesture route: `sumi_add_leader(inst, x, y, dir_x, dir_y, energy, seed)` for the stylus and scripts; the MIDI route derives energy from velocity and the seed from the strike index.

**Gates:** (1) the §10.2 gates for the sub-stepped class under `--soak leader` (the sub-stepped Jacobian gate: det J > 0.5, mean within 2·10⁻³ of 1; ±k inversion; zero fabrication — every segment's window is finite); (2) **determinism:** two runs of one replay → bit-identical fields (the step-76 gate, rerun with strokes); (3) **thinness (from 76):** mean ridge width of a lit stroke ≤ 3 texels at 1080p under the step-76 reading — **red:** the blob episode under the same reading ≥ 12 texels (the first build, reproduced); (4) **branches (from 76):** skeleton endpoint count per 100 texels of ridge ≥ 4 at p_b = 0.15, exactly 1 per stroke at p_b = 0 (a single leader) — **red:** the retired σ + κ·grain trick on a blob gives speckle, not endpoints (skeleton endpoints < 1 per 100 texels of area); (5) **afterglow on a stroke:** the channel decays to 1/e in `spark_afterglow` ± 5 % while the leader frame is ≥ 3× brighter than the channel at t = 0; (6) **velocity → reach:** N_steps monotone in velocity, charted at 32 / 64 / 96 / 127; (7) **pass budget:** a 15-voice MPE storm of strokes at depth 3 stays within the pass table (segments merged into one pass per generation by the windowed-shear batching of the local spark — if it does not batch, the depth drops to 2 and the roadmap says so); (8) **the viewing:** plasma strike vs leader stroke vs leader stroke at p_b = 0 on one replay, archived under `docs/evidence/step76b/leader_ab/`; the Anod default strike per `glow_mode` folded; the DBM decision recorded (walk kept / DBM taken as a 4.0 item).
**DONE when:** all eight green with (3) and (4)'s reds; `field_512_metal.bin` bitwise (the stroke is `glow_mode` 1 only); the ABI history lists `sumi_add_leader`; the Anod page's *Spark* section rewritten around the stroke, with the plateau figure as the "why" and the FTLE line kept for the played canvas (repeated stretching and folding makes filaments on its own; isolated strikes never will — both sentences, side by side).

Evidence: `docs/evidence/step76b/` — the plateau cross-section (raw σ, the blob, flat top); the torsion-scene thinning; the three-way viewing; the velocity reach chart.

---

## The ABI block (amend)

```c
/* step 76: params.anod_glow_mode (0 plasma, 1 spark), params.spark_afterglow (s); params.spark_branch DROPPED before ship */
/* step 76b: the leader stroke — class sub-stepped (the local spark's class) */
SUMI_API void sumi_add_leader(sumi_instance_t* inst, float x, float y, float dir_x, float dir_y,
                              float energy, uint32_t seed);
/* the MIDI route: note-on in glow_mode 1 → a stroke, energy from velocity, seed = strike index ^ the instance seed;
   in glow_mode 0 → the v2 blob episode, bitwise */
```
Dev flags, add: `--soak leader`, `--sigma-raw` (grayscale σ, no transfer, no NMS — the cross-section tool), `--leader-pb <p>`.

---

## Decisions list (amend)

* **#20 — RESOLVED, inverted:** the Lichtenberg operator is not optional; the stroke is the spark. The reader (step 76) stays and is proven on the torsion scene first.
* **#21 (new) — the branch trick retired:** σ + κ·grain speckles a plateau and cannot make endpoints; branches are geometry. Recorded with the first build's captures so no future agent re-adds it as "cheap branching".
* **#22 (new) — walk vs DBM:** the biased random walk is the v3 tree; DBM a 4.0 item unless the 76b viewing says the walk is too regular.
* **#23 (new) — the drift direction** of the walk (key column vs velocity vector vs none): `[ITERATE]` at the viewing.
* **Principle, added to the §10.2 preamble at the fold:** *a reader can thin a ridge and cannot invent one — line-like looks need line-like operators.*

**DO NOT FORGET:** 76b before 77's close of the core window; `spark_branch` out of the header before 1.6.0 is written to the ABI history; the plateau figure into the book.

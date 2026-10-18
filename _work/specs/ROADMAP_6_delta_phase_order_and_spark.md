# ROADMAP_6 delta — Phase order (carry-overs after Kessho) & the Anod spark reading (2026-10-14)
**Applies to** `_work/ROADMAP_6.md` in place. Two changes: (A) the carry-over phase moves to directly after Phase 11, Voxo goes last (`DECISIONS_10 #19`, the author's word); (B) the Anod carry-over step gains the **spark reading** (`glow_mode`). Numbers below are the new final numbers; the old ones in brackets.

---

## A. The order

### A.1 Why it is better than it looks
The carry-over phase holds the only two core changes outside Phase 11: the strain buffer (a side buffer, Anod and Kessho both read it) and `SUMI_LAYOUT_BOEHM`. Moving it here gives **one contiguous core-open window, steps 71–77**, and one freeze, 78–92, across all eleven Voxo steps — instead of reopening the core at the old 91 after nine frozen steps. The strain buffer also stops being a promise: Kessho's Layer A reads it from its second session rather than "instantaneous σ until step 90". And the Lichtenberg operator (B.3) has a seed to stand on before the freeze.

### A.2 Renumbering

| Phase (new) | Steps (new) | Content | Old |
| :---- | :---- | :---- | :---- |
| 11 Kessho and symplectic harmony | 71, 71b, 72–75 | unchanged | 71–75 |
| **12 The carry-overs** | **76, 77** | Anod seed + strain buffer + Chirikov grid **+ spark reading**; Boehm + split surface + preset-next | 90, 91 |
| 13 Suzu foundation | 78–82 | SoA SIMD + ×4 oversampler; membrane; bow; bore v2; tables + Scala | 76–80 |
| 14 The voices | 83–86 | Toda ring; Boris; billiard + Dirac; the chaotic modulator | 81–84 |
| 15 Voxo DS 2.0 | 87–91 | unchanged | 85–89 |
| 16 Release | 92 | unchanged | 92 |

Machines unchanged: 76 Linux box, 77 Windows box, Voxo as the proposal mapped them.

### A.3 Cross-references to fix in place

| Where | Old text | New text |
| :---- | :---- | :---- |
| Preamble, scope paragraph | "the strain buffer (90) after Kessho (71), which reads instantaneous σ until then" | "the strain buffer (76) directly after Kessho (71); Layer A reads it from step 76" |
| Preamble, scope paragraph | "the ×4 oversampler (76) before the Toda ring (81) and the billiard (83)" | "(78) before (83) and (85)" |
| Working rules, core line | "…and at step 91 (`SUMI_LAYOUT_BOEHM`); every other step leaves libsumi untouched" | "The core is open **71–77** (Phase 11 and the carry-overs: libsumi 1.6.0, the strain buffer, `glow_mode`, `SUMI_LAYOUT_BOEHM`) and **frozen 78–92**; the Voxo steps touch it only under bug → regression test → fix" |
| Phase 11 close | "The core is frozen again from here to step 91" | "The core stays open through step 77" |
| Step 71 | "Layer A reads instantaneous σ until the strain buffer of step 90 lands" | "…of step 76 lands" |
| Step 75 | "the Boris gyroscope adds its own when it lands at step 82" | "at step 84" |
| Step 79 [77] membrane | "`sumi_layout_probe`… (gate delta c)" | unchanged; the bus plate "struck at the cell" stays here (DECISIONS #13) |
| Step 84 [82] Boris | "verified in Kessho (gate delta c — Phase 11 landed first)" | unchanged, and the spark reading is available too |
| Phase closes | alpha.1 (11), alpha.2 (12 Suzu) … | alpha.1 (11), **alpha.2 (12 carry-overs)**, alpha.3 (13), alpha.4 (14), alpha.5 (15), rc.N (16) |
| Decisions #1 / #2 | "11 Kessho (71–75), 12 Suzu foundation (76–80), 13 voices (81–84), 14 DS (85–89), 15 carry-overs (90–91), 16 release (92)" | as A.2; add **#19: carry-overs after Kessho, Voxo last — the author's word, 2026-10-14; one core-open window 71–77** |
| Decisions #13 | "bus plate stays with the membrane (step 77)" | "(step 79)" |
| Evidence paths | `docs/evidence/step90/`, `step91/` | `step76/`, `step77/` |

---

## B. Step 76 [90] — Anod: deterministic seed, strain buffer, Chirikov grid, **and the spark reading** (Linux box) — AMENDED

### B.1 Deliverables (existing, unchanged)
DECISIONS_7 #25 (the gesture route reproducible run to run — the seed carried through the gesture ABI); the §10.4 long-exposure strain buffer (σ accumulated in a side buffer; Anod's glow by its own question, Kessho's Layer A unchanged); the Chirikov feel grid (DECISIONS_5 #89).

### B.2 Deliverable (new): `glow_mode` — plasma or spark
Anod reads σ as a **density** — every stretched texel glows in proportion, a sheared region glows uniformly across its area, soft edges, bloom: a plasma. A spark is a different *reading* of the same field: a thin core, a corona, afterglow, branches. All of it is composite-side; the field stays bitwise.

* **`params.anod_glow_mode`:** 0 **plasma** (v2, bitwise), 1 **spark**.
* **Ridges, not sheets.** Non-maximum suppression of σ across the strain axis: a texel survives only if σ is a local maximum along ê_ψ⊥ — ψ from the §10.4 stencil **shared with Kessho** (step 71 already computes it; the stencil becomes one function with two readers). The survivors are the finite-time stretching ridges — the Lagrangian coherent structures of the performance; FTLE ridge plots of chaotic flows look like lightning tangles because they are this object.
* **Sparse:** a steep transfer (σ_ridge/σ_ref)^γ, γ ≈ 4 `[ITERATE]`, lit fraction a few percent of the canvas.
* **Leader and afterglow:** the instantaneous ridge is the leader (bright, white-blue), the strain buffer's accumulated ridge is the hot channel decaying (dimmer, warmer); two gains, two time constants — `params.spark_afterglow` (seconds, the buffer's decay, default 0.6 `[ITERATE]`). The buffer is the afterglow by construction; nothing new is stored.
* **Corona:** the existing bloom, on the leader only.
* **Flicker:** the ridge threshold modulated in time by the phosphor grain's value at the texel — the grain stays spatially screen-locked (§4.5); only its use in time changes.
* **Branches (the reader trick):** the threshold taken against σ + κ·grain instead of σ, κ = `params.spark_branch` ∈ [0, 1]. High-frequency grain crossing a smooth ridge's threshold fragments it into side branches and dead-end spurs. Honest statement, in the book: a smooth det-1 field's ridges fork only at saddles; the dendrite is lighting, not geometry.
* ABI: three fields appended to `sumi_params_t` (`anod_glow_mode`, `spark_afterglow`, `spark_branch`), additive; no CTL (`[ITERATE]` — afterglow on the mod wheel if it wants performing). The Anod default per medium stays plasma until the A/B says otherwise.

### B.3 Recorded, not built: the Lichtenberg operator (`DECISIONS_10 #20`)
Tearing the ink along a real breakdown tree — the Anod note-on spark given a dielectric-breakdown path (DBM, η ≈ 1 a bush, η ≈ 3 sparse lightning) generated at strike time from the seed, each branch a short windowed shear; **class sub-stepped**, the same class as the existing windowed local spark (no new class debt); the seed of B.1 is its prerequisite, which is why it is recorded here. **This step is the last core-open window of 3.0:** if the author wants the torn tree on paper, it is **76b** (same A/B logic as the braid — a viewing decides); otherwise it is 4.0. Not decided today.

### B.4 Gates
Existing: (1) an identical gesture script → bit-identical Anod AND Kessho fields across runs; (2) the `--replay` gates hold on the desktop and mobile tiers; (3) the author's choice from the Chirikov grid recorded; (4) the strain buffer feeds Kessho Layer A with no change to Layer A's gates (step 71's (4) rerun on the accumulated σ).

Spark: (5) **bitwise:** `glow_mode` 0 reproduces the v2 Anod goldens; the field bitwise under every mode (the §10.1 gate: a reader); (6) **thinness:** mean ridge width ≤ 3 texels at 1080p on the swirl soak — **red:** NMS off → ≥ 12 texels; (7) **sparsity:** lit fraction ≤ 5 % at the default transfer (plasma on the same frame ≈ 30 %), charted; (8) **afterglow:** one strike's channel decays to 1/e in `spark_afterglow` ± 5 % (a byte-log assert on the buffer); (9) **branches:** at `spark_branch` = 0 the skeleton has no spurs; at 0.5, ≥ 4 endpoints per 100 texels of ridge (skeleton endpoint count) — **red:** the grain off → zero spurs at any κ; (10) the spatial grain screen-locked under a 120 BPM scroll with flicker on (§4.5 gate reused); (11) **A/B viewing:** plasma vs spark on one replay, archived under `docs/evidence/step76/spark_ab/`, the Anod default per medium folded; the Lichtenberg decision (B.3) recorded: 76b or 4.0.

**DONE when:** all eleven green with (6) and (9)'s reds; `field_512_metal.bin` bitwise; the ABI history lists the three fields; the operator book's Anod page gains a *Spark* section with the FTLE line (literature check: Haller's LCS, then the sentence).

Phase close: `v3.0.0-alpha.2`; the fold; **the core freezes from step 78 to step 92**.

---

## C. Step 77 [91] — Boehm layout, split surface, preset-next (Windows box) — note only
Content and gates unchanged. It is now the **last core-open step**; the ABI history's 1.6.0 entry closes here with `SUMI_LAYOUT_BOEHM` and the step-76 fields.

**DO NOT FORGET:** 71b and (if taken) 76b before 78; the stencil is one function with two readers after 76; the Anod default per medium is an A/B outcome, not a preset.

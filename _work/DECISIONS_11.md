# Part XI — v3.0, Phase 12: the carry-overs (steps 76–77: Anod finished — the deterministic seed, the strain buffer, the Chirikov grid, the spark reading; the Boehm layout, the split surface, preset-next — moved directly after Kessho on 2026-10-14, `DECISIONS_10 #13`)

Opened 2026-10-14 at step 76; merged into `docs/DECISIONS.md` as Part XI at the phase's fold. The spec is `docs/PROJECT_SPEC.md` (v5) with `_work/TO_PROJECT_SPEC.md` §10.10, §14.7 and §15.4 as the phase's draft, corrected per step; the roadmap `_work/ROADMAP_6.md` (Phase 12); the author's delta `_work/specs/ROADMAP_6_delta_phase_order_and_spark.md` (the spark reading, §B). `DECISIONS_11 #n` means an entry here; `DECISIONS_10 #n` Part X (Phase 11). The core is OPEN for feature work through step 77 and frozen from step 78 to step 92 (the phase order of `DECISIONS_10 #13`); the phase invariant stands — `tests/fixtures/field_512_metal.bin` bitwise on Metal, the §10.1 switching gate across the media.

1. **Step 76 — Anod finished: the deterministic seed, the strain buffer, the
   Chirikov grid and the spark reading (2026-10-14; libsumi 1.11.0, additive;
   evidence `docs/evidence/step76/`).** **The seed** (DECISIONS_7 #25
   answered): `sumi_set_seed` / `sumi_get_seed` — the mapper's strike-phase
   generator is one 32-bit word, the core's only draw. **Measured first:** the
   word alone left Anod divergent run to run; the rest was the mapper's
   gesture CLOCKS — the press-fed torsion sweep's phase and its sub-floor
   remainder, the stir's pending rotation and sense, the running spark and
   burst episodes, the smoothed controls that never quite reach their
   targets — so the set rebases them too (the state a recording cannot
   carry, at rest). The replay's header gains `seed <n>` (schema 1, additive:
   older readers skip it, older files have none); the player sets it after
   the session, before its first frame; the desktop's recorder and the
   bench's write it. Gate 1 on the bench (`--seed-test`): one gesture script
   — eight strikes, a press at two spots for a second — twice from seed 7
   reads bitwise in Sumi, Anod AND Kessho (2 097 152 bytes each, 0 differ);
   the red: twice without the reseed, Anod differs in 1.07 million bytes
   (the step-59 finding reproduced on purpose; Sumi never cared); the word
   read back after eight strikes and set back reruns bitwise. Gate 2: the
   replay gate on the bench's recording (`--record-demo`, 480 frames, now
   carrying `seed`) GREEN on the desktop tier, the 60 Hz re-bucketing
   diverging as required (the mobile tier is the tablets' sub-steps').
   **The strain buffer** (SPEC §10.4's carried question, answered): a side
   RGBA16F target at the field's size, ping-pong, stepped once a frame
   after the deformations by a third program of `composite.glsl` (`strain`)
   through the stencil `kessho_strain` — now a `@block` with three readers
   (Kessho's Layer A, the spark, the strain pass; the plasma keeps its own
   inline copy of the same arithmetic, bitwise 1.10.0 by the Anod golden):
   x the texel's σ, y the FAST activity (the strain added per frame, summed
   over 50 ms), z the SLOW activity (the same over `spark_afterglow`
   seconds), w the EXPOSURE (σ's EMA over the same). Reset by the dip and at
   creation; an export of a foreign field reads instantaneous σ
   (`SUMI_COMPOSITE_NO_STRAIN`). The field's payload and both fixtures are
   untouched; the plasma never reads it. **What "the strain buffer" had to
   mean:** σ of the accumulated map never decays — a channel that "cools"
   can only be the strain's RATE integrated, so the buffer holds activity,
   not σ; the afterglow is its slow channel (gate 8: a press feed's spot
   reads 11.30 at the press end and 4.11 one time constant later, the ratio
   0.3634 against 1/e = 0.3679), and Kessho's Layer A reads the exposure
   (a time constant on the fringes; the Kessho bench gates were not rerun
   against it in this step — the roadmap's gate 4 is open until the Kessho
   test is run with a settle of three time constants, `[ITERATE]`).
   **The spark reading** (the delta's B.2): `params.anod_glow_mode` 0 plasma
   / 1 spark, `spark_afterglow` (0.05..5 s, default 0.6), `spark_branch`
   (κ, 0..1), `SUMI_GLOW_PLASMA` / `_SPARK`. Ridges, not sheets: a texel
   survives if σ is within a tenth of its neighbours' maximum across ê_ψ⊥
   at the stencil's half-width (the tolerance keeps a ridge continuous
   across the half-float's dips; without it the ridges were one-texel
   dust); the transfer 1 − exp(−(σ/1.5·anod_glow)⁴); the LEADER = ridge ×
   the fast activity saturating at 0.01 (above the strain rate's half-float
   noise — at 0.03 the leader flickered per texel and the skeleton
   fragmented), toward white-blue, the bloom's only source; the AFTERGLOW =
   ridge × the slow activity (reference 0.3, gain 0.55) in the charge's own
   colour; the flicker 15 % at 7 Hz with the speckle's value as the phase;
   charge only, as the plasma. **Branches, as built:** the delta's
   "threshold against σ + κ·grain" cannot touch a hot ridge — on a swirled
   filament σ is several, far above the reference, and the transfer
   saturates whatever the dither (measured twice: 11.67 endpoints per 100
   texels of skeleton at κ 0 and 11.67 at κ 0.5 with an additive dither;
   11.62 and 11.95 with a multiplicative one of ±1.5σ) — and a texel-scale
   grain only punches holes a two-texel ridge absorbs. So κ GATES the ridge
   against a grain COARSER than a ridge (nine texels a cycle at 1080p,
   screen-locked): cut wherever the grain exceeds 1 − 1.2κ — never at κ 0,
   three texels in ten at κ 0.5 — a chain of segments with dead ends, the
   dendrite's spurs; lighting, not geometry. **What the gates measured**
   (`--spark-reading-test`, the swirl scene at 1080p: twelve large strikes
   torn by their sparks, 180 frames of a strong Lamb–Oseen swirl with a
   tine across; the press scene for the afterglow): (5) the plasma prints
   bitwise with the spark's knobs moved (0 bytes), the field under the
   spark bitwise the plasma run's (0 bytes) while the print differs in
   6 167 — a reader; the Anod golden `tests/fixtures/composite_512_metal_anod.rgba`,
   captured from the tree BEFORE the step and gated by
   `composite_gate.py --medium 1`, bitwise; (6) thinness: the lit ridges'
   mean width 2.49 texels (area 6 354 over a skeleton of 2 548; Zhang–Suen);
   the red, the suppression off: 6.3 texels — the roadmap's 12 was a soak
   scene's guess, this scene's strained charge is thinner, the bar is 6 and
   twice the ridges; (7) sparsity: the spark lights 0.31 % of the canvas
   above the bright threshold, the plasma 0.59 % — the roadmap's "≈ 30 %"
   was the same guess: this scene's charge covers 3 % of the canvas; the
   bar is 5 % and 0.6× the plasma; (8) the afterglow above; (9) the
   branches: 11.62 endpoints per 100 texels at κ 0 (a one-texel ridge has
   ends of its own — "no spurs" is relative), 16.23 at κ 0.5 (1.40×), the
   red (the branch grain off at κ 0.5) 11.82; (10) the glass under a 120 BPM
   roll scroll with the flicker's clock running and the grid off prints
   bitwise (0 bytes); (11) the A/B renders at the author's defaults —
   `spark_ab/plasma.png`, `spark.png`, `spark_branches.png` — the author's
   viewing, the Anod default per medium stays plasma until it. **The
   Chirikov grid** (DECISIONS_5 #89): `--chirikov-grid <dir>` — one pass of
   the operator at K 0.3 / 0.6 / 1.0 × periods 2 / 4 / 8 × ε 0.15 / 0.3 /
   0.6 on a sheet with two drops, 27 renders and an index
   (`docs/evidence/step76/chirikov_grid/`); the author's choice becomes the
   defaults by their word (today K 1.0, periods 2, ε 0.5). **The frozen
   paths:** the field and the Sumi composite fixtures bitwise, the Anod
   golden bitwise, ctest 14/14, the web gate green. **Not in this step:**
   the Lichtenberg operator (open decision 21: 76b or 4.0, the author's);
   the tablets' recorders' `seed` and their Glow-reading rows (their
   sub-steps); the Kessho gates against the exposure (above). Next: step
   77, the Boehm layout, the split surface and preset-next — the last
   core-open step. **Amended by #2 (2026-10-15):** the branch trick is
   retired and `spark_branch` dropped before 1.11.0 ships; the strike's
   shape follows `glow_mode` from 76b.

2. **The first spark build viewed, and the branch trick retired (2026-10-15;
   the author's `ROADMAP_6_delta_76b_leader_stroke.md`, folded with
   corrections).** The author was not satisfied, and the delta says why: on
   the strikes the three readings — plasma, spark, spark with branches —
   have the same silhouette. The reason is in the operator, not the reader:
   the Anod note-on's shear episode is a stack of triangle waves, whose
   slope is piecewise constant, so σ across the blob is a set of PLATEAUS;
   non-maximum suppression thins only a peaked profile (and #1's tolerance
   of a tenth keeps a plateau whole on purpose), so every texel of the blob
   survives. #1's ridge numbers were true but came from the swirl scene,
   where differential rotation grades σ; a strike alone has no line in it,
   and a reader cannot invent one. **The line has to be written: step 76b,
   the leader stroke** (the Lichtenberg operator of open decision 21, now
   inverted from "recorded, not built" to "the fix"). **Retired:** the branch
   trick — measured in #1 as unable to make endpoints on a hot ridge, it
   cannot make them on a plateau either; `params.spark_branch` is dropped
   from the header, the ABI history (1.11.0 lists two fields), the preset
   table, the INI, the settings window and the bench, and its dev mode with
   it. **Kept, unchanged:** the seed, the strain buffer, the Chirikov grid,
   the reading's NMS, transfer, leader, afterglow and flicker — they were
   right, with nothing thin to act on. **The fold's corrections to the
   delta:** 76b on the Mac (the Linux box is out); the roadmap's decisions
   are 21 (inverted) and 22–24 (the trick retired, walk vs DBM, the drift
   direction), plus 25 the §10.2 principle — the delta's 20–23 were off by
   one; "1.6.0" in its ABI note is 1.11.0; the walk's lengths in canvas
   heights, not texels (ℓ ≈ 0.012, the window's half-width 0.003 — thirteen
   and three texels at 1080p); the plateau red in an operational form (the
   suppression keeps ≥ 80 % of a blob's charge — a stack of three has
   several flat levels, so the 5 % cross-section is the figure, not the
   gate); "exactly 1 per stroke at p_b = 0" read as the single leader's two
   ends; and the stroke as an EPISODE like the spark's — the tips advancing
   over frames, one batched pass a frame per stroke — so the pass budget
   holds by construction and the buffer's leader rides the tip. The first
   build's captures are archived under `docs/evidence/step76/spark_first_build/`.
   The amended step-76 gates: (6) and (9) move to 76b; (12) the plateau and
   (13) the torsion-scene thinning are added, measured at 76b's start.

3. **Step 76b — the leader stroke, as shipped (2026-10-15; the delta of #2,
   `ROADMAP_6_delta_76b_leader_stroke.md`; libsumi stays 1.11.0, additive:
   `sumi_add_leader`, `SUMI_DEFORM_LEADER` = 17; evidence
   `docs/evidence/step76b/`).** The operator is the delta's: under
   `glow_mode` 1 the Anod note-on is a stroke along a jagged tree from the
   key instead of the blob episode (which mode 0 keeps bitwise — the Anod
   golden holds, the field under plasma holds); the walk ℓ = 0.012 canvas
   heights, ±30° about the strike's pitch axis, p_b = 0.15 at ±45° with
   half the parent's count, depth 3, N = 8 + 32·v (v the strike's value:
   r = r_min + span·√v, so the radius the mapper hands the strike gives v
   back as ((r − r_min)/span)² — no second velocity path); the generator a
   32-bit LCG on the strike's draw from the seed, the same draw the blob's
   phase takes, so the replay gate of step 76 holds with strokes (gate 2:
   the press scene twice from seed 7 bitwise, 16.6 MB, 0 bytes differ). Each
   segment a slip along its own direction in a C² quintic window across
   (h = 0.003) and along; one pass per stroke per frame composes the
   frame's sub-steps last-first (the pre-image of a composition), up to
   eight entries. **Resolved in the building, none of it in the delta:**
   * **The channel is charged.** The first build's stroke walked into dark
     water: the reading lights charge only and a strike's charge is a small
     disc, so the whole tree was a crack through the far-field grid, lit
     nowhere (`spark_first_build`-style, archived in the step's
     `leader_ab/` as the plateau renders' sibling). The leader IS the
     ionized path, so the pass writes the strike's own band (phase base +
     the radial fraction across, as a drop's; the strike's aux with it)
     within h of the segment, round caps at the joints; the slip stays.
     The mapper notes the strike's band at the strike
     (`sumi_voice_mapper_note_charge`; the engine's tap notes its drop's);
     the gesture route's `sumi_add_leader` takes a drop count and a band
     of its own. Under the soak the charge is OFF
     (`sumi_debug_set_leader_charge`): the class gate reads the bare shear,
     the charge a deposit excluded by nature as every strike's drop is (#13
     of Part V's precedent).
   * **The along-ramp is the class.** Across the band a slip is a pure shear
     (det J = 1 for any profile — the tine's argument); along it the window
     is on the coordinate being slipped, so the ramps compress by
     a·w′_max/ramp, w′_max = 1.875 for the quintic. The first build's ramp
     of h made det J ≈ 0.06 at the ramps' steepest; the ramp is 2ℓ = 0.024
     and the slip a = h·(0.6 + 1.4 v) ≤ 2h = ramp/4, so det J ≥ 0.53 and
     ONE sub-step at every velocity (K = ⌈4a/ramp − 10⁻⁴⌉ = 1; the formula
     stays general). The soak: det min 0.763, mean 1.00000 everywhere after
     one sub-step; 500 pairs 0.15 texel of pre-image drift, the mass within
     0.9 %; the stream (a stroke every 12 frames at half energy, 6000
     frames) grows the mass +20.6 %, 3.4·10⁻⁵ a pass — the far ramps'
     bilinear magnification, within the class's rate bar (5·10⁻⁵; the
     burst's and the wake's bar) — and the erosion reads below the glide
     tine's control. `[ITERATE]` a gentler ramp (3ℓ) if the growth matters
     in play; the strokes are brief events there.
   * **The advance is time-based.** 120 sub-steps a second as a fractional
     credit on dt (one a frame at the scripted 1/120 s; capped at eight so a
     stall never bursts), every alive branch together, the frame's entries in
     time order in the one pass; the budget refused → nothing committed,
     the stroke waits a frame (the first build advanced the counter and lost
     the segment). The storm of fifteen strokes at 127: 15 alive at once, at
     most 30 passes a frame (the fifteen strokes and their fifteen charges on
     the strike's frame; the budget 64), the last walked out at frame 59.
   * **The channel's gain 0.55 → 0.4** (`SPARK_AFTER_GAIN`, a step-76
     `[ITERATE]`): the delta's "the leader ≥ 3× the channel at the tip" read
     2.6 at 0.55 and reads 3.03 at 0.4 — measured on the charged texels with
     the bloom off (the composite's own terms: the tip the brightest, the
     channel the crest's 90th percentile of the texels whose fast activity
     has gone), because the lit mask's threshold clips the channel's
     distribution from below and the first measure could not move. Plasma
     is untouched by the constant.
   * **The seed test's red control, found and fixed in the bench:** a run
     cut short mid-press carried its held press into the next run — the
     press's own state is not the seed's to rebase — and the two "identical"
     runs differed by 3.6 MB. Every leader-test run now ends its press
     (`sumi_gesture_press_end`); the determinism itself was never in doubt.
   **The measures, corrected against the delta's figures (the roadmap's
   gate text is the author's to amend):** (3)'s red — "the blob keeps ≥ 80 %
   of its charge lit" — is the suppression's share alone, NMS on against
   NMS off on the same reading: 32 % on one blob, 48 % on the press scene's
   eight (a thinned line would keep a few percent: two widths over a
   diameter), the silhouette 100 % within two texels of the plasma's; the
   plasma-relative figure (45 %) conflates the spark's steeper transfer with
   the suppression; raw σ over the blobs' interior reads (max − mean)/max =
   0.53 — a stack of three is terraces, not one plateau, which the fold's
   note anticipated. (4) — "≥ 4 endpoints per 100 texels of ridge" — is read
   on the CHARGE's skeleton (the channel the stroke writes, thinned, spurs
   pruned four texels): the lit mask's skeleton counts the flicker's dashes
   (41.8 ends a stroke at p_b 0), and the strain's band overshoots every
   kink by the along-ramp (6.8); on the charge, six strokes at p_b 0.15 end
   in 3.83 tips each (0.58 per 100 texels — the "4 per 100" assumed the
   dashes), at p_b 0 in 1.50 — the tip, and the root where the charge disc's
   skeleton leaves one — and the blob episodes' discs give 0.09 endpoints
   per 100 texels of charge; the bar is ≥ 3.5 a stroke and ≥ 1.5× the single
   leaders'. The amended step-76 gates: (12) the plateau — 47.9 % kept with
   the suppression on, 100 % within two texels — and (13) the torsion scene
   — the rings 1.91 texels wide under the reading, 3.9 with the suppression
   off — are green (`--spark-reading-test` 7/7, the sparsity gate now the
   plasma reading of the SAME field: 1.06 % against 1.95 %). Gate 6, the
   reach chart: 22 / 31 / 47 / 51 frames and 2.4 / 5.6 / 10.4 / 13.4 thousand
   strained texels at 32 / 64 / 96 / 127. Thinness: 1.70 texels on one
   stroke (3.6 k lit texels over a 2.1 k skeleton). `--leader-test` 6/6,
   `--soak leader` green, ctest 14/14, the field fixture within its tier, the
   Sumi composite and the Anod golden bitwise, the web gate green.
   **Kept open for the author:** the viewing (`leader_ab/`: plasma strike /
   leader / leader at p_b 0 on the press scene at frames 40 and 100,
   `stroke_one.png` at the defaults, `blob_under_spark.png` the plateau
   figure, `storm_f30.png`) and the Anod default per `glow_mode`; DBM
   against the walk (decision 23 of the roadmap: the walk is kept unless the
   viewing says too regular); the drift direction (24: the pitch axis
   shipped); the constants ℓ, h, p_b, the rate, the gain — `[ITERATE]`; the
   tablets' Glow-reading rows (their own sub-steps) carry the stroke with
   them, nothing to add there. The `--leader-pb <p>` flag sets this run's
   branch probability for the live window and replays (the test sets its
   own).
   **Amended at the author's viewing (2026-10-18):**
   * **The stroke is struck BESIDE the shear episode, not instead of it.**
     The first ship replaced the Anod note-on's blob episode by the stroke
     under `glow_mode` 1, and the bolt lost its impact — the blast and the
     streamers are the strike's weight, the leader grows out of them. The
     mapper's switch now adds the stroke and falls through to the episode;
     the strike's draw order is the stroke's first, the blob's phase after
     (the determinism gate holds: bitwise twice from one seed). Every
     render under `leader_ab/` is re-cut this way.
   * **The habit and the tine profile are every medium's.** The drop's
     habit (`drop_habit`, `habit_roundness`, `habit_blades`) and the tine's
     profile (`tine_profile`) shape the drop and the tine under Sumi and
     Anod as under Kessho, and sat under Kessho's Mineral panel; the
     desktop's settings window gives them a *Drop & tine shape* section of
     their own, always present (the author: "the habit affects all the
     mediums"); a mineral preset still pairs a habit and a profile with its
     optics, and editing either still marks the mineral *Edited*. The
     settings reference moves the two rows with it.
   * **A step-76 bug, found by gate 5 and fixed:** the strain pass compared
     a float σ against its own half-rounded previous value (the buffer is
     RGBA16F), a residual of up to half an ulp every frame on a static field
     — positive where the rounding went down — so a strained texel kept a
     permanent activity floor: a stroke's channel decayed to 0.44 instead
     of 1/e, and a texel of σ ≳ 16 would have read as a leader forever. The
     pass now rounds σ to half (`packHalf2x16`, the value stored is the
     value compared); the press scene's afterglow gate of step 76 reads
     0.358 after the fix (0.364 before — its feed spot sat at a lower σ),
     and the fixtures are untouched (the plasma and Sumi composites never
     read the buffer; Kessho's exposure channel is the same EMA of the same
     σ).
   * **The measures, once more:** with the strike's blast under every
     stroke, the branch count and the afterglow are read on STROKES ALONE
     through the gesture route (`sumi_add_leader`: six on the tap grid's
     points at the tap's energy; one at full energy) — the torn charge
     would sit in every count; thinness is read beyond the strike's blob
     and streamers (2.5 r about the key); the reach is the channel's
     charge beyond the same disc; the leader's contrast is read at frame
     30 with the tip walking, the channel the texels whose leader is under
     a tenth; the storm's cost is the pass TOTAL over 320 frames against
     the same storm under plasma — the shear episodes' steps land where the
     budget lets them, so a per-frame peak compares two different frames.
     The figures are the evidence's table.
   * **The viewing's verdict and the default (the author, 2026-10-18):**
     "this looks good" — the walk is kept (DBM stays a 4.0 item unless
     asked for; roadmap decision 23), the pitch axis stays the drift (24),
     and **plasma stays Anod's default `glow_mode`**: the spark reading
     with its stroke is the player's choice in the settings, the presets
     and the INI. The roadmap's gate 8 is folded.

# Part X — v3.0, Phase 11: Kessho and symplectic harmony (steps 71–75: the third medium, the Arnol'd lock, the braid, the chord wave, the Poincaré strobe — opened at the fold of the proposals, 2026-10-14)

Opened 2026-10-14, the day `v2.0.0` shipped (#1 the fold of the proposals, #2 the order); merged into `docs/DECISIONS.md` as Part X at the phase's fold. The spec is `docs/PROJECT_SPEC.md` (v5) with `_work/TO_PROJECT_SPEC.md` as the arc's draft, corrected per step; the roadmap `_work/ROADMAP_6.md`; the author's proposals `_work/specs/` (`KESSHO §n` means `medium_specs_v3.md`). `DECISIONS_10 #n` means this file; the next phase opens `_work/DECISIONS_11.md`, and so on, one file per phase as the v2 arc did.

1. **The v3.0 arc opens: the three proposals folded into `_work/ROADMAP_6.md`,
   the spec draft opened at the START of the arc, and what the fold corrected
   against the tree.** The author delivered `_work/specs/` on 2026-10-14 —
   `roadmap_voxo.md` (Phases 11–13: Suzu's deferred physics, four new
   voices, Voxo's Decent Sampler coverage; and a first draft of the
   carry-over and release phases), `roadmap_voxo_gate_delta.md` (its review
   fold), `road_map_medium_v3.md` (Phases 14–16: Kessho and symplectic
   harmony, the carry-overs, the release) and `medium_specs_v3.md` — with
   the word that version 3 has both features. The fold: one roadmap in the
   house form (working rules, the ABI block, the binding table, a step
   per section with its gates and its DONE clause, the deferred list, the
   open decisions), the gate delta's amendments applied in place and
   marked, the first draft's carry-over and release rows (its 85–87)
   superseded by the medium roadmap's Phases 14–16 (90–92), which carry
   the same items plus Kessho's; and `_work/TO_PROJECT_SPEC.md` opened
   now rather than at a phase's close — the proposals ARE the spec until
   a step corrects its subsection to what shipped, so the author
   transcribes a true text at the fold (the v2 arc wrote this file at each
   close and the specs left the tree; the proposals stay in `_work/specs/`
   until the author removes them). THE ORDER OF THE PHASES IS NOT DECIDED
   HERE: the proposals number Voxo-first and say they work in either
   order; the author asked for the fold's opinion (harmony-first; the
   roadmap's open decision 1 records it) and decides before step 71; the
   step numbers keep the proposals' numbering until then and renumber on
   the author's word. What the fold corrected against the tree, each a
   fact the proposals got wrong or left to the header: (a) the control
   dimensions — the proposal's `SUMI_CTL_POLARIZER_ANGLE` = 14 … `_ARNOLD_K`
   = 17 collide with the torsion's, the Chladni's, the spark's and the
   Chirikov's (14–19; `SUMI_CTL_COUNT` 20), so they are 20–23 and the count
   24, the chart's rows likewise; (b) the trace header is
   `trace/include/sumi_trace.h`, not `trace.h`, and the mode is a field of
   `sumi_trace_config_t` — the proposal's text says "mode 2" and its enum
   says 1: it is the SECOND mode, value `SUMI_TRACE_POINCARE` = 1; (c) the
   export API is a flags word (`SUMI_EXPORT_ANOD_ALPHA` = 1u is a bit), so
   one new flag, `SUMI_EXPORT_KESSHO_CAUSTIC` = 2u (Layer B alone over
   alpha); the proposal's `SUMI_EXPORT_KESSHO` is dropped — a Kessho
   export without the flag is the full composite, as an Anod export is;
   (d) the consonance curve's home is `core/src/midi_normalizer.cpp`, beside
   the voice table it needs — the proposal's `midi_processor.cpp` does not
   exist; (e) `press_mode` 3, `bend_mode` 5 and `slide_mode` 3 are the next
   free value of each (the proposals had them right); the two A/B modes
   are fields appended to `sumi_params_t` (`polarizer_mode`, `braid_mode`
   — the Step-33 pattern), not new enums alone; (f) `voice_kind` is a
   number with a comment in `voxo.h` (0–8 today), not the proposal's
   `VOXO_SUZU_TODA`-style constants: 9 the membrane, 10 the Toda ring, 11
   the Boris gyroscope, 12 the Dirac network, 13 the billiard, in the order
   the steps land, named constants if a step adds them; the Suzu params
   struct is `voxo_suzu_params_t` as the proposal says; `bow_mode` is new;
   (g) step 90's "(Linux box)" and step 91's "(Windows box)" are the Mac's:
   both are core and shared-UI work, authored on the desktop harness by
   the one-platform rule, the Linux box is out of commission and the
   Windows box runs the lane only; (h) Kessho's Layer A is served by the
   strain buffer of step 90, which lands AFTER step 85: the layer reads
   instantaneous σ first and the buffer feeds it unchanged; (i) the web's
   medium parameter is numeric (`?medium=1`, DECISIONS_9 #6), so step 92's
   `?medium=kessho` is `?medium=2`; (j) `voxo_fuzz`'s unit is seconds
   (`--seconds`), not the proposals' "25 M iterations"; (k) step 75's Scala
   loader is Voxo's (the sound follows the table) — the layouts'
   microtonal mapping (§14.6) is a core item and stays carried, or rides
   with step 91; (l) the SoA SIMD refactor of step 71 meets the web gate's
   bit-for-bit contract (`suzu_web_gate.mjs` against the native
   reference): a working rule now says the contract is restated before the
   refactor lands — the scalar path as the reference build, or a tolerance
   gate — recorded, not discovered; (m) "Voxo 2.0" is the sampler's
   format coverage, not the library's version, which follows its ABI
   (0.16.1 today) — the phase is named "Voxo DS 2.0" and the author names
   the library; (n) the braid's pass count is gated against the pass
   budget's table (`sumi_core.h`, DECISIONS_5), the proposal's "≤ 64" being
   its reading of it; (o) the release split of `road_map_medium_v3.md`
   §5.1 (3.0 = harmony alone, the Voxo phases as 3.x) is superseded by the
   author's framing — both features in 3.0, one release — and survives
   only as the order question; (p) the tag `3.0.0-alpha.kessho` passes the
   spine (`X.Y.Z[-pre]`, any suffix); the v2 pattern was `-alpha.N`; the
   author's naming; (q) the decisions files are one per phase as in v2
   (`DECISIONS_10` … `_15`, Parts X–XV), the proposal's "`DECISIONS_10` for
   Phases 11–15" read as the first of them; (r) `site/scripts/build-notes.mjs`
   is extended to render this file in flight — its loop to 10 and its
   path-trim regex to two-digit numbers (`_work/DECISIONS_10.md` was not
   matched by `DECISIONS_\d`). The gate delta's own corrections (the
   billiard's det J = +1 in 4D, the Dirac step Q-orthogonal, the Toda ring
   near-integrable, the Boris voice volume-preserving with both prewarps,
   the membrane's CFL at E_max and its h² scaling, the bore shock
   dissipative, the oversampler a prerequisite) are applied as the
   author's review, not re-decided. Nothing in the tree changed for this
   entry beyond the three `_work/` files, the notes renderer and
   CLAUDE.md; no step has started.

2. **The order of the phases: harmony first, on the author's word.** Asked
   whether to start the Kessho phase before the sampler's (the proposals'
   14 before 13), the fold recommended further — Kessho and harmony before
   everything (the reply of 2026-10-14): the riskiest, taste-decided work
   (two A/B viewings, a Newton kick in the shader, the caustic) gets the
   arc's slack; the core reopens once, early, and freezes for the eleven
   Voxo steps (step 91's Boehm layout the one later touch); the Poincaré
   strobe becomes the instrument every new voice is seen through at its
   own step rather than retrofitted; the one cross-tie — the bus plate
   struck at the canvas — needs nothing from Kessho. The author: "no
   particular order is preferred for me, so I will listen to you — we can
   start with 14, then the others". RESOLVED: Phase 11 = Kessho and
   symplectic harmony (steps 71–75: the composite, the Arnol'd lock, the
   braid, the chord wave, the strobe), Phase 12 = Suzu foundation
   (76–80), Phase 13 = the new voices (81–84), Phase 14 = Voxo DS 2.0
   (85–89), Phase 15 = the carry-overs (90–91), Phase 16 = the release
   (92) — the proposals' other phases in their own order. The
   renumbering is mechanical and final (`_work/ROADMAP_6.md`,
   `_work/TO_PROJECT_SPEC.md`; no evidence existed); its consequences in
   the roadmap: the Boris gyroscope's spirograph is verified in Kessho
   (the gate delta's 77c, now step 82's), each Phase-13 voice carries a
   strobe-portrait gate, the membrane's bus plate (77) is seen as well as
   heard from its first session, this file is Phase 11's and the next
   phase opens `_work/DECISIONS_11.md`. The release split of
   `road_map_medium_v3.md` §5.1 is not taken: both features ship in 3.0.
   Next: step 71, the Kessho composite, on the Mac.

3. **The Kessho composite as built (step 71, 2026-10-14): a reader of the same
   field — the chart, the brush, the trace, the caustic; libsumi 1.6.0.**
   What shipped against KESSHO §1 and ROADMAP_6's ABI block, and what the
   step resolved. (a) THE ABI, 1.6.0, additive: `SUMI_MEDIUM_KESSHO` = 2;
   `sumi_ctl_t` 20–23 (`SUMI_CTL_POLARIZER_ANGLE`, `_CAUSTIC_DEPTH`,
   `_BRAID_COUPLE`, `_ARNOLD_K`; `SUMI_CTL_COUNT` 24 — 22 and 23 declared
   now, inert until steps 73 and 72); `sumi_params_t` appended with
   `polarizer_mode`, `braid_mode`, `kessho_retard` (nm per unit strain,
   200..8000, default 2000), `kessho_backlight` (0..2, 0.85), `kessho_grain`
   (0.5), `kessho_tint` (0..1, 0.25), `kessho_caustic` (0..2, 0.5);
   `slide_mode` 3 (the polarizer), `press_mode` 3 and `bend_mode` 5 reserved
   (accepted, resolving to the medium's interim defaults);
   `SUMI_EXPORT_KESSHO_CAUSTIC` = 2u; the twelve Kessho palettes
   (`sumi_palette_preset_count(2)` = 12, index 0 "Michel-Lévy"). Nothing
   moved; media 0 and 1 render bitwise (the field and composite fixtures on
   Metal, the thirty bench checks of the Anod, print, palette and gesture
   tests, `docs/evidence/step71/gates/`). (b) THE LUT AS THE PALETTE — the
   `[ITERATE]` of KESSHO §1.2 resolved: Layer A's colour is one 1024-texel
   RGBA16F table over the retardation 0..2200 nm (the fourth order's end),
   built by the engine on the CPU when its inputs change and uploaded by
   the renderer when its version moves, outside any pass — the same bytes
   on every backend. Palette 0 is the physical chart: `tools/kessho_lut.py`
   integrates sin²(πΓ/λ) against the CIE 1931 2° matching functions (the
   Wyman–Sloan–Shirley multi-lobe fit, JCGT 2013) under a 6504 K Planckian
   backlight, white-balanced per channel so a full transmission is (1, 1,
   1), and writes 8-bit sRGB into `core/src/kessho_lut.h` (regenerated
   bit-identically; the engine decodes it to linear). The other eleven —
   smoked quartz, calcite, nematic, amber, tourmaline, olivine,
   labradorite, fluorite, cobalt & amber, viridis, cividis — and the custom
   slot are `sumi_palette_t` gradients read PERIODICALLY in the order
   Γσ/550 nm: the first order runs the gradient once, the next orders repeat
   it fading toward their own grey (three quarters by the fourth) as the
   chart's colours do, the first 200 nm darkened so a resting crystal is
   dark under every palette; the morph ring mixes the two slots texel-wise
   in the one table. The editor edits stops as before; the hue drift is
   unused (Kessho reads no aux); the accent is the caustic's light; the
   clear tone is carried but the substrate is the composite's own fixed
   calcite (0.012 / 0.013 / 0.018, as Anod's glass is fixed), so a custom
   palette's clear tone never lights the sheet. (c) THE SHADER: Layer A from
   Anod's short stencil (one-sided min, the rounding bias subtracted) — σ
   as Anod's, ψ = ½·atan2(2C₁₂, C₁₁ − C₂₂) of C = JᵀJ, the brush ½(1 −
   cos 4ψ·c − sin 4ψ·s) with (c, s) = (cos 4α, sin 4α) from the CPU, I_A =
   backlight · brush · LUT(kessho_retard · σ). Layer B from the WIDE window
   the Anod grid reads (±3 texels at 512, the staircase averaged along each
   axis): tr(∇d) = tr J − 2, Δ = (tr J)² − 4, the tint cool (0.10, 0.22,
   0.42) or warm (0.52, 0.22, 0.08) by smoothstep(0.05, 0.8, |Δ|) with the
   noise floor under 0.05; the caustic from the COLLAPSED determinant 1 +
   z(1 − z)·tr(∇d) — a LINE at the contour, drawn screen-space a fragment
   and a half wide (fwidth: it neither aliases where the strain changes
   fast nor smears where it changes slowly) over the soft 1/(|den| + ε)
   form with the rest value subtracted, so resting water is dark. The
   composite order substrate → B tint → B caustic → A → the shared bloom
   and shoulder; the exports: `SUMI_EXPORT_ANOD_ALPHA` writes the crystal's
   light over alpha, `SUMI_EXPORT_KESSHO_CAUSTIC` Layer B alone. The masks:
   fresh water and the seam (the short stencil's test), a window off the
   sheet or across a seam (the wide one's). (d) THE ANALYZER: α = ctl·π/2 —
   the control's range is one period and ITS END IS ITS START (ctl ≥ 1 −
   1e-6 wraps to 0: the smoother settles an ULP short of 1), so α = 0 and
   α = π/2 render bitwise the same by construction, not by float luck (gate
   3; the first run read 25 bytes apart before the wrap). Mode A, global:
   CTL 20, or the latest voice's slide under `slide_mode` 3. Mode B, per
   voice: `sumi_voice_mapper_polarizer_voices` hands the held voices (x, y,
   cos 4α_k, sin 4α_k) to the composite, which takes the nearest four by
   inverse-square distance (a voice's reach 0.03 canvas heights) over the
   global as a floor (0.35) and normalizes the summed vector — the π/2
   period wraps in the vector, as KESSHO §1.5 asks. Both built; the A/B K-1
   is the author's viewing (#5). (e) THE BINDING TABLE, interim: the strike
   is the drop — a lens whose rim Layer B draws as a ring, nothing new
   needed; poly pressure the Lamb–Oseen swirl; the slide the polarizer; the
   mod wheel the caustic depth (the `CAUSTIC_DEPTH` control or the
   mod-wheel dimension, whichever is higher — the vortex quiet in Kessho as
   in Anod; z = 1.05·(8/1.05)^ctl, log-spaced); the bend the glide until
   step 73's braid; and THE PRESS THE SWIRL until step 72's lock — chosen
   over the ink feed after the first A/B renders: the feed's nested rings
   read as Sumi's, and the lock is a twist built on the swirl, so
   `press_mode` 1 is Kessho's interim default and the reserved 3 resolves
   to it. (f) The gestures are Sumi's under Kessho (the tap a drop, the
   pinch the saddle Layer B shows warm, the twist the given profile, the
   press Sumi's): the gesture table has no Kessho column in the spec. (g)
   THE DESKTOP: the Medium radio gains Kessho and its knobs (retardation,
   backlight, regime tint, caustic, the polarizer's mode), the Substrate
   section the facet grain and the shared bloom, the slide combo "Polarizer
   angle", the ledger's "Light over alpha" (Anod's discharge or Kessho's
   crystal), the INI and the preset schema the new keys (schema 1 stays —
   additive keys with defaults); the bench's A key cycles three media;
   `--kessho-test`, `--kessho-flat <op|all>`, `--ab polarizer --ab-out
   <dir>`. (h) The web: the scenes' medium toggle to 2 and the shell's
   clamps; the wasm builds (the shader cross-compiles to WGSL, fwidth
   included) and the web gate holds; the tablets consume in their own
   steps — their medium pickers offer two media until then. THE GATES
   (`kessho_test.txt`, 12/12): the identity sheet dark (mean 31.9, max
   35.1); the chart within ΔE 0.67 of the integration at 550 / 1100 / 1650
   nm, black at 0, white at 275 nm; the switch Sumi → Anod → Kessho → Sumi →
   Kessho → Anod → Kessho bitwise on the field (gate 1); the moved water
   lit (105 against the substrate's 32); the Maltese cross (146 on the
   axes, 48 on the diagonals — the dark arms on the diagonals, where a
   simple shear's principal axis lies parallel to the analyzer; at α = π/4
   they turn to the axes, 588,845 bytes apart); the period bitwise (gate
   3); the regimes (the vortex core cool r 110 / b 121, the saddle warm r
   192 / b 90); the caustic (the fold's line at 253 against 65 at rest);
   the substrate screen-locked under a 120 BPM scroll (gate 5, 0 bytes);
   the export the dip's print (gate 6, 0 bytes); the caustic alone over
   alpha within the crystal's light (194,936 lit texels, every one); the
   8K caustic export. The bench's `t42_print` dips the paper and resets
   the field — the first run's four reds were that; the Kessho checks
   shoot through the export instead (`t71_shot`), which gate 6 proves is
   the print.

4. **Gate 2 as measured: the field cannot hold the proposal's 1e-4, the two
   classes are not separable by differencing it, and the gate is re-pinned
   to the floor — the class stays declared.** KESSHO §1.3's "free det-J
   gate" — z = 1 flat for any exact field, a sub-stepped or mis-windowed
   operator showing as texture — holds in the continuum. `--kessho-flat`
   measures det J from the field's pre-image by central differences over
   ±1, ±2 and ±3 texels on every soak operator's scene (one strong pass,
   the drops' rims masked where the band boundary is). What it reads
   (`kessho_flat.txt`): the identity sheet 0 exactly; the estimator's own
   CPU controls — an exact Lamb–Oseen twist 1.4e-3 max (the stencil's
   truncation), the first draft's windowed Arnold kick 50 max and 0.35
   rms, RED as required; the GPU fields — the three-drop SCENE ALONE reads
   a median of 2e-3 (±3) to 5e-3 (±1), a p99 of 0.2–0.8 and a max of 8–30:
   the drops' rims, where the inverse of an exact expansion has an
   unbounded radial derivative (new ink meets pushed water — the
   pre-image's one singularity, exact and unresolvable by any stencil),
   own everything above the median even with the boundary masked; every
   operator then reads the same floor (medians 2e-3 to 6.5e-2, the spark
   shear's kinks the highest), the exact family's worst p99 4.2 (the
   Chirikov map) against the sub-stepped family's best 0.9 (the Chladni
   field) — NOT separable. Resolved: (i) the composite's caustic uses the
   collapsed determinant 1 + z(1 − z)·tr(∇d), which assumes the class —
   what the declaration promises — rather than measuring it; the full form
   1 + z·tr(∇d) + z²·det(∇d) would paint the estimator's floor, 36× at z =
   6, onto exact fields; (ii) the gate holds each operator's TYPICAL texel
   within 0.1 of det 1 (18/18, the gross non-conservation of the windowed
   kick would read here) and keeps the identity and the red control; (iii)
   the class is where it was — declared in the header, the test and the
   page, proven by the four-part conservation gate, never discovered. The
   author's spec is flagged for correction (`TO_PROJECT_SPEC` §10.7.3): the
   flat plane is a diagnostic of gross non-conservation, not a 1e-4
   certificate. A discriminating estimator — a rim-aware window, a
   smooth-map fit — is a research item, not this step's.

5. **The look and the defaults; what the author views: the A/B K-1 renders,
   the shared bloom, the frost, the depth's top.** Defaults by eye on the
   §4.6 script and the A/B phrase: retardation 2000 nm per unit strain (the
   first-order red at σ ≈ 0.27), backlight 0.85, tint 0.25, caustic 0.5,
   grain 0.5 — softened from a first pass at 1.0 / 0.35 / 0.6 after the A/B
   renders under the shared bloom (Anod's 0.75) blew the sheet to white:
   Layer A lights all strained water where Anod's filaments are sparse, so
   the same bloom sums far more. The renders are archived both ways
   (`kessho_ab/kessho_ab_*.png` at the shipped bloom, `*_bloom035.png` at
   0.35); the bloom knob is shared by the spec and is the author's to
   settle — a Kessho-side scale was considered and not hidden in the
   shader. PENDING THE AUTHOR: (1) K-1, A against B — the phrase holds
   three MPE notes with slides at 7°, 45° and 84° over a vortex and a
   saddle: in B each note's cross sits at its own angle, in A the whole
   sheet at the latest slide's; the difference lives in the three notes'
   crosses and is subtle at this phrase; A stays the default until the
   viewing. (2) The depth's upper bound (KESSHO §1.3's `[ITERATE]`): at z ≈
   6 the fold's lines are crisp and the soft term lights the half-float
   staircase of strongly strained water as fine fringes — a texture, not a
   fault; the control's top may want 4 rather than 8. (3) THE FROST: Layer A
   reads the far field's small smooth strain through the half-float
   staircase — the case Anod avoided by never glowing water by strain —
   and it renders as a frosted speckle round every swirl: in character for
   calcite, but a quantization; the wide window the trace already uses
   could serve ψ and σ too if the author wants it smooth (`[ITERATE]`).
   (4) The chart's hues sit 20–30 nm early against the textbook
   Michel-Lévy chart (a 6504 K Planckian rather than D65, the fit's own
   tails): `kessho_retard` absorbs the scale, and a published chart's
   sampled colours replace the integration as the reference when the
   author has one (`kessho_lut_check.txt` lists the table's hues). Next:
   step 72, the Arnol'd tongue lock.

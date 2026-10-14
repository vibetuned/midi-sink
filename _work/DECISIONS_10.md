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

6. **Step 71b — the minerals: the crystal drop, the cleavage profiles, a
   mineral in place of the palette (2026-10-14; libsumi 1.7.0, additive;
   evidence `docs/evidence/step71b/` and `docs/evidence/step71/minerals/`).**
   The author's `Mineral.md` and its two deltas (`ROADMAP_6_delta_minerals.md`,
   `KESSHO_SPEC_delta_minerals.md`, folded the same day as `ROADMAP_6` step
   71 amended / 71b and `TO_PROJECT_SPEC` §10.7.8) ratified with the
   following resolutions, each the record of what shipped. **The ABI:**
   `sumi_mineral_t` (sized, 60 bytes, fifteen fields: the optic class, Γ, Γ₀,
   2V, the walk-off, the dispersion, the film's index and reflectance, the
   ring's radius, the body tint, the bloom gain, the chart id),
   `sumi_set_mineral` / `sumi_get_mineral` (a shorter `size` keeps the known
   prefix and defaults the rest from Quartz; the core clamps),
   `sumi_mineral_preset` (six: Quartz, Calcite, Muscovite, Barite, Fluorite,
   Selenite — the OPTICAL half only), and four fields appended to
   `sumi_params_t`: `drop_habit` (circle, rhomb, hex, rosette),
   `habit_roundness`, `habit_blades`, `tine_profile` (smooth, twin kink,
   terrace). The mechanical PAIRING — which habit and profile a mineral
   brings — is the shells' and the JSON presets', not the core's: the core's
   library is optical, `app_mineral_preset` on the desktop fills both halves,
   and `presets/kessho_<mineral>.json` carry both (the serializer's `mineral`
   block, `mineral_present`, applied through `sumi_set_mineral`); the halves
   stay independent parameters. The 1.6.0 Kessho palette library is retired
   (`sumi_palette_preset_count(KESSHO)` = 0): the chart is the mineral's
   `lut_id` — 0 the physical table, 1 the custom slot read once per order.
   **The crystal drop (the theorem, exact):** `drop_fs` evaluates R(θ) in the
   drop's frame — the rhomb as the polar support form r₀/max(|sin θ|,
   |sin(θ − 78.1°)|), the hexagon likewise over three normals, the rosette
   r₀(1 + 0.6 cos⁸ mθ) normalised to the mean radius — the hard max softened
   by a log-sum-exp whose sharpness is k = 2 + 40(1 − roundness)², then
   blended toward the circle by the roundness (`[ITERATE]` by eye: the
   corner at roundness 0 is nine times sharper than at 0.5, the crease
   visible, DECISIONS_10 #6's gate 7); the interior ink from r/R(θ). The
   strike's frame is the note's pitch axis (atan2 of the layout's axis); a
   held voice stores its habit at note-on and the feed grows it as the same
   habit — the fold's "area ∝ (1 + p)" is NOT implemented: the feed's law is
   unchanged (R = R₀ + ∫p·k dt, the nominal radius linear in the pressure
   integral, the area ∝ R²; measured 0.1197 → 0.1554 → 0.1909 at 100 / 200 /
   300 fed frames, steps equal within 1 %), as §10.7.8's "the same map, no
   new code path" requires; the spec's sentence is corrected. The engine's
   own pushes (the §4.6 field script, the gestures, `sumi_add_drop`) take
   the params' habit with angle 0; the fixture's drops are the circle.
   **The cleavage profiles (exact, bumps):** `tine_fs` takes a profile —
   Jaffer's α/(α + d) bitwise; the twin kink 1 to d = H/3 then linear to 0
   at H = 4α; the terrace stair₄(1 − d/H); and profile 3, DEV ONLY, the
   first draft's monotone ±1 ramp, the fabrication red control
   (`sumi_debug_set_tine_override`, `--tine-fault`). The glide tine, the
   master bend's shear and `sumi_add_tine` read the params' profile; the
   pinch's crossed tines keep Jaffer's. **What the soaks measured** (`--soak
   crystal_drop | twin_tine | terrace_tine`, 6000 passes): the two tines
   green on all four parts — twin kink (b) −1.19 % / dev 5.26 texel, (c)
   +0.20 %, (d) ×−0.03; terrace (b) −0.67 % / 5.33, (c) +0.00 %, (d) ×0.15
   (Jaffer's reads 11.9 texel at 500 pairs: the bumps' compact support
   resamples less). The crystal drop has no inverse pass (the fold's note),
   so its (b) is one rhomb pass on the identity sheet against the closed
   form — the pre-image det min 0.972 outside the rim annulus, mean 1.00004,
   the flatness median 1.16e-3 (the burst's bars, #4's) — and its stream is
   a clear-water habit drop every third frame, the habit and the roundness
   cycling: (c) +0.00 % (the gate), (d) INFORMATIONAL — 1.67e-4 per pass,
   ×14 the glide-tine control, because a drop's exact expansion carries the
   ink off the sheet (2000 strikes of r = 0.03 displace several canvas areas
   outward; the interior lost 132 k texels to the edge), the same geometry
   that excluded the spark's drop (DECISIONS_5 #13, #39), not resampling
   erosion; `--kessho-flat` reads the three operators at the field's floor
   (#4's gate: medians 2–3e-3 at ±3). **The red control:** under the
   unwindowed profile the gate goes red at its FIRST part, not at (c): an odd
   profile reversed is the same shear, so the "pair" carries both half-planes
   — and the ink — off the sheet (−99.89 % in 500 pairs, pre-image deviation
   305 texels), and nothing is left for the stream to fabricate from; the
   delta's "fails zero fabrication" is this failure under the gate's own
   order. **The analytic witness** (`tests/habit_det.c`, strict C11, no
   core): the shader's maps transcribed, |det J − 1| < 1e-6 at 10⁴ random
   (θ, roundness) per habit (measured ≤ 7e-9 with h = 1e-7, r ≥ 1.05 R —
   the rim is the v2 drop's singular pre-image), det 1 on the corner ray at
   every roundness, each ray monotone, the bumps zero beyond H and
   continuous, every profile's shear det 1. **The optical laws as built**
   (three stylisations the spec's wording did not fix, recorded here): (i)
   the biaxial brush keeps the analyzer's π/2 period — |cos² 2V − cos² 2(ψ −
   α)| / max(cos² 2V, sin² 2V), the Maltese cross splitting into two
   brushes 2V apart — because the per-voice analyzers carry (cos 4α, sin 4α)
   (K-1's wrap, #3) and a π-periodic law has no sign to read from them;
   the lemniscate of the proposal is not what ships; (ii) the sensitive-tint
   plate is FIXED TO THE SHEET at 45° (β = π/4 in the sheet's frame, Γ_eff =
   |Γ₀ + Γσ sin 2ψ|), for the same reason, and the plate replaces the brush
   — a full-wave plate floods the cross, so under Selenite the analyzer's
   turn plays no part; (iii) the walk-off doubles the CHART under the
   point's own brush (the first form averaged the brush too and filled the
   dark arms in: on the cross's diagonals the double image must stay
   closed — measured 1.16/255 against no walk-off on the arms, 13.3 between
   them). The thin film reads h from the band's radial fraction (200–800
   nm); the dispersion's three depths are z/√(1 + d), z, z√(1 + d); the
   conical ring is masked to |Δ| < 0.15 and σ > 0.05; the chart beyond 2200
   nm fades to 0.92 white over 4000 nm; the substrate is 0.018·body; the
   caustic's light is the body (the accent is gone with the palette); the
   bloom's source is Layer B × bloom_gain. **Step 71 amended, gates 8–12**
   (in `--kessho-test`, 17 checks): the six minerals over one field — the
   field bitwise, every render distinct; the selenite sign — a tine's two
   sides read (b − r) 14 and 57, a 43/255 swing, 2.4 at Γ₀ = 0 (red);
   fluorite = Layer B alone — the light export IS the caustic export
   bitwise, quartz 794 k bytes apart; the dispersion fringe — 78 k of 139 k
   lit caustic texels off the body's ratio in linear light under barite,
   0 at dispersion 0; the walk-off collapse as above. **The step's gates:**
   the homothety (a rhomb's fract-0.5 ring is the rhomb at f = 0.1503 of
   r₀ 0.3, residual 0.49 texel; the circle under the rhomb model misses by
   6.1 — red), the hexagon and the rosette at 0.33 / 0.30; the growth
   (residuals 0.26 / 0.25 / 0.26 texel at 100 / 200 / 300 frames); the
   crease (corner-ray contrast 0.464 at roundness 0, 0.060 at 1); calcite +
   the twin kink (the lamella's interior 38 against its lit ramps at 222 in
   Kessho; Jaffer's 189 against 198 — no lamella; the Sumi facets archived);
   the pass count (a rhomb strike one pass, a twin-kink stroke one, as the
   circle and Jaffer's — `sumi_debug_pass_count`). The field and composite
   fixtures bitwise on Metal, ctest 13/13 (`habit_det` 22/22, the preset
   round trip with the mineral), the web shell compiles. **Deferred:** the
   Suzu conical intersection (Phase 13, `[ITERATE]`); the book's medium
   guide and the tablets' Mineral pages (their own steps); the author's
   viewing of the six at the fold's exposure (the one `[ITERATE]` number).

7. **The settings window, the author's two asks (2026-10-14).** One line
   per medium: the Medium row's third radio clipped at the window's right
   edge, so the three media are vertical now and every medium added later
   takes a line of its own. And the medium key: the bench's legend named the
   US positions — "A medium" — while the author's AZERTY presses "Q" there;
   `dev_key_name` asks GLFW for the key's name on the live layout (a single
   printable byte translates, A → Q, W → Z, M → ","; the digits keep their
   caps), the legend is built from it once, and the Medium section itself
   carries the line "Lab bench: the Q key on the canvas cycles Sumi → Anod →
   Kessho" under `--dev`. The Kessho section's "Retardation" knob is
   "Exposure" now (2000 = one stop), since the retardation is the mineral's.
   Under Kessho the Palette section gives way to a Mineral section: the
   library combo (the six and *Edited*), the optic (class, 2V, retardation,
   sensitive tint, walk-off, dispersion, conical ring, thin film and its
   index, body, bloom gain, chart) and the habit (drop habit, roundness,
   blades, tine profile); the INI carries `mineral_*` and the four params,
   the session JSON the `mineral` block. The author's third ask, the same
   evening: the panel's "Mineral" combo hashed to the "Mineral" header's ID
   (ImGui IDs are label hashes within the window) — the panel is scoped
   under its own ID now.

8. **The author's viewing of the minerals (2026-10-14): the closest real
   habit per mineral, roundness 0, reviewed at the phase's end.** "I like
   the mineral" — and the pairing follows nature rather than the delta's
   table: quartz the hexagonal prism's section, calcite its cleavage rhomb,
   muscovite the pseudo-hexagonal plate, barite the desert rose's blades
   (m = 5), fluorite the cube seen along its body diagonal (a hexagon — a
   square, the face-on cube, is not a habit yet; a candidate for the
   review), selenite the rhombic cleavage plate of gypsum (the rhomb's
   angles are calcite's 78.1°, not gypsum's ~66°; likewise a candidate).
   Every habit ships at roundness 0 — the sharp polygon, the corners
   creasing — "for the moment", the roundness law and the pairings to be
   reviewed at the end of Phase 11 with all the new operators on the sheet.
   The tine profiles keep the delta's pairing (calcite the twin kink,
   muscovite the terrace, the rest Jaffer's). Changed: the desktop's
   `app_mineral_preset` table, the six `presets/kessho_<mineral>.json`, the
   core's default `habit_roundness` (0; the default habit is the circle,
   which ignores it), the book's and the spec draft's tables. Next: step 72.

9. **Step 72 — the Arnol'd tongue lock (2026-10-14; libsumi 1.8.0, additive;
   evidence `docs/evidence/step72/`).** `SUMI_DEFORM_ARNOLD`, one pass per
   voice per frame: M = T ∘ Kick about the voice's drop in the action-angle
   frame (ρ = r²/2, θ) — the kick from the generating function F₂ = θρ₁ +
   (K/q)·W(ρ₁)·cos q(θ − φ), the twist the Lamb–Oseen rotation with its
   core at the ring (the swirl voice's convention, r_c = R), scaled so the
   ring turns Ω₀ turns a frame; the lookup is M⁻¹ (the twist undone, then
   one scalar Newton on θ, ρ explicit), composed in ONE lookup — the spec's
   two passes (the kick, then the existing swirl pass) became one, which
   halves the passes and resamples once. Class exact: the four-part gate
   green (`--soak arnold`: (b) through the pass's own EXACT INVERSE,
   `inverse` = 1, the lookup of M by one Newton on ρ₁ — −0.13 % / 6.1
   texel; (c) +0.00 %; (d) ×0.09), the flatness at the field's floor (#4),
   the pass against its CPU closed form to 0.12 texel, the kick then its
   inverse 0.38 texel. **Five corrections to the spec as written, each
   forced by the mathematics (`TO_PROJECT_SPEC` §10.8.1 marked):** (i)
   K_max = 0.8 / max|W′|, not 0.8·q / max|W′| — the θ-shift's derivative is
   −K W′ sin qθ, the q cancels; at the spec's bound the kick folds for every
   q ≥ 2; (ii) THREE Newton iterations leave 1e-3 at the 0.8 bound (the
   basin's ratio c²/(2(1 − c)) = 1.6 > 1); FIVE reach 1e-11 (the study in
   the log: three suffice only below c = 0.5, half the kick); the red at 1.3
   K_max (|K W′| = 1.04, the fold) reads 1e4; (iii) the window W must be ZERO
   AT THE CENTRE: with W = 1 down to ρ = 0 the inverse ρ = ρ₁ − K W sin qθ
   goes negative — material sent to negative action; W rises C² from 0 to 1
   over [0, ρ_drop/2], is 1 to ρ_drop, falls C² to 0 at 1.5 ρ_drop; (iv) the
   twist is C²-windowed to zero between 1.75 R and 2.5 R (a windowed rotation
   is still a rotation by a function of r: exact) — the bare Lamb–Oseen tail
   at half a turn a frame would wind the whole sheet; beyond 2.5 R the pass
   copies the texel exactly; (v) the tongue takes the continued-fraction
   convergents of order 2..6 only: 0/1 and 1/1 are not tongues (they would
   swallow every interval); an integer ratio (within the q = 6 tongue's
   half-width) is q = 1 with the signed detune, and twists a FULL TURN plus
   the detune — Ω₀ = 0 would leave no twist and no islands. **The lock and
   the beat, as built:** the kick's frame φ precesses by 2πδ a frame, δ =
   Ω₀ − p/q, and the twist turns Ω₀ = p/q + δ — in the precessing frame the
   rotation is exactly p/q and the islands stand; in the lab they turn at δ.
   The dynamical alternative (a fixed kick frame, the true circle map) LOCKS
   a small detune into a radius shift and hides the beat the author asked to
   see; the precessing frame shows it. **The bass keeps the swirl** (press
   → the Lamb–Oseen stir as under press_mode 1): the reference voice has no
   interval, and a solo note under pressure must still do something. A q
   change crossfades K to zero and back over 50 ms (ARNOLD_XFADE_S,
   `[ITERATE]`), q switched at the zero — the headless witness: the records
   across a two-semitone bend sweep (q 2 → 5 → 3) have |ΔK| ≤ 0.26 K_max a
   frame, every q change through a frame at K = 0, |Δφ − 2πδ| ≤ 2e-7.
   Kessho's press default is 3 (the lock); press_mode 3 is accepted in any
   medium; SUMI_CTL_ARNOLD_K is the CC alternative (the larger of the two).
   **What the gates measured (the bench's `--arnold-test`, 7 checks):** gate
   4 at the EXACT ratios through the gesture (Ω₀ = 1/2 and 1/4, 600
   iterations at the press-100 kick): the q-fold pattern's angle stays
   within its island — excursion 0.30 rad for q = 2, 0.14 for q = 4 (the ink
   LIBRATES inside the islands; the islands stand) — the proposal's "< 0.5
   texel" cannot be read off ink that circulates inside an island, and a
   MIDI fifth cannot be exact: equal temperament's 2^(7/12) is two cents
   under 3:2 and its islands precess at δ = −0.00169 turns a frame (one turn
   in 600 frames; measured −0.0018), the tempered major third fourteen cents
   over 5:4 at +0.0099 — THE TEMPERAMENT'S BEATS ON THE WATER, the finding
   of the step; gate 5 the tempered fifth bent ten cents (Ω₀ = frac(2^(7.0996/12)),
   δ = 0.00695): the pattern precesses with the twist's sense at 0.00694
   turns a frame over 1600 frames — within 0.2 % (the ink's harmonic phase
   wobbles with the libration, which the long run averages; the records'
   phase is exact to 2e-7); gate 6 redefined: the
   proposal's "without the twist the petals grow without bound" does not
   happen — the window bounds the kick either way — what the twist does is
   make the ISLANDS: without it the kick alone has no elliptic points, the
   ink collects on the kick's nodal lines (the 4-fold harmonic 0.29 over the
   2-fold 0.01); with it the two islands (0.084 over the red's 0.010); gate
   7's "no frame-to-frame Δ > 1 texel across a tongue boundary" is the
   records' continuity (the headless test above); the sheet's band flicker
   through the crossing is measured and archived (mean 0.09, max 0.17 of
   the drop's texels; 0.145 at the first q change). **Seen in the renders:**
   under the lock a pressed drop becomes a cool (elliptic) disc in Kessho
   with its islands as bright crescents, and the first drop struck being
   ink and the second clear (the parity form), a pressed clear drop shows
   its islands as a thin ring of band-1 phase about a white interior — the
   bench strikes the upper voice first. The half-turn-a-frame twist of a
   fifth shears the drop's interior to grey within a second (the Lamb–Oseen
   core turns 1.58× the ring); the islands at the ring stand. PENDING THE
   AUTHOR: the stroboscopic rate — one map iteration per frame at Ω₀ turns
   is the spec's; a slower cadence (`[ITERATE]`, the spec's slow-motion
   factor) would spare the interior; the 50 ms crossfade; the look in
   Kessho. Next: step 73, Kirchhoff braiding.

10. **Step 73 — Kirchhoff chord braiding (2026-10-14; libsumi 1.9.0, additive;
    evidence `docs/evidence/step73/`).** bend_mode 5 is live and Kessho's
    bend default: every sounding voice is a Lamb–Oseen vortex at its drop
    whose circulation is its consonance against the lowest held note,
    Γ_k = sign(C_k)·Γ_vel·(0.3 + 0.7|C_k|), the bass Γ_vel positive, the
    others scaled by SUMI_CTL_BRAID_COUPLE (1 unmapped); the ink is advected
    by the superposed flow — never integrated: the exact swirl passes, one
    per voice at the half-circulation, composed in STRANG ORDER (1..N then
    N..1), 2N passes a frame, every factor a rotation, so the composition is
    det 1; no new pass type and no new entry point (the spec's `SUMI_DEFORM_
    BRAID` is the swirl pass's sequence). **The consonance curve as built**
    (`sumi_consonance`, `core/src/midi_normalizer.cpp`): Sethares' sensory
    dissonance over the first six partials (amplitudes 0.88^(n−1)) at the
    bass's frequency, mapped on THREE ANCHORS — the unison +1, the tritone 0,
    the minor second −1 — piecewise linear in the roughness between them;
    the register floored at C4 (261.6 Hz): below it the raw curve is rough
    everywhere (a low third IS muddy, but the mapping needs its anchors —
    at C3 the linear two-anchor map read the major third −0.56 and the
    fourth −0.26). At C4: the fifth +0.68, the fourth +0.34, the major
    third ≈ 0, the major sixth +0.37, the octave +1 `[ITERATE]`. **The floor
    made continuous** (`[ITERATE]`): sign(C)·(0.3 + 0.7|C|) jumps from −0.3
    to +0.3 at the tritone; shipped as the line 1.7·C for |C| < 0.3 (the
    spec's formula beyond), so a bend through the tritone flips the topology
    without a jump — and an EXACT tritone carries no vortex (C = 0). **The
    voice's bend in every mode:** the mapper keeps `bend_s` beside the glide
    (the lock's pitch and the braid's both read it; the step-72 lock read
    the glide, which modes 1–5 leave at rest). **Free (Kirchhoff), B-1's
    B:** `sumi_braid_step` (`displacement.cpp`), the N-vortex Hamiltonian
    with the pairwise regularization r_c² = R_j·R_k (the cores' product),
    the implicit midpoint rule iterated to 1e-15 (the spec's three rounds
    leave the quadratic invariants at 1e-6; six rounds reach machine
    precision), the positions in DOUBLE (ten minutes of float rounding walk
    the centre of vorticity to 1e-6; in double 4e-16): over 72000 steps of a
    consonant triad |ΔH/H| = 3e-9, the centre of vorticity and the angular
    impulse 4e-16 and 6e-16 — gate 6 green. A note re-anchors at its cell at
    the strike; a lifted note's vortex fades over BRAID_RELEASE_S = 0.25 s
    going on its way; the 5 % clamp `[ITERATE]`; ONE vortex a voice (the
    first echo — Jankó's rows would be 90 passes). Γ_vel = BRAID_GAMMA_S·v =
    0.036 canvas²/s at full velocity `[ITERATE]`: a fifth's pair orbits in
    ~4 s, a minor second's dipole walks a third of a texel a frame. **What
    the gates measured** (`--braid-test`, 4 checks; `--soak braid`;
    `--kessho-flat braid`; the headless suite): gate 1 fifteen voices → 30
    swirl records a frame (the headless witness; the bench's chromatic
    cluster reads 28 because its tritone, C = 0 exactly, has no vortex),
    10 ms a frame at 512² and at 1024² (the bench's frame pacing, not the
    passes: 60 fps holds); gate 3 a fifth's dyad after 300 frames: the
    saddle between the cells at 54 % of the way from the bass — nearer the
    weaker vortex, as the velocities' balance puts it — its material moved
    1.8 texel (a stagnation point; the texel nearest it drifts by the
    saddle's own stretching), the discriminant there +0.51, the cores −3.6
    and −2.6 (elliptic); gate 4 a minor second's dyad: the ink's centroid
    8.4 texels across the key line in 120 frames (the dipole's jet; 0.7
    along it); gate 5 the second bent six semitones to the fifth over 240
    frames: the material never moved more than 0.25 texel a frame; between
    the cells the jet's least-moved material came 22 texels upstream, the
    fifth's 0.4 — a stagnation point appears — but at the SECOND's geometry
    (the cells 0.070 apart, the cores 0.087) it is ELLIPTIC, the one centre
    of two overlapping cores co-rotating, not the saddle of gate 3 whose
    cells sit 0.58 apart: the saddle needs the cores apart; the flip the
    gate sees is jet → co-rotation (the records' sign flip is the headless
    witness, |ΔS| ≤ 5 % of the bass's a frame). **The soak:** (b) the
    two-vortex Strang pair at ±Γ — the exact inverse — holds the mass
    −0.70 % with a pre-image deviation of 32.3 texel against the bench's bar
    of 32 for TWO-pass pairs: the pair has four passes each way, and the
    resampler's walk grows ~N^0.7, so the bar is scaled by (passes/2)^0.7
    (SoakDesc.pair_passes; 52 for the braid); (c) +0.00 %; **(d) ×10.9 the
    glide-tine control — INFORMATIONAL for the braid:** a dyad resamples the
    whole sheet four times a frame for as long as it is held (the Lamb–Oseen
    tails never vanish), fifteen voices thirty times — the Strang
    composition's own cost, not a leak. The mitigations, the author's:
    alternate the order frame by frame (Lie–Trotter, N passes a frame,
    Strang over two frames — halves it), window the far field as the lock's
    twist is (loses the long-range saddle), slow Γ_vel. The flatness at the
    field's floor (median 8.5e-3 at ±3). **A/B B-1** (`--ab braid`, a triad
    — the bass, its fifth, its minor second — 600 frames, pinned and free,
    Sumi and Kessho, `docs/evidence/step73/braid_ab/`): the author's
    viewing; pinned stays the default. In Kessho the chord's accumulated
    flow fringes the whole sheet within seconds — the same remark as the
    lock's rate (#9). **B-1 DECIDED by the author's viewing (2026-10-14): pinned
    is the default; free survives as the non-default "dance" mode** (the spec's
    allowance), reachable in the settings and by the D key. The author's ask after
    the step: a lab key — D toggles
    the braid's vortices (pinned / free) live; the P and M keys now cycle every
    press mode (the lock included) and every bend mode (the braid included)
    through the medium's default, Shift backwards; the legend names them.
    Next: step 74, the chord-wave lattice.

11. **Step 74 — the chord-wave lattice, route (a) (2026-10-14; libsumi 1.10.0,
    additive; evidence `docs/evidence/step74/`).** Under Kessho the poly
    pressure (0xA0) of a held note is the amplitude A_k of a ripple-tine
    through its drop — the ripple pass given a window: `sumi_deform_ripple_t`
    gains `win_c, win_w` (0 = none, the v2 whole-canvas ripple bitwise) and
    `ripple_fs` multiplies the sine by a C² bump across the band; a shear by
    a function of s₂ alone, windowed or not — exact. No new pass type, no
    new entry point (the proposal's `SUMI_DEFORM_CHORD_WAVE` is the ripple
    pass windowed), no new params. **As built:** the axis ê₁ at the pitch
    class's angle on the circle of fifths, FIFTEEN degrees a step — the
    circle's thirty would give a tritone pair the same orientation (a
    ripple's lines have period π), fifteen gives the twelve classes twelve
    distinct orientations across a half turn `[ITERATE]`; the wavenumber by
    the octave, four periods a canvas at octave 3 doubling an octave up,
    clamped 2..32 `[ITERATE]`; the window's half-width 2.5 drop radii
    `[ITERATE]`; a crest at the note (φ = π/2 − ω s₂,k); the amplitude at
    full pressure CHORD_WAVE_AMP = 0.02 canvas (ten texels at 512)
    `[ITERATE]`; delta-driven as the ripple bake is — the passes compose
    additively at fixed (ω, φ, angle), so the baked amplitude follows the
    pressure both ways, one pass a voice per change, nothing while held, and
    the wave stays baked when the note lifts (the ripple's permanence); one
    band a voice (the first echo). The voices compose as separate passes.
    **What the gates measured** (`--chord-wave-test`, 2 checks; `--soak
    chord_wave`; `--kessho-flat chord_wave`; the headless suite): one note
    (G3) at full pressure — across its band the wave's displacement along
    the axis (the strike's own field subtracted) fits A sin(ω s₂ + φ) w(s₂)
    with |correlation| 0.978, amplitude 10.2 texels, 0.00 outside the band;
    the pass alone on a fresh sheet reads the same profile a quarter canvas
    along the axis (the band runs the sheet's length — on a struck sheet the
    baked wave re-samples the strike's own radial field, which is not
    uniform along the band, so the length is read on the bare pass); a fifth
    (C3 + G3) at full pressure — two bands at 0° and 15° crossing at (0.60,
    0.39): along G's band away from the crossing G's component alone (13.5
    texels, the other's residue 2.0), along C's C's alone (13.1; 2.3), at the
    crossing both (19.9 and 19.9) — THE LATTICE; a triad's crossing region
    archived. The soak green on all four parts — the pairs at ±A (the exact
    inverse of a shear) −1.07 % / 8.5 texel, (c) +0.00 %, (d) ×0.20 the
    control; the flatness at the floor (median
    2.75e-3 at ±3); the fixtures bitwise; the headless suite pins the
    records (the angle by the class, the periods by the octave, the crest at
    the note, the deltas up and down, held none) and Sumi's swirl untouched.
    **Route (b), the local rosettes** — the generating-function shear
    windowed in both coordinates, a 2 × 2 Newton per note per texel, the
    only way to the first draft's 3/5/6-fold quasicrystals — stays
    `[ITERATE]`: the spec says go or defer after (a) has been seen on paper;
    the renders are the author's to see (`docs/evidence/step74/renders/`,
    Sumi and Kessho). Next: step 75, the Poincaré strobe — no core change;
    the core is frozen again from here to step 91.

12. **Step 75 — the Poincaré strobe, the trace's second mode (2026-10-14;
    Voxo 0.17.0 and `trace/`; NO core change — the core is frozen again from
    here to step 91; evidence `docs/evidence/step75/`).** **Voxo** keeps the
    reference phasor the proposal asked for: the lowest HELD voice's frequency,
    bend included (the lowest sounding one when none is held — "the
    normalizer's bass" read on Voxo's side, where the voices are), advanced per
    sub-step in double along the block's glide line, the block's wraps
    precomputed once (the sub-step each completes in and the fraction into it,
    at most 64 a block) so every voice's loop reads the same instants; at each
    wrap the traced voice's pair — the trace's own (x, y), with the kind's aux
    pair — goes into a per-voice strobe ring of 256, interpolated between the
    two sub-steps the wrap falls between: a RETURN. `voxo_set_strobe` (0 off,
    1 the bass phasor, 2 each voice's own drive where it has one — the Duffing
    cell at its drive's phase wrap, undriven the bass; the kicked rotor at each
    kick, where the returns (θ = atan2(y, x), p = u after the kick) ARE
    Chirikov's section — the rest the bass), `voxo_strobe_poll` (the newest ≤
    32 returns a voice since the last poll, raw in the cell's units, with the
    count since the strike and the clock's frequency), `voxo_strobe_hz`.
    Captured only for the kinds in the trace mask; a reader; nothing
    allocates. **`trace/`:** `sumi_trace_config_t.mode` (SUMI_TRACE_CONTINUOUS
    0, _POINCARE 1, _POINCARE_OWN 2); the bridge sets Voxo's clock each frame
    (a shell adds the combo and nothing else — the tablets' *Synth trace* rows
    gain it in their own sub-steps); per voice an `OrbitDust` ring of the
    newest 128 returns for the scopes (the settings miniature draws them as
    fading dots, the canvas scope as dashes round-robin across voices up to
    its 128); on the ink route a micro-drop under Sumi (`sumi_gesture_tap`,
    r = 0.003 canvas × scale/0.25), a CHARGE SPECK under Anod (an ink drop of
    0.004 × scale/0.25 — the strike's own charge call; the continuous trace
    lays one at every kept vertex there too) or a micro-swirl under Kessho
    (`sumi_gesture_twist`, strength 0.06, core 3r, Lamb–Oseen), through the
    hook (kinds 0 / 7 / 2: a recording carries them), the frame budget of 24
    shared fairly. **Desktop:** the *Suzu trace mode* combo, `suzu_trace_mode`
    in the INI, `trace_mode` in the preset (the schema, the round trip
    pinned). The bench's `--strobe-test`; the suite's block 28; the fuzz's
    `--strobe-seconds` (ctest runs 5 s; the evidence run 120 s);
    `tools/strobe_chart.py` draws the returns (`SUZU_STROBE_DUMP`).
    **What the gates measured.** 1:1 — the undamped cell at A2 strobed at its
    own frequency, 2 s: 219 returns (220 expected) within 0.001 texel of one
    point (the gate: 0.5 at the default scale, 512). 3:2 — E3 bent +3 units
    to the just fifth (the bend's resolution is 0.59 cents; the residual 0.20
    cents), 1 s: 109 returns alternating between two points 3.142 rad apart,
    each class within 0.120 texel over every tenth of a second; the
    equal-tempered fifth unbent precesses its pair 1.132 rad over the second
    (1.128 from the 1.955 cents) — THE STROBE READS THE TEMPERAMENT, the same
    δ = −0.00169 the lock beats at (#9). The bass bent +49.8 cents: the phasor
    at 113.2105 Hz (the ratio 1.02919, exact to 1e-5), 1132 returns in 10 s
    against 1099. The tritone (√2, exact in ET) over A2: every one of sixteen
    angular bins visited, the radius within 0.5 %: the dotted ring. The
    Duffing cell by its own drive: 219 returns in 2 s (one a drive period; its
    section at drive 0.6 a three-point cycle, charted). The rotor by its own
    kicks, undamped, K swept 0.3 / 0.6 / 0.9 / 1.2 / 1.8 / 2.5 (12 s each,
    charted `strobe_sections.png`): over the first 440 kicks 10 / 12 / 20 /
    18 / 47 / 22 % of a 24 × 24 (θ, p) grid, |p| ≤ 1.11 at K 0.3 — the
    separatrix's eye from the strike's hyperbolic point (θ₀ = ε/2, p₀ = 0),
    2√K; the sea at 1.8. Bit-identical with the strobe on and off for kinds
    0, 1, 4, 5, 7 (three bent voices, polled every block), zero global
    allocations and the malloc zone's blocks unchanged across the loop; the
    fuzz's strobe run 120 s, 0 blocks differing from the unstrobed twin. On
    the live core (`--strobe-test`, 8 checks, Sumi, Kessho and Anod renders
    archived): the scope route alone leaves the field bit-identical to the
    untraced run; the ink lands under both media; through the hook the bass
    alone on one point (95 stamps within 0.001 texel), the just fifth on two
    sides of its cell (47 + 46 stamps, every one within 0.011 rad of the
    pair's axis, the pair precessing 0.0011 rad a return — the residual's
    rate), the tritone's 95 round all sixteen bins; the rotor's dust inked. The
    fixtures bitwise on Metal; the web wasm's gate GREEN; the preset round
    trip 40/40. **What the strobe found** (it is the instrument Phases 12–13
    will hold every voice to, and it began on the rotor): (1) the rotor's
    section is Chirikov's map only approximately — the retune's re-basing at
    each kick is not exactly area-preserving in (θ, p) — and at K 2.5 the
    undamped rotor SINKS into a period-4 cycle within ten seconds (the
    occupancy by thirds 22 → 7 → 6 %), while a damped one's amplitude is in
    float noise by six (its T60 of 3 s); the gate reads the first 440 kicks
    and prints the thirds — Phase 12's rotor, `[ITERATE]`. (2) The strobe
    fuzz found two Voxo faults, fixed in 0.17.0 with their checks in block 28:
    a patch of ANOTHER VOICE KIND over sounding notes rendered the old bodies
    (cells under the trumpet's bore were NaN in two blocks, and a NaN never
    leaves the bus) — a kind change ends the voices, the same kind's knobs
    stay live; and the flute at B5 bent +47 semitones in one jump (17.6 kHz)
    was NaN in its block — the jet's receptivity band past the SVF's range,
    the bore at four cells — so the jet and the bore are built for the pitch
    up to rate2/16 (6 kHz at 48 kHz, above any flute's top) and saturate
    there, as the chain's pitch does at its CFL bound. And THE BREAKER: a
    non-finite block never leaves the bus — the voices end, the reverb and
    the delay clear, the block goes out as silence, `voxo_stats_t.breaker_trips`
    counts it (the lab's forced-unstable chain trips it; the next note
    sounds; the suite's four red controls — the chain's CFL, the loop's
    passivity, the bore's CFL, the valve's junction — now read the trip as
    their red, since a blow-up no longer reaches the output). (3) The ink route floods at the clock's rate: every return a
    drop at a stationary point composes — 876 of three texels in 4 s made one
    drop of 0.18 canvas — so the stamps are a texel and a half and the ink
    route keeps about INK_RATE = 23.7 returns a second per voice (a credit
    of 23.7/clock a return, a stamp when it reaches one — a FRACTIONAL
    stride, because every N-th return aliases the section: a 2-cycle inked
    every tenth return is one point, measured on the bench; and not a round
    rate, so no equal-tempered pitch divides it), the scopes every return
    `[ITERATE]` — the author's eye on the rate and the micro sizes. **The
    author's viewing (2026-10-14): the rate is right, and in Sumi and Kessho
    the micro stamps make the strobe read better than the continuous
    segments; in ANOD both modes are imperceptible** — the medium's rule:
    only CHARGED material glows by strain, and the water draws only its
    grid where the displacement exceeds a texel; the trace's tines and the
    strobe's micro-swirls land on water around a charge a third of the
    strike's radius (the step-59 figure read because its demo's charges
    were torn into streamers the tines then strained). An Anod stamp must
    be the medium's own event; **the author chose the CHARGE SPECK for both
    modes** (over the micro-spark, an episode per return): an ink drop of
    two texels glowing at the charge's base (0.22) and brightening under
    the chord's own strain — the strobe's returns and, in the continuous
    mode, one speck along the orbit at the same ink rate (a speck at every
    vertex flooded: 5040 in 4 s charged discs far past the orbits; the
    bridge has no clock, so the continuous credit runs per frame at an
    assumed 60 fps), kind 7 through the hook; the bench's Anod renders
    (`strobe_anod.png`, `trace_anod.png`) are the author's to view. **Open
    for the author:** the tablets' mode row; the rotor's symplecticity
    (Phase 12). Next: the phase close — the fold of
    this file as Part X (after the commit, the author's word), the pre-release
    tag `v3.0.0-alpha.1` the author's, then Phase 12 (step 76) under
    `_work/DECISIONS_11.md`.

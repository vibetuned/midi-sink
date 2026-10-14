# The open roadmap — v3.0, Phases 11–16 (steps 71–92)
**Phases 1–10 (steps 1–70) are DONE and folded into `docs/ROADMAP.md` Parts 1–9; `v2.0.0` shipped on 2026-10-14. This file is the v3.0 arc, folded on 2026-10-14 from the author's three proposals in `_work/specs/`: `roadmap_voxo.md` (the Suzu, voices and sampler phases, and a first draft of the carry-over and release phases), `roadmap_voxo_gate_delta.md` (its review fold — every amendment is applied IN PLACE below and marked *gate delta*), `road_map_medium_v3.md` (Kessho and harmony, the carry-overs, the release — which replace the first draft's carry-over and release phases), with the Kessho spec `medium_specs_v3.md` (`KESSHO §n`). THE ORDER IS THE AUTHOR'S (2026-10-14, `DECISIONS_10 #2`): harmony first — the proposals were numbered Voxo-first (their Phases 11–13, then Kessho at 14); this file is renumbered to the order below and its step numbers are final. The proposals stay in `_work/specs/` until the author removes them; where this file and a proposal disagree, this file is the fold and `_work/DECISIONS_10.md #1` says why.**
**Companions: `docs/PROJECT_SPEC.md` (`SPEC §n`, spec v5); `_work/TO_PROJECT_SPEC.md` — what this arc adds to the spec, drafted FROM THE START this time (the proposals as the spec, corrected to the tree; each step corrects the subsection it ships and cites its entry; the author transcribes at the fold); the decision log `_work/DECISIONS_10.md` (Part X in flight, Phase 11's; Phase 12 opens `_work/DECISIONS_11.md`, and so on — one file per phase, as the v2 arc did); history `docs/CHANGELOG.md` (a `## v3.0.0` section opens with step 71); the v2.0 records `docs/DECISIONS.md` Parts V–IX (`DECISIONS_5 #n` … `DECISIONS_9 #n`).**
**Scope: six phases, one public release, two features. (1) The picture — Kessho, the third medium: the field read as a birefringent crystal (λ, ψ, cos φ) with its iso-trace caustic; and symplectic harmony, the coupled multi-voice operators that turn a chord into geometry (the Arnol'd tongue lock, Kirchhoff braiding, the chord-wave lattice) with the Poincaré strobe in the trace — Phase 11, FIRST: the riskiest, taste-decided work (two A/B viewings, a Newton kick in the shader, the caustic) gets the arc's slack; the core reopens once, early, and freezes for the eleven Voxo steps; the strobe becomes the instrument every new voice is seen through at its own step. (2) The sound — Suzu's deferred physics (the SoA SIMD engine and the ×4 oversampler; 2D membranes, plates and gongs; the stick-slip bow; the bore's toneholes, two-mass lips and shock; editable tables with Scala and unison — Phase 12), four new symplectic voices (the Toda ring, the Boris gyroscope, the billiard, the Dirac network — Phase 13), then Voxo's second-generation Decent Sampler coverage (sustain mechanics, chorus and phaser, the filter family and the modulators, legato, the compat report pruned) with the §16 sound carry-overs (Phase 14). Then the carry-overs (Anod's determinism and strain buffer, the Chirikov grid, the Boehm layout, the split surface, preset-next — Phase 15), the web lab, the books and the 3.0 release (Phase 16). Hard orderings: within Phase 11 the microscope before the specimens (Kessho's z = 1 flatness gate is the det-J gate every harmony operator is held to); within Phases 12–13 the ×4 oversampler (76) before the Toda ring (81) and the billiard (83); the strain buffer (90) after Kessho (71), which reads instantaneous σ until then; docs last (`/marble/` is the newest stable tag). One step per session, as always.**

---

## Working Rules (apply to every step)

* All prior working rules hold (`docs/ROADMAP.md` Parts 1–9). The core is REOPENED for feature work in Phase 11 (libsumi 1.6.0, additive: `SUMI_MEDIUM_KESSHO`, four control dimensions, two mode fields, `sumi_add_arnold`, one export flag, the three operators' passes) and at step 91 (`SUMI_LAYOUT_BOEHM`); every other step leaves `libsumi` untouched (Voxo, `trace/`, `presets/`, the shells) or touches it under bug → regression test → fix. Core changes prove out on the desktop harness FIRST, every time.
* **The phase invariant:** `tests/fixtures/field_512_metal.bin` stays BITWISE on Metal (DECISIONS_5 #87; GL, D3D11 and GLES hold their tiers). No step of this arc changes the field's payload: Kessho is a reader of the same (u − x, v − y, ink, aux) field, the harmony operators add passes, the strain buffer is a side buffer. The §10.1 switching gate (the field bitwise across Sumi ↔ Anod ↔ Kessho) is the arc's second invariant.
* **Every operator declares its class** (SPEC §10.2; §13.1 for the audio twins) in its header comment, its test and its book page — and this arc adds a rule (KESSHO §2.1, the exactness rule): *exact + localised ⟹ twist-type, or generating-function (implicit, Newton in the shader)*; everything else is sub-stepped and says so. Kessho's `--kessho-flat` (z = 1 is flat for any exact field) is the runtime check of the rule and a gate on every operator of Phase 11; the four-part conservation gate (`--soak`) stands. Negative controls prove every new gate red before it is trusted — the "red" items below are not optional.
* The delta rule and the one-consumer rule (`bend_mode`, `slide_mode`, `press_mode`) are unchanged; Kessho adds *defaults* for them (`press_mode` 3, `bend_mode` 5, `slide_mode` 3), never a second consumer.
* **One platform per step.** Core, Voxo, `trace/` and shared UI are authored on the desktop harness (the Mac); iOS and Android on the Mac too (the tablets plugged in); the Linux box is out of commission (SPEC §16 — the Linux lane runs on GitHub's runner; Linux gates stay recipes); the Windows box runs the Windows lane and nothing else. The proposals' "(Linux box)" for step 90 and "(Windows box)" for step 91 are corrected to the Mac — both are core and shared-UI work (DECISIONS_10 #1). Verification fan-out stays the sanctioned exception.
* **Composed gestures inherit the strictest class of their members** — the braid is exact by construction (a composition of det-1 factors) and is gated anyway; the Kessho strike (a lens drop and a caustic ring) inherits the drop's class.
* **The ABI grows additively** (the Step-33 minor-bump pattern): libsumi 1.6.0 at step 71 — one bump for the phase, steps 72–74 filling what 71 declared, or a bump per step if a step adds a symbol 71 did not declare (the header's ABI history says which); Voxo's version per its own ABI changes (its 0.x line continues — "Voxo DS 2.0" names the sampler's format coverage, not the library's version; the author's call to rename, DECISIONS_10 #1); `trace/` and `presets/` likewise. Version strings come from the git tag, never hand-edited.
* **Voxo's audio-thread contract** (SPEC §12.1) holds for every new voice and effect: zero allocation on the audio thread, the storm (`--voxo-storm`) at 0 XRuns from a Release build, long `voxo_fuzz` runs (`--seconds`), output bit-identical with any new tap (the strobe) on or off. **Every synth voice ships with its sound profile** (DECISIONS_7 #9): `--voxo-profile` and `tools/sound_profile.py`, the figure in the step's evidence and the Suzu book.
* **The web gate's bit-for-bit contract** (`tools/suzu_web_gate.mjs` against `build/tests/suzu_web_reference`) is restated at step 76 BEFORE the SoA SIMD refactor lands — the scalar path stays the reference build, or the gate becomes a tolerance gate; recorded, not discovered.
* Evidence per step under `docs/evidence/<step>/`; at each phase end the fold: that phase's `_work/DECISIONS_<n>.md` merges into `docs/DECISIONS.md` as the next Part, evidence condenses into `CHANGELOG.md` and leaves the tree, scripts worth keeping move to `tools/`, and `_work/TO_PROJECT_SPEC.md`'s subsections for the phase are corrected to what shipped. `site/scripts/build-notes.mjs` renders `_work/DECISIONS_10.md` … `_15.md` while in flight (its loop already runs to 15).
* **Documentation timing:** guide fixes ship to `main` at any time. Pages for the new medium, the new operators, the new voices and the sampler's coverage are drafted in the step's evidence folder and move into `site/` at step 92 — the live demos point at the released wasm.
* **Pre-release tags** end each phase (`v3.0.0-alpha.N`; the spine accepts any `X.Y.Z-pre`, so Phase 11's proposed `3.0.0-alpha.kessho` passes too — the author's naming); the author installs the build on every device and plays it. Phase 16 uses `v3.0.0-rc.N`. Nothing reaches a stable channel before step 92.
* Credentials: Phases 11–15 need none beyond the machines; Phase 16 reuses the release set. Author inputs are listed per step so they can be staged before the session.

---

## The ABI block — resolved against the headers (DECISIONS_10 #1)
The proposals' identifiers, corrected to what the tree holds; the final names are each step's to settle (`[ITERATE]`, as the proposal says).

```c
/* sumi_core.h — libsumi 1.6.0, additive (step 71 declares, 72–74 fill) */
enum { SUMI_MEDIUM_SUMI = 0, SUMI_MEDIUM_ANOD = 1, SUMI_MEDIUM_KESSHO = 2 };     /* sumi_params_t.medium */

/* sumi_ctl_t: 0–19 are taken (SUMI_CTL_TORSION_K 14 … SUMI_CTL_CHIRIKOV_K 19, SUMI_CTL_COUNT 20) — the proposal's 14–17 collide */
enum {
    SUMI_CTL_POLARIZER_ANGLE = 20,  /* alpha in [0, pi/2): the brush sin^2(2(psi - alpha)) has period pi/2 */
    SUMI_CTL_CAUSTIC_DEPTH   = 21,  /* z in [1.05, 8], log-spaced; z = 1 only through --kessho-flat        */
    SUMI_CTL_BRAID_COUPLE    = 22,  /* gain on the non-bass circulations Gamma_k                           */
    SUMI_CTL_ARNOLD_K        = 23,  /* kick strength, clamped to K_max = 0.8 q / max W'                     */
    SUMI_CTL_COUNT           = 24
};

/* the mode mechanism of §10.5 (SUMI_MODE_MEDIUM_DEFAULT = 255 resolves to the medium's column); two fields appended to sumi_params_t */
enum { SUMI_POLARIZER_GLOBAL = 0, SUMI_POLARIZER_PER_VOICE = 1 };   /* params.polarizer_mode — the A/B K-1 */
enum { SUMI_BRAID_PINNED     = 0, SUMI_BRAID_FREE          = 1 };   /* params.braid_mode     — the A/B B-1 */
/* press_mode 3 = Arnold K; bend_mode 5 = braid consonance; slide_mode 3 = polarizer alpha — the next free value of each */

/* the Arnold gesture route; the MIDI route derives Omega and q from the normalizer */
SUMI_API void sumi_add_arnold(sumi_instance_t* inst, float x, float y, float radius,
                              float winding_omega, float k, uint32_t resonance_q, float twist);
/* sumi_add_braid: DROPPED — the braid is the Strang composition of per-voice Lamb–Oseen passes; sumi_add_vortex composes. */

/* exports: the flags word of sumi_export_begin (SUMI_EXPORT_ANOD_ALPHA = 1u is a bit) */
#define SUMI_EXPORT_KESSHO_CAUSTIC 2u   /* Layer B alone over alpha; a Kessho export without it is the full composite */

/* trace/include/sumi_trace.h (the file is sumi_trace.h, not trace.h): a field of sumi_trace_config_t */
enum { SUMI_TRACE_CONTINUOUS = 0 /* v2.0: curvature-decimated segments */, SUMI_TRACE_POINCARE = 1 /* v3.0: the strobe */ };

/* voxo.h — in order of landing: voice_kind 9 the membrane (77), 10 the Toda ring (81), 11 the Boris gyroscope (82),
   12 the Dirac network and 13 the billiard (83); VOXO_CTL_CHORUS_MIX 1006, _RATE 1007, _DEPTH 1008 (86);
   voxo_suzu_params_t gains bow_mode (78), the editable tables (80), the mod matrix (84). */
```
Dev flags (`midi-sink --dev`): `--kessho-flat` (force z = 1, print max |det J − 1| over the canvas), `--soak arnold|braid|chord_wave|poincare`, `--ab polarizer|braid` (render a replay in both modes, archive side by side).

```c
/* KESSHO §4 (the minerals delta, folded 2026-10-14): THE MINERAL — libsumi 1.7.0, step 71 amended / 71b */
/* KESSHO §4.2 — the optical descriptor: a SIZED struct (size = sizeof; additive by size, the editor edits it) */
typedef struct sumi_mineral_t sumi_mineral_t;   /* fields as KESSHO §4.2 */
enum { SUMI_OPTIC_ISOTROPIC = 0, SUMI_OPTIC_UNIAXIAL = 1, SUMI_OPTIC_BIAXIAL = 2 };
SUMI_API void sumi_set_mineral(sumi_instance_t* inst, const sumi_mineral_t* m);      /* a pure composite change */
SUMI_API void sumi_get_mineral(sumi_instance_t* inst, sumi_mineral_t* out);          /* as stored (the fold adds it: a shell needs it, as sumi_get_palette) */
SUMI_API bool sumi_mineral_preset(uint32_t id, sumi_mineral_t* out, const char** name); /* fills size and every field; false past the end */
enum { SUMI_MINERAL_QUARTZ = 0, SUMI_MINERAL_CALCITE = 1, SUMI_MINERAL_MUSCOVITE = 2,
       SUMI_MINERAL_BARITE = 3, SUMI_MINERAL_FLUORITE = 4, SUMI_MINERAL_SELENITE = 5, SUMI_MINERAL_COUNT = 6 };

/* KESSHO §4.2 — the mechanical half: four fields APPENDED to sumi_params_t (at its end — the Step-33 pattern; the
   delta said "after braid_mode", which the kessho_* knobs of 1.6.0 already follow), read at strike / stroke */
enum { SUMI_HABIT_CIRCLE = 0, SUMI_HABIT_RHOMB = 1, SUMI_HABIT_HEX = 2, SUMI_HABIT_ROSETTE = 3 };   /* params.drop_habit  */
enum { SUMI_TINE_SMOOTH = 0, SUMI_TINE_TWIN_KINK = 1, SUMI_TINE_TERRACE = 2 };                     /* params.tine_profile */
/* params.habit_roundness (float, 0 sharp .. 1 circle), params.habit_blades (uint32_t, rosette m) */
/* INVARIANT: drop_habit 0 and tine_profile 0 are BITWISE the v2 drop and the v2 tine (field_512_metal.bin) */

/* presets/SCHEMA.md: a Kessho preset names a mineral id or carries a full sumi_mineral_t, plus the two habit fields */
```
Dev flags, add: `--mineral <id>`, `--soak crystal_drop|twin_tine|terrace_tine`, `--habit-corner` (roundness 0, prints the crease residual).

### The Kessho default binding table (extends SPEC §10.5)

| Dimension | Sumi default | Anod default | **Kessho default (v3)** |
|---|---|---|---|
| strike (note-on) | the drop | the spark charge + the shear episode | a refractive lens drop + an initial caustic ring |
| press (0xD0) | boundary growth (`press_mode` 0) | the torsion feed (2) | **Arnold K (`press_mode` 3)** — Ω and q from pitch against the bass |
| per-note bend | glide tine / ripple | the Chladni stir (`bend_mode` 4) | **braid consonance (`bend_mode` 5)** — bends move C_k, flipping saddle ↔ dipole |
| CC 74 (slide) | aux hue / pinch | the spark's frequency (`slide_mode` 2) | **polarizer α (`slide_mode` 3)** — global or per voice, per A/B K-1 |
| poly pressure (0xA0) | the Lamb–Oseen swirl | torsion and spark wavenumbers | **chord-wave amplitude A_k** |
| mod wheel / breath | the vortex | the Chirikov throw | **caustic depth z** — scans the iso-trace fold through the hyperbolic regime |
| sustain (CC 64) | paper dip | paper dip | paper dip (unchanged) |

---

# Phase 11 — Kessho and symplectic harmony (steps 71, 71b, 72–75)
**`_work/DECISIONS_10.md` is open** (opened at the fold, 2026-10-14; its #1 is the fold, its #2 the order). Spec: `KESSHO §1–§4` (§4 = `_work/specs/KESSHO_SPEC_delta_minerals.md`, folded 2026-10-14 with `ROADMAP_6_delta_minerals.md`), `TO_PROJECT_SPEC` §10.7–§10.9; SPEC §4.5, §5.3, §10.1–§10.5, §13.2 (the trace). **The core reopens for feature work: libsumi 1.6.0 (step 71), 1.7.0 (71b).** Order rationale: the microscope before the specimens — Kessho's z = 1 flatness test is the det-J gate every operator of the phase is held to, and the medium in which the Arnold islands are seen to be elliptic. **71b must land before step 75's freeze or it waits for 4.0** (the sub-step precedent: 55b, 58b–c). **Author input:** the Michel-Lévy reference (a published chart's sampled colours, or the CIE integration's own output as the reference — the step says which), **the six mineral presets' exposures (`[ITERATE]`, by eye against the captures) and the rosette's blade count**, the two A/B viewings. The sentence "a reader of the same field" covers every mineral: the §10.1 switching gate runs across media *and* minerals.

## Step 71 — Medium 2: the Kessho composite (Mac)
* `SUMI_MEDIUM_KESSHO` in `composite.glsl`, a pure reader of the same (u − x, v − y, ink, aux) field. **Layer A:** ψ (the principal axis from the §10.4 stencil's J) and σ (Anod's, exactly) → the isoclinic brush sin²(2(ψ − α)) × the Michel-Lévy LUT (retardation Γσ → sRGB, a 1024-texel 1D texture from an offline spectral integration against CIE 1931; the three-channel sin² form only as the web's fallback). **Layer B:** the trace discriminant Δ = (tr J)² − 4 tints elliptic (cool) and hyperbolic (warm); the caustic I_B = 1/(|1 + z(1 − z)(tr J − 2)| + ε) added. The frosted calcite substrate, backlit, screen-locked; the composite order substrate → B tint → B caustic → A (multiplicative over the backlight) → the bloom shared with Anod. **A MINERAL REPLACES THE PALETTE** (`KESSHO §4`, the minerals delta of 2026-10-14 — open decision 9 resolved; the twelve LUTs of the first landing are retired). Layer A takes a `sumi_mineral_t`: retardation Γ (nm at σ = 1) and bias Γ₀ into the one physical Michel-Lévy LUT; the optic class picks the brush — uniaxial sin²(2(ψ − α)), biaxial the two-axis product |sin(ψ − α + V)·sin(ψ − α − V)| (a stylisation, documented as one), isotropic none; the optional laws — **walk-off** (a second ink/aux sample at x + δ·sin(2(ψ − α))·ê_ψ, blended by cos² : sin²; the caustic not doubled), **thin film** (Airy over the ink channel, three evaluations), **dispersion** (three caustic depths z_R < z_G < z_B: three offset iso-trace folds), **conical ring** (an 8-point ring kernel on the pre-image where |Δ| < ε, masked tight). The substrate is the mineral's **body**: `body_tint` as the backlight, frosted, screen-locked. **The bloom is gated to Layer B only** — the fringes never bloom (the step's own capture, the pink blow-out, is the red control). `sumi_set_mineral`, `sumi_mineral_preset`, the six presets (`KESSHO §4.6`) with Quartz = the medium as first captured; the settings panel lists minerals where the other media list palettes; the stylised charts survive only as `lut_id` ≥ 1. `SUMI_EXPORT_KESSHO_CAUSTIC`; CTL 20 (α) and 21 (z); `params.polarizer_mode` both ways (A: a global α from CTL 20 or the last-touched voice's slide; B: per-voice α_k from each held voice's slide, blended as (cos 4α_k, sin 4α_k) over the nearest ≤ 4 voices by inverse distance, the global α where no voice is near); `--kessho-flat`. **Layer A reads instantaneous σ until the strain buffer of step 90 lands and feeds it unchanged.**

**Gates:** (1) live switching Sumi ↔ Anod ↔ Kessho bitwise on the field (the §10.1 gate reused); (2) **flatness:** `--kessho-flat` over the v2 operator soak reports max |det J − 1| ≤ 1e-4 for every exact operator — **red:** a deliberately windowed kick (the first draft's Arnold form) reports > 1e-2; (3) **period:** the renders at α and α + π/2 bitwise identical; (4) the LUT against the reference at Γσ = 550, 1100 and 1650 nm within ΔE 2; (5) the grain screen-locked under a 120 BPM scroll (§4.5); (6) the print readback and the 8K caustic export match the screen's sRGB; (7) **A/B K-1:** `--ab polarizer` on one gesture script and one MIDI replay, the renders archived under `docs/evidence/step71/kessho_ab/`, the decision folded (the loser stays in the enum as a non-default mode unless it is a maintenance burden); **(8) mineral switching bitwise on the field** — every pair of the six presets, the §10.1 gate extended; **(9) selenite sign test:** a +γ and a −γ smooth tine render blue ↔ yellow swapped about first-order magenta, archived — **red:** Γ₀ = 0 renders them identical; **(10) fluorite = Layer B alone:** the Fluorite render bitwise equals the Quartz render with Layer A multiplied out; **(11) the dispersion fringe:** at `dispersion` = 0.3 the three folds are distinct, and at 0 bitwise one fold; **(12) walk-off collapse:** at ψ = α the Calcite ghost is bitwise the single sample.
**DONE when:** all twelve gates green with (2)'s and (9)'s reds; `field_512_metal.bin` bitwise; the ABI history says 1.6.0 / 1.7.0 and what they declare; the medium in the desktop's settings and in the scenes' medium toggle as value 2 (the web's `?medium=2`); the six presets in `presets/`, each with its exposure recorded as `[ITERATE]` until the author signs it; the schema row. Evidence for the amendment: `docs/evidence/step71/minerals/` — the six presets on one replay, the selenite pair, the dispersion pair.

## Step 71b — Crystal habits and cleavage profiles (`deform.glsl`) (Mac) — the minerals delta, 2026-10-14
**Spec:** `KESSHO §4.3`–`§4.4`; SPEC §4.3(1)–(2), §10.2. **Author input:** the rosette's blade count; the corner roundness by eye.

* **Class exact, both.** **The crystal drop:** `SUMI_DEFORM_DROP` takes the habit profile R(θ) — ρ_src = ρ − R(θ)²/2, θ_src = θ, the kick generated by V(θ) = R(θ)²/2, det J = 1 for any R(θ) > 0 at any amplitude (`KESSHO §4.3`); bijective on each ray; boundary growth (`press_mode` 0) by R → sR with s = √(1 + p) — the same map, no new code path; **the interior ink phase from r/R(θ)** so the rings are homothetic habits; the corner crease (R′ jumps ⟹ J jumps) kept, `habit_roundness` as a log-sum-exp soft-min `[ITERATE]` by eye, zero allowed. Habits: rhomb (78.1° / 101.9°, the polar support form), hex, rosette r₀(1 + β cos^{2p}(mθ − θ₀)) with m = `habit_blades` (the first draft's Σ_j cos^{2p}(m(θ − θ_j)) corrected). The Kessho strike (71's lens drop) inherits the habit. **The cleavage profiles:** the tine's F(y) becomes selectable — smooth (Jaffer, bitwise v2), twin-kink (a piecewise-linear bump: two kink lines, the shear inside the lamella), terrace (A·stair_N(y)·w(y), N flat terraces) — **every profile a bump, zero outside |y| < H** (the first draft's monotone ramp and staircase pull texels from off-canvas: exact, but they fabricate). The glide tine and the ripple inherit the profile; no new operator, no new gesture entry. Habit and profile are read from `sumi_params_t` at strike / stroke time; a held drop keeps the habit it was struck with.

**Gates:** (1) the four §10.2 gates under `--soak crystal_drop` (every habit, roundness 0 and 0.5), `--soak twin_tine`, `--soak terrace_tine`; (2) flatness under all three soaks ≤ 1e-4 — the analytic det is also a unit test on the R(θ) family (`core/tests/habit_det.cpp`: finite-difference det − 1 < 1e-6 at 10⁴ random (θ, roundness)); (3) **red (the fabrication bug):** the unwindowed ramp fails zero fabrication at the first soak, charted; (4) **bitwise:** habit 0 + profile 0 reproduce `field_512_metal.bin` and the v2 tine goldens; (5) **homothety:** a rhomb drop's interior rings are rhombs — the ring-boundary polygon fit residual < 0.5 texel; **red:** the interior phase from r gives circles (fit residual > 5 texels); (6) **growth:** a rhomb under `press_mode` 0 for 300 frames stays a rhomb (fit residual < 0.5 texel) with area ∝ (1 + p) within 1 %; (7) **the crease** at roundness 0 is a Layer A line in Kessho and absent at roundness 1, archived; (8) a Calcite drop + a twin-kink bend show stepped facets in Sumi *and* two lamella edges in Kessho, archived; (9) the pass count unchanged (a habit is a profile, not a pass).
**DONE when:** all nine green with (3) and (5)'s reds; `field_512_metal.bin` bitwise; the ABI history lists the four fields; the operator book's drop and tine pages gain a habit / profile section each, and a *Minerals* page carries Hamilton's line after Jaffer's. Evidence: `docs/evidence/step71b/` — one strike per habit in Sumi, Anod and Kessho; the crease pair; the growth sequence. *(The fold's note on gate (1): drops have no inverse pass — the soak's (b) pairs do not apply to the crystal drop; its (b) is the pass against the closed form, as the burst's and the spark's are; `DECISIONS_10` records what the soak measured.)*

## Step 72 — The Arnol'd tongue lock (`SUMI_DEFORM_ARNOLD`) (Mac)
* **Class exact.** Local action-angle coordinates about the drop centre (ρ = r²/2 so that dx∧dy = dρ∧dθ; θ); one iteration per frame = twist ∘ kick. **The kick,** from the generating function F₂ = θρ₁ + (K/q)·W(ρ₁)·cos qθ: ρ₁ = ρ + K·W(ρ₁)·sin qθ, θ₁ = θ + (K/q)·W′(ρ₁)·cos qθ, W a smooth window (1 to ρ_drop, C² decay to 0 by 1.5 ρ_drop, W′ bounded); the inverse lookup is one scalar Newton on θ (initial guess θ₁, three iterations), then ρ explicit; contraction needs |K W′/q| < 1 ⟹ K_max = 0.8·q/max W′, clamped in `sumi_set_params`. **The twist is the existing Lamb–Oseen swirl,** its circulation chosen so the angular advance at the ring radius is Ω₀ turns per frame with slope τ = dΩ/dρ·ρ_ring — **mandatory: with constant Ω the q petals grow without bound under pressure; islands of width ∝ sqrt(K/τ) need τ ≠ 0.** Ω₀ = (f_voice/f_bass) mod 1 from the normalizer (bend included; the bass itself Ω₀ = 0, q = 1); q the nearest continued-fraction convergent p/q with q ≤ 6 and |Ω₀ − p/q| < 1/(2q²), else q = 6; K crossfaded over 50 ms `[ITERATE]` on a q change so petals do not pop; off-lock the islands precess by δ = Ω₀ − p/q turns per frame — the beat in bass-period units (`[ITERATE]` a slow-motion factor if 60·δ turns/s flickers on wide intervals). CTL 23 (K), `press_mode` 3, `sumi_add_arnold`. The twin is the Suzu kicked rotor (the same (K, Ω) semantics; a Suzu circle-map voice is out of scope). The Jaffer lineage line for the twist (the Lamb–Oseen swirl is his extension) ships with the docs, literature check first.

**Gates:** (1) the four §10.2 gates; (2) flatness under `--soak arnold` ≤ 1e-4; (3) the Newton residual < 1e-6 in ≤ 3 iterations for K ≤ K_max — **red:** K = 1.2 K_max diverges, the clamp proven necessary; (4) 3:2 → q = 2 and 5:4 → q = 4 islands stationary over 600 frames (< 0.5 texel drift) under a sustained press; (5) a +10 cent bend → precession at δ turns per frame within 1 %; (6) **red (the twist bug):** τ = 0 → the petal radius grows monotonically, no islands; (7) no frame-to-frame Δ > 1 texel across a tongue boundary. The locked and the precessing renders archived in Kessho.

## Step 73 — Kirchhoff chord braiding (`SUMI_DEFORM_BRAID`) (Mac)
* **Class exact by construction.** The ink advected by the superposed stream function ψ(x) = Σ_k Γ_k ψ_LO(x − x_k; r_c) — never integrated directly: the existing exact Lamb–Oseen maps, one per voice with circulation Γ_k·Δt, composed in **Strang order** (half-steps 1…N, then N…1) — a second-order splitting of the superposed flow, every factor det 1, so the composition is det 1 exactly; the hyperbolic saddle and the cat's-eye separatrix emerge in the composed field; 15 voices = 30 passes, within the pass budget (the proposal says ≤ 64; the table in `sumi_core.h`, DECISIONS_5, is the number the gate reads). **Circulation from consonance:** C_k ∈ [−1, +1] against the bass from Sethares / Plomp–Levelt roughness over the first six partials (unison and octave +1, the minor second −1, the tritone ≈ 0) — **computed in `core/src/midi_normalizer.cpp` (the proposal's `midi_processor.cpp` does not exist)**; Γ_k = sign(C_k)·Γ_vel·(0.3 + 0.7|C_k|), `[ITERATE]` the curve and the floor; the bass takes Γ_vel positive; same-sign pairs braid (a co-rotating saddle between them), opposite-sign pairs jet (a dipole); `bend_mode` 5 moves C_k live, flipping the topology continuously; CTL 22 scales the non-bass circulations. **A/B B-1, `params.braid_mode`, both built:** A pinned — the vortex at the voice's layout cell, always; B free (Kirchhoff) — x_k integrated on the CPU under H = −(1/4π)·Σ_{j<k} Γ_jΓ_k ln(‖x_j − x_k‖² + r_c²), implicit midpoint (symplectic; three fixed-point iterations; N ≤ 15), note-on re-anchors at the cell, note-off removes the vortex over the release, a 5 % boundary clamp `[ITERATE]` (image vortices later); the layout invariant broken by design — consonant dyads orbit each other, dissonant dyads translate as a dipole and carry their ink; ~N² per frame on the render thread, zero on audio.

**Gates:** (1) 2N within the pass budget at 15 voices, 60 fps on the desktop tier; (2) flatness under `--soak braid` ≤ 1e-4 (exact by construction — gated anyway); (3) a 3:2 dyad → one hyperbolic point between the cells (Δ > 0 at the midpoint, Δ < 0 at both cores), the figure-eight separatrix visible in Kessho; (4) a minor-second dyad → the ink centroid displaced ⟂ to the key line ≥ 8 texels over 120 frames (the jet); (5) a bend from m2 to P5 flips (4) → (3) with no discontinuity > 1 texel per frame; (6) **free mode:** H, ΣΓ_k x_k and ΣΓ_k‖x_k‖² conserved within 1e-6 over 10 minutes; (7) **A/B B-1:** `--ab braid` renders archived under `docs/evidence/step73/braid_ab/`, the decision folded (free mode may survive as a non-default "dance" mode even if pinned wins).

## Step 74 — The chord-wave lattice (`SUMI_DEFORM_CHORD_WAVE`, route a) (Mac)
* **Class exact.** Per held voice k: the axis ê₁(k) at the pitch class's angle on the circle of fifths, the wavenumber ω_k by octave, coordinates (s₁, s₂) along (ê₁, ê₂); one ripple-tine per voice, s₁′ = s₁ + A_k·sin(ω_k s₂ + φ_k)·w(s₂ − s_{2,k}), **windowed in s₂ only** — a sinusoidal tine spanning the canvas along ê₁, exactly Jaffer's comb and v2's ripple; voices compose as separate passes: one note parallel ripples, two a lattice where the bands cross, three and more the crossing region; poly pressure → A_k; no gesture route (the ripple's serves). **Route (b)** — local rosettes by a generating-function shear windowed in both coordinates, a 2 × 2 Newton per note per texel, exact and compactly supported — the only route to the first draft's quasicrystalline 3/5/6-fold rosettes: **go or defer, decided after (a) has been seen on paper.**

**Gates:** (1) the §10.2 gates; (2) flatness under `--soak chord_wave`; (3) a single note → parallel ripples, a P5 dyad → the lattice in the band crossing, archived; (4) the route (b) decision recorded.

## Step 75 — The stroboscopic Poincaré section — the trace's second mode (Mac; Voxo and `trace/`, no core change)
* Voxo keeps a clean **reference phasor at the normalizer's bass frequency** (bend included) — not a zero-crossing detector on the rendered signal, which is ill-defined for a chain or a bore; on each wrap (sub-sample interpolated) it emits every traced voice's (x_j, y_j) into the existing trace ring — zero allocation, the audio thread bit-identical with the strobe on or off; a Duffing or rotor voice may substitute its own drive clock per voice (the Boris gyroscope adds its own when it lands at step 82). `sumi_trace_config_t` gains the mode (`SUMI_TRACE_POINCARE` = 1); on the canvas each return stamps a micro-drop (Sumi) or a micro-swirl (Anod, Kessho) at the voice's cell through the gesture ABI; on the `sumi_set_scope` overlay a dust point. 1:1 one stationary point; 3:2 a 2-point orbit; incommensurate a dotted KAM ring; a high-K rotor fractal dust. The tablets' *Synth trace* rows gain the mode in their own sub-steps.

**Gates:** (1) 1:1 → one point, < 0.5 texel drift; (2) 3:2 → two; (3) a +50 cent bass bend shifts the strobe period by the matching ratio (a byte-log assert); (4) the rotor sweep K = 0.3 → 2.5: a closed KAM ring → fractal dust, charted; (5) the audio thread zero-allocation and bit-identical with the strobe on and off (a long `voxo_fuzz` run).

Phase close: `v3.0.0-alpha.1` (the proposal's `3.0.0-alpha.kessho` passes the spine too — the author's naming); the fold of `DECISIONS_10` as Part X with the two A/B decisions; `TO_PROJECT_SPEC` §10.7–§10.9 corrected to what shipped. The core is frozen again from here to step 91 (71b lands inside this phase, before the freeze).

# Phase 12 — Suzu foundation and the §13.7 deferred physics (steps 76–80)
**Open `_work/DECISIONS_11.md`.** Spec: SPEC §13 and `TO_PROJECT_SPEC` §13.8–§13.12 (the proposals, corrected). The Suzu suite (`tests/voxo_suzu_tests.cpp`), the storm, the fuzz, the profile and the lab gates are the phase's instruments; every voice's class in the §13.1 table.

## Step 76 — The SoA SIMD engine and the ×4 half-band oversampler (Mac)
**Spec:** SPEC §13.2 (the chain, the lattice), §13.4; `TO_PROJECT_SPEC` §13.8. **Carried:** the chain's SIMD (DECISIONS_7 #13), the shears' aliasing (#9).

* The Verlet chain and the modal lattice refactored to structure-of-arrays SIMD (AVX2 on x86-64, NEON on arm64, SIMD128 on the web — one source, three lane widths behind one header); an optional ×4 polyphase half-band FIR oversampler for the fold shears. **A hard prerequisite of the Toda ring (81) and the billiard (83), not optional (gate delta):** soliton compression and collision impulses are broadband. The magic-circle frequency prewarp as a shared helper (`suzu_prewarp(omega, dt)`) for the Boris gyroscope (82).
* **The web gate's contract restated first** (working rule): the scalar path stays as the reference build the gate compares bit for bit, or the gate becomes a tolerance gate — recorded before the refactor.

**Gates:** 16 chains of 80 nodes below 6 % of the callback (the headroom gate's method: the best of three measured seconds, DECISIONS_9 #13); the SIMD output against the scalar reference within the declared tolerance on every lane width, bit for bit where the op order is the same; the oversampler's alias floor ≤ −60 dB above Nyquist/2 on the fold shear at full drive; the storm on the winds and the strings at 0 XRuns from the Release build; the strobe of step 75 bit-identical before and after.
**DONE when:** the suite, the storm, the fuzz and `suzu_web_gate.mjs` are green on the Mac, and the Tab and the iPad run the patches unchanged (the tablets' NEON); every v2 patch's profile figure unchanged within the tolerance.

## Step 77 — 2D symplectic FDTD membranes, plates and gongs (Mac)
**Spec:** SPEC §13.7 (the deferred item), §13.1; `TO_PROJECT_SPEC` §13.9. **Author input:** which drum and which gong the profiles are fitted against.

* `voice_kind` 9, the membrane: an N_x × N_y leapfrog grid (displacement u, momentum p) under H₀ = Σ ½p² + ½c²|∇_h u|² + ½κ²(Δ_h u)² — the drift–kick step is det J = 1 exactly; the declared conformal contraction p ×= γ₀ − γ₂Δ_h (air and thermoelastic loss); the biharmonic boundary DECLARED in the spec (clamped or simply supported — it changes the mode table; gate delta d). **Pitch lands on f₀ through c, continuous; (N_x, N_y) are chosen for the budget** — the proposal's "0.000 cent by scaling the grid" is dropped (gate delta a).
* **The gong (Berger / von Kármán):** H_gong = (α/4)(Σ|∇_h u|²)² ⟹ Δp = −Δt·α·E_stretch(u)·(−Δ_h u) — E_stretch a scalar computed once per sub-step in the Δ_h u pass, depending on u alone: an exact momentum shear, one multiply-add per node. **The CFL gate is evaluated at E_max** (c²_eff = c² + αE_stretch rises with amplitude), E_max bounded by the strike-energy cap, E_stretch ≤ 2H₀/c² (gate delta b).
* **Two topologies:** the per-voice SIMD mini-membrane (16 × 16 cap; 16 float columns in two AVX2 or four NEON registers, horizontal neighbours as in-register shifts) and the **shared bus plate** (64 × 64, one instance on the Voxo bus, every voice striking or bowing it at the normalized (x, y) of its layout cell — the canvas as a soundboard, under Kessho a plate whose strikes are seen; `sumi_layout_cells` gives a note's cell, `sumi_layout_probe` the touch). **The biharmonic CFL scales as h²:** at 2× oversampling the 64 × 64 plate admits κ ≤ κ(16 × 16)/16 — the bus is specified near-membrane (κ small) or tiered 32 × 32 on mobile (gate delta c).

**Gates:** CFL λ_2D = Δt·sqrt(8c²/h² + 64κ²/h⁴) ≤ 1.95 at E_max, the load-time rejection above 2; **red:** a strike at 1.2× the cap → λ_2D > 2 → divergence within 100 steps; the undamped grid's energy drift < 0.005 dB over 10 minutes; the bus plate under λ_2D ≤ 1.95 at its declared κ on the mobile tier; the profile figures for the drum and the gong; the membrane's strobe (step 75) charted.
**DONE when:** the suite's new gates green with their reds; the storm with the membrane patch at 0 XRuns; the plate audible under a chord on the desktop and on the tablets.

## Step 78 — The stick-slip (Helmholtz) bow and the strike profiles (Mac)
**Spec:** SPEC §13.2 (the breath bow), §13.7; `TO_PROJECT_SPEC` §13.10. **Carried:** the press as a sustained drive and the strike position from the layout's cell (DECISIONS_7 #12).

* `bow_mode` 1: the implicit hyperbolic friction curve φ(v_rel) = μ_d + (μ_s − μ_d)·v₀²/(v₀² + v_rel²), solved by Newton–Raphson in three iterations against the string's or the mode's impedance, bounded by the winds' power-limited port discipline (E_injected ≤ F_bow·v_bow·Δt — the ledger); the servo bow stays `bow_mode` 0.
* The layout cell's strike-position profile (the plucked string's sin(kπp)/k² from the cell; a struck bar's) and the press as a sustained drive.

**Gates:** the Helmholtz corner's sharpening in the spectrogram as the bow force rises; the energy held ≤ ∫F_bow·v_bow dt (the ledger, red when the bound is lifted); the profile figure.
**DONE when:** the suite green, the bowed patches re-profiled, the Tab and the iPad bowing under the finger.

## Step 79 — The acoustic bore v2: toneholes, two-mass lips and the brass shock (Mac)
**Spec:** SPEC §13.2 (the bore, the jet, the reed, the lips), §13.7; `TO_PROJECT_SPEC` §13.11. **Carried:** the reed's flow inertia (DECISIONS_7 #23), the sax's quasi-periodic A3 (#33).

* Toneholes as branch ports — shunt inertance and radiation resistance along the Webster–Yee grid — driven by the fingering CCs 110–112 and a register vent (pitch is bore length in v2); the valve upgraded to 2-DOF lips (shearing and swinging modes); the acoustic mass term L_r·dq/dt in the reed's channel solver.
* **The (γ+1)/2 convective steepening along the bore, DECLARED DISSIPATIVE (gate delta):** a Burgers shock must lose energy (the entropy condition) — an energy-conserving limiter rings; the flux limiter's loss is logged in the mouth-power ledger, where `k[9] ≤ k[8]` stands and now has a known positive term.

**Gates:** the sax A3's sidebands below −45 dB; the ledger under shock steepening; **red:** the limiter off → Gibbs ringing at the shock front in the spectrogram; the flute, the sax and the trumpet re-profiled.
**DONE when:** the suite green with the new reds; the storm on the winds; the tablets' winds unchanged under the finger (the press rule of DECISIONS_9 #16 holds).

## Step 80 — Editable modal tables, Scala and unison (Mac)
**Spec:** SPEC §13.7, §14.6; `TO_PROJECT_SPEC` §13.12. **Carried:** the editable ratio tables with the VCSL fits in mind (DECISIONS_9 #9), the unison preset (DECISIONS_7 #34).

* User-editable modal ratio and decay arrays in `voxo_suzu_params_t` and in the preset schema (`presets/SCHEMA.md`, its version bumped); a `.scl`/`.kbm` loader in Voxo — the sound follows the table for Suzu and the sampler alike. **The layouts' microtonal mapping (§14.6) is a core item and is NOT this step's** — it stays carried unless step 91 takes it (DECISIONS_10 #1). A unison mode-splitting patch.

**Gates:** a table round-trips through a preset byte for byte on every shell; a Scala file's cents reproduced within 0.1 cent at the oscillator; the unison patch's profile.
**DONE when:** the suite and the preset and replay gates green; the desktop's Sound section and the tablets' Sound pages expose the table and the file.

Phase close: `v3.0.0-alpha.2`; the fold of `DECISIONS_11` as Part XI; `TO_PROJECT_SPEC` §13.8–§13.12 corrected to what shipped.

# Phase 13 — The new symplectic voices (steps 81–84)
**Open `_work/DECISIONS_12.md`.** Spec: `TO_PROJECT_SPEC` §13.13–§13.14. Every voice in the §13.1 class table with its red control, its profile and its orbit in the trace — the Poincaré strobe of step 75 is the instrument, and each voice's strobe portrait is a gate of its own step.

## Step 81 — The Toda lattice soliton ring (`voice_kind` 10) (Mac)
* N nodes on a ring under the exponential potential H = Σ p²/2 + (a/b)·Σ(e^(−b(q_{n+1} − q_n)) + b(q_{n+1} − q_n) − 1), symplectic Verlet at ×4 oversampling (step 76 is the prerequisite); press and CC 74 → the nonlinearity b (from a marimba-like linear ring to self-focusing soliton pulse trains); **poly pressure → a momentum IMPULSE on note-on and on a swirl change, not a sustained parameter** — the ring conserves Σp (gate delta b).
* **Near-integrable, not integrable (gate delta a):** symplectic Verlet preserves a modified H, not the Lax invariants; the proposal's "never breaks into chaotic noise" is replaced by the shape gate.

**Gates:** the soliton's shape (width × amplitude) conserved within 2 % over 10 minutes at b_max; the lossless energy drift < 0.001 dB over 10 minutes; no alias blow-up at full compression (the ×4 path); the strobe portrait; the profile.

## Step 82 — The Boris–leapfrog magnetic gyroscope (`voice_kind` 11) (Mac)
* A particle on a 2D potential surface under a perpendicular field B(x₁, x₂): half potential kick → the exact Cayley rotation by θ_B = 2·atan(BΔt/2) → half kick; (x₁, x₂) straight to (L, R) — the quadrature-stereo carry-over of §13.2 answered — and into `trace/` (the spirograph); poly pressure → B; its own drive clock offered to the strobe of step 75.
* **Class: volume-preserving (det J = 1), not energy-exact once the potential kick is in** — a bounded oscillating error, like leapfrog's (gate delta a). **Both the spring and B prewarped:** k through the magic-circle prewarp, B_num = (2/Δt)·tan(ω_BΔt/2) (gate delta b).

**Gates:** the Zeeman doublet ω± = sqrt(ω₀² + (ω_B/2)²) ± ω_B/2 within 0.1 cent across B ∈ [0, 50] up to 2 kHz; **red:** the prewarp off → 1.2 cent at 2 kHz, charted; the energy's bounded oscillation over 10 minutes; the spirograph in the trace **verified in Kessho** (gate delta c — Phase 11 landed first); the profile.

## Step 83 — The port-Hamiltonian Dirac network (`voice_kind` 12) and the billiard (`voice_kind` 13) (Mac)
* **The Dirac network:** ẋ = (J − R)∇H, J skew-symmetric; with H = ½xᵀQx the Cayley / implicit-midpoint step is (I − Δt/2·(J − R)Q)⁻¹(I + Δt/2·(J − R)Q) — **Q-orthogonal (an isometry in the energy norm), not orthogonal** (gate delta b); a prepared-network patch routing a reed or bow exciter, a string, a rattle stop (the unilateral collision of the reed's lay) and a modal bridge or the plate, with no feedback runaway.
* **The billiard, 4D:** the state (q, v), free flight under a harmonic potential inside a convex boundary — the stadium, with the aspect on CC 74 and the straight-wall length L on the mod wheel — with exact specular reflection; **det J = +1 (Liouville) — the proposal's "det J = −1" was a slip; a 2D (x, p) cell with a wall would be an impact oscillator, a different voice** (gate delta a); the circle limit integrable by angular momentum (warm, wavefolded), the stadium ergodic (gritty, bounded by geometry); each collision a velocity jump at a sub-sample collision time at ×4 oversampling (step 76).

**Gates:** the passivity probe ≤ 1.000 measured in the Q-norm across arbitrary skew J; the billiard's aliasing ≤ −60 dB above Nyquist/2; the KAM-to-ergodic spectrum sweep over L, charted, and its strobe from a closed ring to dust; the profiles.

## Step 84 — The per-voice chaotic modulator and the 4-slot mod matrix (Mac)
* The projected double pendulum (DECISIONS_7 #16, one per patch) promoted to per voice; `mod_target` grown to a 4-slot matrix (source × target × depth) in `voxo_suzu_params_t` and the preset schema. No change from the proposal (gate delta).

**Gates:** the suite's modulator gates per voice; the storm at 15 voices with the matrix full; presets round-trip.

Phase close: `v3.0.0-alpha.3`; the fold of `DECISIONS_12` as Part XII; the lab's panels for the four voices drafted in evidence (they land at step 92).

# Phase 14 — Voxo DS 2.0: the Decent Sampler coverage, second generation (steps 85–89)
**Open `_work/DECISIONS_13.md`.** Spec: SPEC §12.2–§12.3, `voxo/COMPAT_REPORT.md` (the ledger of what is not played); `TO_PROJECT_SPEC` §12.7–§12.12. Scope and gates as proposed (the gate delta changes nothing here); the Pianobook libraries the author keeps are the acceptance material. **Author input:** three libraries that exercise the pedal (a piano, a Rhodes, a dulcimer), one with chorus, one with a filter envelope.

## Step 85 — Sustain pedal v2, choke groups and pedal noises (Mac)
* `<group trigger="cc">` with `onLoCC`/`onHiCC` evaluated on CC 64 transitions (the thump and the damper lift); **half-pedalling:** a note released at 0 < CC 64 < 127 enters a damped-sustain release whose rate interpolates between the preset's release (pedal up) and `sustainDecay` (down); **re-pedalling** catches a decaying tail at its current level and slows it; `silencedByTags` with `silencingMode` normal (trigger the release) / fast (a 5 ms linear fade), and group-level `seqMode` gated by CC 64.

**Gates:** a headless bounce of pedal-down, half-pedal, re-pedal and a hi-hat choke, each asserted on its envelope; zero audio-thread allocations (the fuzz); the compat report's sentences for these features retired.

## Step 86 — Chorus and phaser, and the CC-map targets (Mac)
* `<effect type="chorus">` at bus and group level: a stereo quadrature-LFO delay line pre-allocated at load (mix, modDepth, modRate, the 90° stereo offset), 4-point Hermite reads (the pitch interpolator reused); `<effect type="phaser">`: 4 or 6 cascaded all-passes under a stereo LFO (mix, modDepth, modRate, feedback, centerFrequency); `VOXO_CTL_CHORUS_MIX` 1006, `_RATE` 1007, `_DEPTH` 1008 in `presets/SCHEMA.md`.

**Gates:** the effects bit-identical across the native builds at a fixed seed; the storm with chorus on every voice at 0 XRuns; the report's chorus sentence retired.

## Step 87 — Multi-mode filters, filter envelopes, the bus EQ and the waveshaper (Mac)
* The per-voice SVF's highpass, bandpass, notch, peak and lowpass_1pl outputs (the same loop computes them — zero extra multiplies for the SVF's own outputs); `<modulators><envelope>` to cutoff and resonance; `<effect type="eq">` (low shelf, peaking mid, high shelf), `<effect type="gain">`, `<effect type="wave_shaper">` (drive, boost, outputLevel).

**Gates:** each filter's response against its analytic curve within 0.5 dB; the report's "other filters" sentence retired.

## Step 88 — The full `<modulators>` matrix, legato and glide, the compat report updated (Mac)
* `<lfo>` (frequency, waveform sine|triangle|square|saw, offset, phase, sync free|tempo from the host's BPM — SPEC §3.4); multi-target `<binding>` modulation of amp, pan, pitch, cutoff and effect parameters; group legato / monophonic mode with `glideTime` and `glideMode` (SPEC §2.4's single-channel keyboards); `voxo/COMPAT_REPORT.md`'s canonical sentence table pruned to the genuinely out-of-scope (convolution, note sequences, streaming, the custom UI) — the report's test asserts the new table.

**Gates:** the preset suite's goldens for every modulator; a long `voxo_fuzz` run (its `--seconds` knob — the proposal's "25 M iterations" is not the suite's unit) clean; the compat test green.

## Step 89 — The preset's instrument slot, the offline bounce and the audio device picker (Mac; the tablets consume)
* The §16 sound carry-overs closed: a library referenced by name and hash in a preset (never embedded), the offline bounce of a replay (`--replay-wav` productized on every shell — Voxo deviceless, the `none` backend), the desktop's output-device row (miniaudio's enumeration, the name in the INI, "System default" first and the fallback when the name is gone — DECISIONS_6 #42).

**Gates:** a preset with a missing library loads with the sentence, not a crash; the bounce byte-identical to the live render at the same seed on the desktop; the device switch without a stop.

Phase close: `v3.0.0-alpha.4`; the fold of `DECISIONS_13` as Part XIII.

# Phase 15 — Visual, instrument and quality-of-life carry-overs (steps 90–91)
**Open `_work/DECISIONS_14.md`.** Spec: SPEC §10.4, §14.6, §15.2, §16; `TO_PROJECT_SPEC` §10.10, §14.7, §15.4.

## Step 90 — Anod's deterministic seed, the strain buffer and the Chirikov grid (Mac — the proposal said the Linux box; see the working rules)
* DECISIONS_7 #25: the gesture route reproducible run to run (the seed carried through the gesture ABI); the §10.4 long-exposure strain buffer (σ accumulated; it feeds Anod's glow by its own question and Kessho's Layer A unchanged — step 71 reads instantaneous σ until now); the Chirikov feel grid (DECISIONS_5 #89 — the author's "the vibrations are tiny and the displacement is huge; I was hoping for the contrary").

**Gates:** an identical gesture script → bit-identical Anod AND Kessho fields across runs; the `--replay` gates hold on the desktop and mobile tiers; the author's choice from the grid recorded.

## Step 91 — The Boehm woodwind layout, the split surface and the strip's preset-next (Mac — the proposal said the Windows box; the Windows lane verifies)
* `SUMI_LAYOUT_BOEHM` = 13 through `sumi_layout_state_t.buttons` (libsumi additive — a minor bump, the arc's second and last core reopening; the toneholes of step 79 are what it drives); the two-handed split surface (§14.6: a layout per screen half); the strip's preset-next pad (§15.2 — which presets cycle: the user-chosen subset, as the layout quick-switch does). `[ITERATE]` whether the layouts' microtonal mapping (§14.6) rides here with step 80's Scala tables.

**Gates:** `normalizer_tests` and `hostmpe_tests` goldens for every Boehm fingering and the split-zone probes; the tablets' panels (the iPad's and the Tab's in their own sub-steps, one platform each).

Phase close: `v3.0.0-alpha.5`; the fold of `DECISIONS_14` as Part XIV.

# Phase 16 — The web lab, the books and the 3.0 release (step 92)
**Open `_work/DECISIONS_15.md`.**

## Step 92 — Kessho on the web, the Suzu lab v2, the books, 3.0.0 (all machines, docs first; the step splits if the session runs long)
* The marble: `?medium=2` on every scene (the toggle is numeric — DECISIONS_9 #6), `?mineral=<id>` on the Kessho scenes, the scenes `arnold`, `braid` and `chord_wave` (`web/site/scenes.js`; `check.mjs`'s list grows to twenty), the Poincaré panel in the scope demo; the lab: panels for the membrane, the Toda ring, the Boris gyroscope, the billiard and the Dirac network (`suzu_lab_gate.mjs`); the books: Kessho and the harmony operators in the operator book (the lineage line for the twist, literature-checked), **a Minerals page** (the six presets with the captures, the crystal-drop theorem with its one-line proof, the cleavage profiles, the selenite sign test as a reader's exercise; Hamilton's line after Jaffer's), the drop and tine pages' habit / profile sections, the new voices in the Suzu book with their profiles and strobes, the sampler's coverage in Voxo's guide and the compat page; the MIDI implementation chart's rows for CTL 20–23 and the three modes against fresh byte logs (minerals are not a dimension); the settings reference with its mineral panel. Then the release: `v3.0.0-rc.N` on both tracks, the wave's questions, the final RC promoted with zero code changes — `v3.0.0`; the fold: every in-flight decisions file merged (Parts X–XV), this file → `docs/ROADMAP.md` Part 10, `TO_PROJECT_SPEC` → `docs/PROJECT_SPEC.md` (spec v6) by the author, evidence out of the tree.

**Gates:** `check.mjs` green with the new scenes embedded; `suzu_lab_gate.mjs` green; the wasm flatness demo ≤ 1e-3 (the fp16 web tier); `chart_check.py` green; the docs' URLs resolve before the lanes run.
**DONE when:** all five platforms install 3.0.0 through their normal channels; the site documents 3.0 with every demo live at `/marble/`; the fold is committed.

---

## Deferred (explicitly out of the v3.0 arc — do not implement)
Voxo Dorean (SPEC §12.6); convolution reverb, note sequences, disk streaming and the custom UI of Decent Sampler; the chord wave's route (b) unless step 74 says go; image vortices for the free braid; a Suzu circle-map voice (the Arnold twin's audio side); Play mode on the web and the sampler on the web; localization; auto-update; telemetry (never); Microsoft Store; Flatpak beyond the Phase-5 verdict; field undo; the libsumi SDK unless the release step asks again.

---

## Open decisions / provisional resolutions (DO NOT FORGET)

1. **The order of the phases — RESOLVED 2026-10-14, the author's word** ("no particular order is preferred for me, so I will listen to you": harmony first, then the others in the proposals' order — `DECISIONS_10 #2`). This file's numbering is final: Phase 11 Kessho and harmony (71–75), 12 Suzu foundation (76–80), 13 the voices (81–84), 14 Voxo DS 2.0 (85–89), 15 the carry-overs (90–91), 16 the release (92). The release split of `road_map_medium_v3.md` §5.1 (3.0 = harmony alone) is not taken: both features ship in 3.0.
2. **A/B K-1** polarizer ownership — step 71, by viewing.
3. **A/B B-1** vortex kinematics — step 73, by viewing; the free vortices' boundary `[ITERATE]`.
4. **The chord wave's route (b)** — go or defer at step 74.
5. **The handedness observable** w = (J₂₁ − J₁₂)/2 as a Kessho hue chirality `[ITERATE]` — cheap, add if the Maltese crosses read ambiguous.
6. **The Arnold precession rate** — a slow-motion factor if δ flickers on wide intervals.
7. **The consonance curve** — Sethares over six partials provisional; the 0.3 floor `[ITERATE]`.
8. **The Jaffer lineage line** for the Arnold twist ships with the docs; literature check first, as for the burst page.
9. **The Kessho palettes as LUTs beside `sumi_palette_t`** — RESOLVED 2026-10-14 (the minerals delta): a mineral descriptor replaces the twelve LUTs; the library ships six and is open.
10. **The web gate's contract under SIMD** — step 76, recorded before the refactor.
11. **The layouts' microtonal mapping** (§14.6) — step 80 is Voxo's loader; the core side rides with 91 or stays carried.
12. **"Voxo 2.0" as a version** — the library's version follows its ABI (0.16.1 today); the author names the phase.
13. **The membrane's bus plate** "struck at the layout's cell" stays with the membrane (step 77): Kessho is there by then, so the soundboard is seen as well as heard from its first session.
14. **The decisions files** — one per phase: `DECISIONS_10` Phase 11 … `DECISIONS_15` Phase 16, merged as Parts X–XV; `build-notes.mjs`'s loop already runs to 15.
15. **Exposures** (the minerals delta): the per-preset exposure that maps physical Δn ratios onto the chart, `[ITERATE]` by eye against `docs/evidence/step71/minerals/`; the ratios themselves are physical and not editable.
16. **Roundness:** the soft-min radius law of the habit corners, `[ITERATE]` by eye at step 71b.
17. **The Suzu conical intersection:** deferred to Phase 13 as `[ITERATE]` (parametric-drive class; an inaudible flip; the trace's double cone its only payoff).
18. **A habit per note?** v3 reads one habit from the params; a per-note habit (pitch class → habit, or velocity → roundness) is a 4.0 question — recorded, not taken.
19. **The crystal-drop theorem and the exactness rule:** the rule of `KESSHO §2.1` gains its third family — *a kick generated by a function of the angle alone* — alongside twist-type and generating-function; the spec's §2.1 sentence is updated at the phase fold.

**DO NOT FORGET:** 71b before 75; Hamilton's line after Jaffer's on the Minerals page; `Mineral.md` and the two deltas stay in `_work/specs/` until the author removes them.

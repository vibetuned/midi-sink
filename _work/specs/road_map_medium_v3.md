# ROADMAP_6 — Phases 14–16 (revised, 2026-10-14)
**Replaces** the Phases 14–16 block of `road_map_medium.md`. Picks up after **Step 84** (end of Phase 13: Voxo 2.0). Companion spec: `medium_specs_v3.md`. Suzu gate amendments: `roadmap_voxo_gate_delta.md`.
Conventions unchanged: one step per session, one platform per step, core-first on the Mac + desktop harness, every gate proven red before trusted green, decision fold at phase end.

---

## 1. C-ABI & default-binding additions (`sumi_core.h`, `trace.h`) — libsumi 1.6.0

Additive only (§5.3). Names follow existing enum style; **[ITERATE]** final identifiers against the header.

```c
/* Medium selector (sumi_params_t.medium, §10.1) */
enum {
    SUMI_MEDIUM_SUMI   = 0,  /* 0th-order: pre-image (u,v) + phase rings on washi            */
    SUMI_MEDIUM_ANOD   = 1,  /* 1st-order: Frobenius strain sigma = lambda - 1/lambda         */
    SUMI_MEDIUM_KESSHO = 2   /* lambda, psi, cos(phi): birefringence + trace + caustic        */
};

/* Global control dimensions (extends sumi_ctl_t) */
enum {
    SUMI_CTL_POLARIZER_ANGLE = 14, /* alpha in [0, pi/2) -- period of sin^2(2(psi-alpha))     */
    SUMI_CTL_CAUSTIC_DEPTH   = 15, /* z in [1.05, 8], log-spaced; z = 1 only via --kessho-flat */
    SUMI_CTL_BRAID_COUPLE    = 16, /* gain on non-bass circulations Gamma_k                    */
    SUMI_CTL_ARNOLD_K        = 17  /* kick strength, clamped to K_max = 0.8 q / max W'         */
};

/* Mode dimensions (via the existing §10.5 mode mechanism) -- the two A/B items */
enum { SUMI_POLARIZER_GLOBAL = 0, SUMI_POLARIZER_PER_VOICE = 1 };  /* SUMI_MODE_POLARIZER */
enum { SUMI_BRAID_PINNED     = 0, SUMI_BRAID_FREE          = 1 };  /* SUMI_MODE_BRAID     */

/* Arnold gesture route. Omega/q/twist are explicit here; the MIDI route derives them
 * from the normalizer (Omega = (f_voice/f_bass) mod 1, q by continued fraction). */
SUMI_API void sumi_add_arnold(sumi_instance_t* inst, float x, float y, float radius,
                              float winding_omega, float k, uint32_t resonance_q, float twist);

/* sumi_add_braid: DROPPED. Braid is the Strang composition of per-voice Lamb-Oseen passes;
 * sumi_add_vortex already composes. Consonance weighting is MIDI-only (needs the normalizer). */

/* Exports */
enum { SUMI_EXPORT_KESSHO = /*next*/, SUMI_EXPORT_KESSHO_CAUSTIC = /*next+1*/ };

/* trace/include/trace.h */
typedef enum {
    SUMI_TRACE_CONTINUOUS = 0, /* v2.0 curvature-decimated phase-plane segments           */
    SUMI_TRACE_POINCARE   = 1  /* v3.0 strobe on the bass reference phasor (or drive clock) */
} sumi_trace_mode_t;
```

Dev flags (`midi-sink --dev`): `--kessho-flat` (force z = 1, print max |det J − 1|), `--soak arnold|braid|chord_wave|poincare`, `--ab polarizer|braid` (render the replay in both modes, archive side by side).

### Kessho default binding table (`SUMI_MODE_MEDIUM_DEFAULT = 255`, extends §10.5)

| Dimension | Sumi default | Anod default | **Kessho default (v3)** |
| :---- | :---- | :---- | :---- |
| Strike (Note-On) | drop | spark charge + shear episode | refractive lens drop + initial caustic ring |
| Press (0xD0) | boundary growth (press_mode 0) | torsion feed (press_mode 2) | **Arnold K (press_mode 3)** — Ω, q from pitch vs bass |
| Per-note bend | glide tine / ripple | Chladni stir (bend_mode 4) | **Braid consonance (bend_mode 5)** — bends move C_k, flipping saddle ↔ dipole |
| Slide (CC 74) | aux hue / pinch | spark frequency (slide_mode 2) | **Polarizer α (slide_mode 3)** — global or per-voice per A/B K-1 |
| Swirl (0xA0) | Lamb–Oseen swirl | torsion & spark wavenumbers | **Chord-wave amplitude A_k** |
| Mod wheel / breath | vortex agitation | Chirikov throw | **Caustic depth z** — scans the iso-trace fold through the hyperbolic regime |
| Sustain (CC 64) | paper dip | paper dip | paper dip (unchanged) |

---

## 2. Phase 14 — Kessho medium & symplectic harmony

Order rationale: **the microscope before the specimens.** Kessho's z = 1 flatness test is the det-J gate every operator in this phase is held to, and it is the medium in which Arnold's islands can be seen to be elliptic. The first draft had it last.

| Step | Title | Deliverables & DONE criteria |
| :---- | :---- | :---- |
| **85** | **Medium 2: Kessho composite** (Mac) | `SUMI_MEDIUM_KESSHO` in composite.glsl: Layer A (ψ, σ → isoclinic brush × Michel-Lévy LUT), Layer B (Δ tint + caustic `1 + z(1−z)(tr J − 2)`), calcite substrate, 12 LUT palettes, exports, CTL 14/15, `SUMI_MODE_POLARIZER` both modes, `--kessho-flat`. **Gates:** (1) live switching Sumi↔Anod↔Kessho bitwise on the field (§10.1 gate reused); (2) **flatness:** `--kessho-flat` over the v2 operator soak reports max \|det J − 1\| ≤ 1e-4 for every Exact operator; **red control:** a deliberately windowed kick (first-draft Arnold form) reports > 1e-2; (3) **period:** renders at α and α + π/2 bitwise identical; (4) LUT matches the reference chart at Γσ = 550, 1100, 1650 nm within ΔE 2; (5) grain screen-locked under 120 BPM scroll (§4.5); (6) print readback and 8K caustic export match screen sRGB; (7) **A/B K-1:** `--ab polarizer` renders archived in `evidence/kessho_ab/`, decision folded to DECISIONS_10. |
| **86** | **Arnol'd tongue lock (`SUMI_DEFORM_ARNOLD`)** (Mac) | Generating-function kick (scalar Newton on θ) ∘ Lamb–Oseen twist; q by continued fraction, K crossfade; CTL 17; press_mode 3; `sumi_add_arnold`. **Gates:** (1) the four §10.2 conservation gates (±k inversion, zero fabrication, erosion ≤ 2× glide tine); (2) **flatness** under `--soak arnold` ≤ 1e-4; (3) Newton residual < 1e-6 in ≤ 3 iterations for K ≤ K_max; **red:** K = 1.2 K_max diverges, confirming the clamp is necessary; (4) 3:2 → q = 2 and 5:4 → q = 4 islands stationary over 600 frames (< 0.5 texel drift) under sustained press; (5) +10 cent bend → precession at δ turns/frame within 1%; (6) **red (the twist bug):** τ = 0 → petal radius grows monotonically, no islands; (7) q crossfade: no frame-to-frame Δ > 1 texel when crossing a tongue boundary. Archive the locked and precessing renders in Kessho. |
| **87** | **Kirchhoff chord braiding (`SUMI_DEFORM_BRAID`)** (Mac) | Strang-ordered per-voice Lamb–Oseen passes with consonance-signed Γ_k; Sethares curve in `midi_processor.cpp`; CTL 16; bend_mode 5; `SUMI_MODE_BRAID` **both modes** (pinned; free with CPU implicit-midpoint N-vortex step, note-on re-anchor, release fade, 5% clamp). **Gates:** (1) pass count = 2N ≤ 64 at 15 MPE voices, 60 fps on the desktop tier; (2) **flatness** under `--soak braid` ≤ 1e-4 (exact by construction — gate it anyway); (3) 3:2 dyad → one hyperbolic point between the cells (Δ > 0 at the midpoint, Δ < 0 at both cores) and a figure-eight separatrix visible in Kessho; (4) minor-second dyad → ink centroid displacement ⟂ to the key line ≥ 8 texels over 120 frames (jet); (5) bend from m2 to P5 flips (4)→(3) with no discontinuity > 1 texel/frame; (6) **free mode:** H, Σ Γ_k x_k and Σ Γ_k‖x_k‖² conserved within 1e-6 over 10 min; (7) **A/B B-1:** `--ab braid` renders archived in `evidence/braid_ab/`, decision folded. |
| **88** | **Chord-wave lattice (`SUMI_DEFORM_CHORD_WAVE`, route a)** (Mac) | Per-voice ripple-tine on the circle-of-fifths axis, windowed ⟂ only; Swirl → A_k. **Gates:** (1) §10.2 conservation gates; (2) flatness under `--soak chord_wave`; (3) single note → parallel ripples; P5 dyad → lattice in the band crossing, archived; (4) route (b) prototype decision recorded (go / defer) after viewing (3). |
| **89** | **Stroboscopic Poincaré section (trace/ mode 2)** (Mac, Voxo side) | Reference phasor at the normalizer's bass frequency; per-voice drive-clock override; strobe → trace ring → canvas stamps / scope dust. **Gates:** (1) 1:1 → single point, < 0.5 texel drift; (2) 3:2 → 2-point orbit; (3) bass +50 cent bend shifts the strobe period by the matching ratio (byte-log assert); (4) rotor sweep K = 0.3 → 2.5: closed KAM ring → fractal dust, charted; (5) audio thread zero-allocation and bit-identical with strobe on/off (`voxo_fuzz` 25M pass). |

Phase close: **alpha tag `3.0.0-alpha.kessho`**, DECISIONS_10 fold including the two A/B decisions.

## 3. Phase 15 — Visual, instrument & QoL carry-overs

| Step | Title | Deliverables & DONE criteria |
| :---- | :---- | :---- |
| **90** | Anod deterministic seed, strain buffer, Chirikov grid (Linux box) | DECISIONS_7 #25 reproducibility; §10.4 long-exposure σ buffer (now also feeds Kessho Layer A); DECISIONS_5 #89 grid. **Gates:** identical gesture script → bit-identical Anod *and* Kessho fields across runs; `--replay` gates hold on desktop and mobile tiers. |
| **91** | Woodwind Boehm layout, split surface, strip Preset-Next (Windows box) | `SUMI_LAYOUT_BOEHM = 13` via `sumi_layout_state_t.buttons`; §14.6 two-handed split; §15.2 button. **Gates:** `normalizer_tests` and `hostmpe_tests` goldens pass for all Boehm fingerings and split-zone probes. |

## 4. Phase 16 — Web lab, docs & v3.0 release

| Step | Title | Deliverables & DONE criteria |
| :---- | :---- | :---- |
| **92** | Kessho web demos, Suzu Lab v2, docs, 3.0.0 (all machines, docs first) | `?medium=kessho`, `?scene=arnold|braid|chord_wave`, Poincaré panel in the scope demo; Suzu Lab v2 panels from Phase 11–12 live in the *Suzu* book, Kessho/harmony pages in the *operator* book (separate books, separate steps if the session runs long); MIDI implementation chart rows 14–17 and modes; release pipeline per RELEASING.md (promote the RC the testers held; store releases manual). **Gates:** `suzu_lab_gate.mjs` green; wasm flatness demo reports ≤ 1e-3 (fp16 web tier); docs URLs resolve before lanes run. |

---

## 5. Open decisions / provisional resolutions (DO NOT FORGET)

1. **Release split.** Phase 14 has no dependency on Phases 11–13. Proposal on the table: **3.0 = symplectic harmony** (Kessho, Arnold, Braid, Poincaré + the Phase 11 membrane/bus plate struck at canvas coordinates), with Voxo DS 2.0 and the Phase 12 voices as 3.x. Rationale: the thesis sentence; 21 single-session steps before a chord becomes geometry is a long wait. **Author to decide before Step 71.** This file is written to work in either order.
2. **A/B K-1 polarizer ownership** — decided at Step 85 by viewing.
3. **A/B B-1 vortex kinematics** — decided at Step 87 by viewing. Free-vortex boundary handling [ITERATE].
4. **Chord-wave route (b)** (local, generating-function, 2×2 Newton) — go/defer at Step 88.
5. **Handedness observable** w = (J₂₁ − J₁₂)/2 as a Kessho hue chirality [ITERATE] — cheap, add if the Maltese crosses read as ambiguous.
6. **Arnold precession rate** — slow-motion factor if δ turns/frame flickers on wide intervals.
7. **Consonance curve** — Sethares over 6 partials is the provisional choice; floor 0.3 in Γ_k [ITERATE].
8. Jaffer lineage line for the Arnold twist (Lamb–Oseen is his extension) ships with the docs step; literature check first, as for the burst page.

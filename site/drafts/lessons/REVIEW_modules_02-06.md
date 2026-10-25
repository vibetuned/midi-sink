# Modules 2–6 — review against the engine as shipped (2026-10-25)

Checked claim by claim against `voxo/src/suzu.h`, `core/src/shaders/deform.glsl`
and `composite.glsl`, `core/src/displacement.cpp`, `core/src/voice_mapper.cpp`,
the suites (`tests/voxo_suzu_tests.cpp`, `tests/normalizer_tests.cpp`), the
step-85/86 printouts and the decision log (Part XIII since today's fold). The
mathematics that is not an engine claim (KAM, Aubry–Mather, Nekhoroshev, the
Froeschlé map, the Rechester–White series) was read for internal consistency
only, not against sources. The five lab files were rendered in headless Chrome
(SwiftShader): all five draw with no console error; their code was not checked
against the engine (by Rule 1 of the module-1 notes they are teaching
re-implementations and never the engine).

Severity: **A** = says something about the engine that is not so; **B** = a
number, name or file that is wrong; **C** = a mathematical slip; **D** = a
wording to qualify.

## Everywhere

* **B — the decision log's address.** Every module cites `_work/DECISIONS_13.md`.
  Phase 14 was folded today: it is `docs/DECISIONS.md` Part XIII, cited as
  `DECISIONS_13 #n` (the number stays).
* **B — files that do not exist.** `tests/suzu_test.cpp` and
  `tests/physics_test.cpp` (modules 2, 4) → the synth suite is
  `tests/voxo_suzu_tests.cpp`; the free braid's invariants are tested in
  `tests/normalizer_tests.cpp`. `advect_fs`, `composite_fs`, `kessho_fs`,
  `sumi_invert_deform_point` (modules 2, 3) → the passes are `deform.glsl`'s
  per-operator `*_fs` (`drop_fs`, `vortex_fs`, `chladni_fs` …) and the reader is
  `composite.glsl`'s `kessho_col`; the exact inverse of a Chladni step is the
  negated shears in reversed order, gated by the soak, not a function.
* **D — line numbers.** Several citations are a few lines off already
  (`Cell::step` 96–104 → 121–125; `DoublePendulum` 1388 → 1400; `TodaRing`
  700 ✓; `Dirac` 909; `Billiard` 827; `Rotor` 1368 ✓; `Membrane` 1927 ✓). Cite
  structs, functions and gate numbers; the lines will not survive step 87.
* **A — "Layer A stores the displacement" (modules 2, 3).** Layer A is Kessho's
  birefringence layer. The field is the ping-pong displacement texture,
  RGBA16F by default (RGBA32F behind a desktop flag), and the Jacobian is read
  by central differences over texels (the short stencil for ψ, σ; ±3 texels at
  512 for tr J), not by `dFdx`/`dFdy`.

## Module 2 — Hamiltonian vector fields and symplectic integrators

1. **A — the Chladni listing is not the engine's code (§2.1.1).** `chladni_fs`
   applies ONE wave per pass: `P_src = P + ½ψ·weight·sin(s)·(wy, −wx)` with
   `s = w·(P − P₀)`; the two shears of ψ = ½Ψ[cos(u−v) + cos(u+v)] are two
   passes (stage 0, then 1), each exact. The `s1/u1/v1/s2/u2/v2` block and the
   "3.1e-8 round trip" are not in the tree. Keep the mathematics (the splitting
   is right), quote the shader's line, drop the number or source it.
2. **A/B — the braid (§2.1.2).** Right: Strang halves, 2N passes a frame, 30 at
   fifteen voices; the regularised Hamiltonian
   H = −Σ ΓjΓk ln(|xj − xk|² + R_j R_k)/4π (the regulariser is the product of
   the two cores, `rc2[j][k]`, not r_c²); the implicit midpoint by fixed-point
   iteration in double (`sumi_braid_step`). Wrong: the circulation is
   Γ_k = sign(C_k)·Γ_vel·(0.3 + 0.7|C_k|) with C_k the Sethares consonance
   against the bass and Γ_vel = 0.036 canvas²/s at full velocity — not "sign
   from pitch class mod 2"; free mode is `braid_mode` = `SUMI_BRAID_FREE` (the
   D key), not `braid_free = 1`; the fixed point stops at 1e-15 (≤ 16 rounds),
   not 1e-13; the test is a consonant triad for 72 000 steps at 1/120 s (ten
   minutes) with |dH/H| < 1e-6 and the impulses within 1e-9 — not "36 000
   frames at 60 Hz, parts in 10⁹".
3. **D — the wake (§2.1.3).** The ≤ a/4 sub-step (n = ⌈len/(tip/4)⌉) is the
   engine's; the "fold at d = a/2 from the rear stagnation point" is the
   lesson's own derivation — say so (the shader's comment says "area-preserving
   to first order per sub-step, hence the ≤ a/4 budget (measured)"). The
   Stokeslet is `wake_profile` 1; the header lists 0, the inviscid doublet,
   first — verify the default before calling the Stokeslet "the default
   viscous mode", and drop "ℓ/a ≥ 1.5 keeps |∇d| ≤ 0.25 d/a" unless it is
   measured somewhere.
4. **B — the Dirac gate (§2.2).** `tests/voxo_suzu_tests.cpp` gate 59: twelve
   random skew J (n 6..16, a step fifty times the engine's), the bound
   |gain_q − 1| < 1e-9 with R = 0, measured 2e-16; < 1 with R ≻ 0. The power
   iteration is 120 steps ✓, Gauss–Jordan with partial pivoting ✓. "dim x = 2m + 3
   is odd" — not verified against `chain()`; check before teaching it.
5. **C — Gromov's theorem (§2.3).** "symplecticity requires the oriented area
   projections onto every conjugate plane to sum to an invariant, known as
   Gromov's non-squeezing theorem" conflates two things: the sum of projected
   areas being invariant is just ω-preservation (linear algebra); Gromov's
   theorem is the obstruction to squeezing a ball into a thinner cylinder.
6. **C — RK4's determinant (§2.3 table and Problem 2.1).**
   (1 − ε²/2 + ε⁴/24)² + (ε − ε³/6)² = 1 − ε⁶/72 + ε⁸/576, not −ε⁶/36.
7. **C — the Störmer–Verlet shadow (§2.4).** With x drifted and y half-kicked
   the invariant is ½[(1 − ε²/4)x² + y²] (the factor sits on the DRIFTED
   variable). Problem 2.1 has it right for (q, p); §2.4's ½[x² + (1 − ε²/4)y²]
   has the factor on the wrong one.
8. **B — "+14 dBFS safety breaker" (§2.4, lab).** The bus breaker (step 75)
   cuts a NON-FINITE block — `breaker_trips`; there is no level threshold. The
   lab's worklet mutes at full scale on a blow-up and restarts the engine.
9. **A — "the `live_cap` red control in the Suzu Lab" (§2.4, lab, Problem 2.2).**
   There is no Toda lab page (the lab has cell, modal, strings, chaos, flute,
   winds). The ring's red is `toda_gate` 0 (+19 dB of energy in a second at C8,
   gate 52). The live-peak limiter that pumped +162 dB/s was a development
   finding (`DECISIONS_13 #1`, item 3), never a shipped control. The standalone
   lab may well offer one; say it is the lab's.
10. **B — `toda_exp`.** A degree-11 Taylor after Cody–Waite range reduction
    (10⁻¹⁴ relative), not a "5-term minimax"; the re-measured drift 0.000004 dB ✓.
11. **B — the stability bounds (§2.4 failure mode 3).** The chain's bound is
    s ≤ 1 (`string_cfl`; 1.05 the red); the ring's is k·cap ≤ 0.9 at the note
    plus the resolution rule √k·ĉ·b ≤ 0.15; f < 1.9 is the SVF's. "VerletString
    and TodaRing enforce λ ≤ 0.95, or ε ≤ 1.90" is none of these.
12. **B/D — the membrane (failure mode 4).** §13.9, not §13.12. The bars 1.95 and
    1.80 ✓; the ten-minute figure ✓ (−0.00016 dB on the 8 × 6 grid at 96 k). The
    record says only "measured: a swelling sheet at 1.95 diverged"; the Mathieu /
    Floquet-multiplier-at-−1 reading and "+90 dB" are the lesson's
    interpretation — present them as such.
13. ✓ Verified: the Toda step listing (lines 722–733, word for word), the knee
    r_c = −ln(cap/2)/b, the stiffness peak k·cap/2, the double state, the
    0.01 dB/min float drift, 0.000017 dB, 0.72 %, 0.26 %, Σp ~1e-14; the Duffing
    kick on the updated x; the double pendulum's quote ("E = −2 to +1.4 … NaN")
    and its projection within 9e-8; H's per-step change ε²/2 (x′² − y²) and the
    ellipse ratio √((2+ε)/(2−ε)).

## Module 3 — Darboux, Moser and generating functions

1. **A — Layer B is not Gaussian curvature (§3.1.2).** The shipped Layer B is
   the trace discriminant Δ = (tr J)² − 4 (elliptic cool, hyperbolic warm) and
   the caustic det(I + z∇d) = 1 + z(1 − z)(tr J − 2), its fold the iso-trace
   contour tr J = 2 + 1/(z(z − 1)); Barite's dispersion reads it at three
   depths. There is no heightfield h(x, y), no K = (h_xx h_yy − h_xy²)/(…)²,
   and the conical ring sits where |Δ| < 0.15 and σ > 0.05 — near the parabolic
   line of Δ, not of a curvature. Rewrite the paragraph on the shipped
   observables (module 1 §1.5 has them); the Riemannian-vs-symplectic contrast
   survives intact with C = JᵀJ for the metric side and tr J for the rotation.
2. **B — `kessho_fs`, "PROJECT_SPEC §16"** → `composite.glsl`'s `kessho_col`;
   Kessho is TO_PROJECT_SPEC §10.7 (PROJECT_SPEC §16 is "what carries").
3. **B — the rosette.** r₀(1 + β cos⁸ mθ) as written is the proposal's; the
   shader's is r₀(1 + 0.6 cos⁸(mθ))/(1 + 0.6·35/128), mixed toward the circle
   by the roundness (module 1 §1.4 has the table).
4. **A — "the same action πA²" and Problem 3.3(2).** `Cell::retune` keeps the
   x-AMPLITUDE A, not the action: the orbit I_ε = A²(1 − ε²/4) encloses
   πA²√(1 − ε²/4), which changes with ε. The matrix in 3.3(2),
   [[1/√(1−ε²/4), 0], [(ε/2)/√(1−ε²/4), 1]], has determinant 1/√(1 − ε²/4),
   not 1 — it is not a linear symplectomorphism (it is the conjugation that
   makes I_ε a circle, which is fine, but then say what it is). The plain
   retune's amplitude change is not a fixed ratio either: it is
   √(I_ε′(x, y)/(1 − ε′²/4))/A with the OLD state under the NEW ε, so it
   depends on the phase at the retune. Module 1's Problem 1.3(b) and the
   suite's numbers (0.097 dB re-based, 0.057 / 0.062 plain) are the safe ground.
5. **A — the Toda pickup (§3.4 "three more places", item 2).** Reversed. The
   Eulerian reading (ξ + q(ξ) = x) was the one REJECTED at step 83: a hard
   soliton's density spike put −43 dB of content in the top octave, which
   aliases. What ships reads the compression in the lattice's own coordinates,
   the pickup standing still against the mean drift. It is not "brightness
   added while the pitch stays locked".
6. ✓ Verified: `shear_period_ratio` / `shear_table_fill`, 17 points, 80 cycles
   at ε = 2 sin(π/256), "110 and 555 cents at full gain (measured)"; `Rotor`'s
   re-base onto f₀(1 + p/2π); the billiard's exact rotation, reflection,
   det J = +1 and the two-point polyBLAMP; the crystal drop as the translation
   in (ρ, θ).

## Module 4 — Integrable systems, action-angle, KAM

1. **D — the Toda ring is near-integrable, not integrable (§4.1.2).** The
   discrete ring's leapfrog conserves a modified H, not the Lax invariants
   (`DECISIONS_13 #1`, gate delta a: the shape gate replaced "never breaks into
   chaotic noise"). Keep Hénon–Flaschka for the continuous lattice and say the
   engine's ring is the discrete, near-integrable one — that is the point of
   the gate.
2. ✓ Verified: the billiard's strobe (24 of 576 cells on the circle — a line,
   a central kick being radial; 153 on the stadium; 66 393 collisions in 60 s
   lossless within 2.4e-12 dB) — gates 61–62 of `tests/voxo_suzu_tests.cpp`;
   the tongue rule (the nearest convergent with 2 ≤ q ≤ 6 inside
   |Ω₀ − p/q| < 1/(2q²), else six; q = 1 the integer ratio); the tempered
   fifth's islands turning once in six hundred frames and the major third six
   times faster; five Newton iterations; K_max = 0.8/max|W′|; the per-step cap
   K = 1.25 on the canvas; `lattice_kick` from the pre-update positions.
3. **D — "the Poincaré collision strobe (s, sin ψ)".** The engine's section is
   the strobe at the bass (the (x₁, x₂) pair sampled at the reference phasor);
   (s, sin ψ) is the lesson's own billiard-map coordinates — fine for the
   lesson, but not what the suite counts.

## Module 5 — Resonance overlap, cantori, diffusion

1. **C — the secondary resonance's width.** §5.1 takes Δp_{1/2} = K/2 and gets
   K ≈ 1.46 from 2√K + K/2 = π; Problem 5.1 states Δp_{1/2} = K/π, whose
   solution of 2√K + K/π = π is K ≈ 1.69, not 1.46. Pick one.
2. **D — "Greene's K_c (chirikov.mdx, suzu.h line 1365)".** The engine caps a
   canvas throw at K = 1.25 a step and the rotor's K at 2.5 on the wheel; I did
   not find K_c quoted in the operator page or the header — cite Greene, not
   the tree, unless the page has it.
3. ✓ Verified: `chirikov_fs` driven by wheel deltas, the K = 1.25 cap; `Rotor`
   one kick per period, the cell re-based onto f₀(1 + p/2π).
4. Not checked: the Rechester–White series' coefficients, ΔW's exponent
   3.011722, the accelerator-mode window — literature; the lab's 262 144
   particles and the histogram are the lab's own.

## Module 6 — High-dimensional dynamics and Arnold diffusion

1. **B — the braid's names and cap.** `braid_mode` FREE, not `braid_free`;
   `sumi_braid_step` takes n ≤ 16 (fifteen voices in practice). The reduction
   count n_eff = N − 2 and the N ≤ 3 integrability are literature (right).
2. **D — "preserves total energy to machine precision" (§6.1.2).** The gate:
   the bell, 8 modes, every decay zero, ten minutes, −0.0026 dB (the energy
   ledger); the lattice's partials within 0.08 cent at any admitted coupling.
   Quote it; "machine precision" is the Dirac network's claim (2e-16 in the
   Q-norm), not the lattice's.
3. ✓ Verified: the lattice kick y_k += (κε₀²/ε_k)(Lx)_k from the pre-update
   positions, MODES_MAX 16, the sheared rotation beside it.
4. The rest (the Arnold web, whiskered tori, Nekhoroshev's exponents, the FLI,
   the Froeschlé map's tangent matrix) is literature and internally consistent;
   the tangent map in Problem 6.3(2) is right.

## The labs

All five render headless without a console error (screenshots in the session
scratchpad). Module 2's lab describes "libsumi displacement re-basing in
RGBA32F" and a "+14 dBFS breaker" — the same two notes as above apply to the
lab text. If you want the labs checked the way module 1's was (a CPU readback
of the field against the z = 1 view, the integrator arithmetic against the
engine's in node), say which; it is an afternoon per lab.

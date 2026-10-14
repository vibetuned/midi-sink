# MEDIUM SPEC v3 — Kessho & Symplectic Harmony
**Status:** revised after review fold (2026-10-14). Supersedes `medium_specs.md`.
**Depends on:** §4.5 (screen-locked substrate), §5.3 (additive ABI), §10.1 (live medium switching), §10.2 (operator conservation gates), §10.4 (anti-staircase stencil), §13.2 (trace/), DECISIONS_7 #25.
**Lineage:** Jaffer closed-form maps (the Arnold twist reuses the Lamb–Oseen swirl, which is Jaffer's own extension — arXiv:1810.04646). Frocht photoelasticity; Kirchhoff–Helmholtz point vortices; Arnol'd circle map; Lie–Trotter/Strang splitting.

---

## 0. What changed vs. the first draft (read this first)

| Item | First draft | v3 | Why |
| :---- | :---- | :---- | :---- |
| Kessho retardation | separate quantity | **= Anod's σ exactly** | λ − λ⁻¹ = √(‖J‖²_F − 2) for det J = 1 |
| Caustic horizon | fold at \|tr J\| = 2 | fold on **tr J = 2 − 1/(z(1−z))**; flat at z = 1 | derived below; z = 1 is a free det-J gate |
| Polarizer range | 0..π | **0..π/2** | sin²(2(ψ−α)) has period π/2 in α |
| Polarizer ownership | global α | **A/B: global vs per-voice α_k** | author: test both |
| Arnold kick | ρ₁ = ρ − εKw(ρ) sin qθ, "Exact" | **generating-function kick + Lamb–Oseen twist**, Exact | window on ρ broke det J; constant Ω gives no islands |
| Chord wave | Gaussian-windowed shear, "Exact" | **ripple-tine (window ⟂ only)**, Exact; local form [ITERATE] | window on the sheared coordinate broke det J |
| Braid | pair Hamiltonian, Sub-stepped, det J > 0.5 gate | **Strang composition of exact Lamb–Oseen factors**, Exact; **A/B: pinned vs free vortices** | second H term moves vortices not ink; composition of det-1 maps is det 1 |
| `sumi_add_braid` | new ABI entry | **dropped** | redundant with per-voice vortex passes |
| Poincaré strobe | zero-crossings of bass fundamental | **reference phasor at the normalizer's bass frequency** | physical-model voices have no clean phase |

---

## 1. Kessho (`SUMI_MEDIUM_KESSHO = 2`)

### 1.1 What each medium reads of J ∈ SL(2,ℝ)

Polar-decompose the per-texel Jacobian of the stored inverse-lookup field, J = R_φ · U(ψ, λ), with U symmetric, det U = 1, principal stretches λ ≥ 1 ≥ λ⁻¹ along axis ψ.

| Observable | Formula (finite differences, §10.4 stencil) | Sumi | Anod | Kessho |
| :---- | :---- | :---- | :---- | :---- |
| pre-image (u,v) | sampled | ✓ | – | – |
| stretch λ (as σ = λ − λ⁻¹) | σ = √(‖J‖²_F − 2) | – | ✓ | ✓ (retardation) |
| principal axis ψ | ½ atan2( 2(J₁₁J₁₂ + J₂₁J₂₂), (J₁₁² + J₂₁²) − (J₁₂² + J₂₂²) ) | – | – | ✓ (isoclinics) |
| rotation cos φ | tr J = cos φ · (λ + λ⁻¹) | – | – | ✓ (trace discriminant, caustic) |
| handedness sign φ | w = (J₂₁ − J₁₂)/2 | – | – | **[ITERATE]** optional hue chirality |

Note on frames: J is the inverse map's Jacobian, so `JᵀJ` here is the forward map's *left* Cauchy–Green tensor — Eulerian, which is what a polarizer on the screen sees. tr J is conjugation-invariant and equal for forward and inverse maps when det J = 1, so the regime partition below does not depend on which map is stored.

**Honest headline.** Rigid rotation has C = I: ψ is undefined and σ = 0, so Kessho's birefringence layer is dark exactly where Anod is dark. The Maltese cross forms in the differential-rotation annulus of a vortex (where Anod already glows), not on the rigid core. What Kessho adds over Anod is **orientation (ψ)** and **rotation (tr J)**. The second is the only new observable for elliptic motion.

### 1.2 Layer A — Photoelastic birefringence

$$I_A(\Lambda) = \underbrace{\sin^2\big(2(\psi - \alpha)\big)}_{\text{isoclinic brush}} \cdot \underbrace{\mathrm{ML}_\Lambda(\Gamma\,\sigma)}_{\text{Michel-Lévy}}$$

* **Michel-Lévy as a 1D LUT.** Retardation Γσ → sRGB via a precomputed table (offline spectral integration against CIE 1931 curves, à la Sørensen 2013), 1024 texels, wrapped as a 1D texture. The three-channel `sin²(πΓσ/Λ_RGB)` form is the fallback for the web demo only. The **12 Kessho palettes = 12 LUTs** (1 physical chart + 11 stylised: smoked quartz, calcite, nematic, amber, …).
* **Polarizer angle α ∈ [0, π/2).** Period of the brush in α is π/2; the CC range maps to one quarter turn. Ownership is the **A/B item K-1** (§1.5).
* The §10.4 long-exposure strain buffer (ROADMAP step 90) accumulates σ and serves this layer unchanged.

### 1.3 Layer B — Trace discriminant & Lagrangian caustic

Trace discriminant Δ = (tr J)² − 4 partitions the canvas:

* **Elliptic, Δ < 0** (vortex cores, Chladni eddies): cool tint, calm.
* **Hyperbolic, Δ > 0** (pinches, wakes, Chirikov chaos): warm tint.
* **Parabolic, Δ = 0**: the order/chaos boundary.

Caustic intensity from refraction of the stored displacement d = (u−x, v−y) onto a bed at depth z. With det J = 1 the determinant collapses to one line:

$$\det\big(I + z\nabla d\big) = 1 + z(1-z)\,(\operatorname{tr}J - 2), \qquad I_B = \frac{1}{\big|\det(I + z\nabla d)\big| + \epsilon}$$

Consequences (these replace the first draft's "fold at |tr J| = 2"):

1. **z = 1 is flat.** det = 1 identically for any exact field. Any sub-stepped or mis-windowed operator shows up as texture. This is a **free det-J gate** and is exposed as `--dev --kessho-flat` (forces z = 1, prints max |det − 1| over the canvas).
2. **The fold is an iso-trace contour, not the horizon.** Zero at tr J = 2 − 1/(z(1−z)). For z ∈ (0,1) only tr J ≤ −2 folds (flip regime, rare on a marbling canvas; z = ½ hits exactly −2). For **z > 1** the fold sits on tr J = 2 + 1/(z(z−1)) and sweeps from +∞ toward 2⁺ as z → ∞. So the breath/mod-wheel **scans a level set of tr J through the hyperbolic regime** and approaches the parabolic horizon asymptotically.
3. **CC mapping.** `SUMI_CTL_CAUSTIC_DEPTH` → z ∈ [1.05, 8] (log-spaced); z = 1 reachable only through the dev flag. **[ITERATE]** upper bound by eye.

### 1.4 Substrate & composite

* Backlit frosted calcite micro-facets, **screen-locked** (§4.5 invariant, same grain test as Anod under 120 BPM scroll).
* Composite order: substrate → Layer B tint (elliptic/hyperbolic by Δ) → Layer B caustic (additive) → Layer A (multiplicative over a backlight) → bloom shared with Anod.
* Pure composite.glsl reader over the same (u−x, v−y, ink, aux) field; switching Sumi ↔ Anod ↔ Kessho leaves the field bitwise identical (§10.1 gate reused).
* Export: `SUMI_EXPORT_KESSHO_CAUSTIC` (8K alpha, Layer B only) and `SUMI_EXPORT_KESSHO` (full composite), both through the existing print readback path.

### 1.5 A/B item K-1 — Polarizer ownership (author: build both)

Both behind one mode dimension (`SUMI_MODE_POLARIZER`, see roadmap ABI block):

| | **A: Global α** | **B: Per-voice α_k** |
| :---- | :---- | :---- |
| Source | `SUMI_CTL_POLARIZER_ANGLE` (CC), or last-touched voice's Slide | each held voice's Slide (CC 74) → α_k |
| Field | constant | α(x) = angle-blend of the nearest ≤ 4 active voices, inverse-distance weights, blended as the vector (cos 4α_k, sin 4α_k) so the π/2 period wraps correctly; falls back to global α where no voice is near |
| Reads as | one crystal under one analyzer, whole-canvas rotation on a master gesture | each vortex owns its crystal; sliding one finger rotates one cross |
| Cost | 0 | one small uniform array (≤ 15 voices) + blend in composite |
| One-consumer rule | Slide stays with its Sumi/Anod owner | Slide → α_k is the Kessho `slide_mode 3` consumer |

**Decision gate (step 85):** same gesture script and MIDI replay rendered in both modes, renders archived under `evidence/kessho_ab/`, decision folded to DECISIONS_10. Loser stays in the enum as a non-default mode unless it is a maintenance burden.

---

## 2. Coupled multi-voice operators

### 2.1 The exactness rule (why three class labels changed)

A kick ρ₁ = ρ − ∂V/∂θ is det-1 only if V depends on θ alone; a shear u₁ = u + F(v) only if F ignores u. Any spatial window on the coordinate being kicked yields det J = 1 − (window slope)·(kick). Twist maps (rotation by an angle that depends on radius) are the one family that is both exact and compactly supported — which is why Lamb–Oseen, the Chladni disc and the tine already work. Therefore:

> **Exact + localised ⟹ twist-type, or generating-function (implicit, Newton in the shader).** Everything else is Sub-stepped and must say so.

Kessho's z = 1 flatness test (§1.3) is the runtime check for this rule and is a gate on every operator below.

### 2.2 `SUMI_DEFORM_ARNOLD` — Arnol'd tongue intonation lock

**Class: Exact.** Local action-angle coordinates about the drop centre: ρ = r²/2 (so dx∧dy = dρ∧dθ), θ.

One iteration per frame = **twist ∘ kick**:

1. **Kick (generating function F₂ = θρ₁ + (K/q)·W(ρ₁)·cos qθ):**
   $$\rho_1 = \rho + K\,W(\rho_1)\sin q\theta, \qquad \theta_1 = \theta + \tfrac{K}{q}\,W'(\rho_1)\cos q\theta$$
   W(ρ): smooth window, 1 for ρ ≤ ρ_drop, C² decay to 0 by 1.5 ρ_drop; W′ bounded. The inverse lookup is **one scalar Newton on θ** (initial guess θ₁, 3 iterations), then ρ explicit. Contraction requires |K W′/q| < 1 ⟹ K_max = 0.8·q/max W′, clamped in `sumi_set_params`.
2. **Twist = the existing Lamb–Oseen swirl**, circulation chosen so the angular advance at the ring radius is Ω(ρ_ring) = Ω₀ turns/frame, with slope τ = dΩ/dρ · ρ_ring. Ω₀ = (f_voice / f_bass) mod 1 from the normalizer (live, including bend); the bass voice itself gets Ω₀ = 0, q = 1.

Why the twist is mandatory: with constant Ω, M^q reduces to ρ ↦ ρ − qK W sin qθ — q petals appear but grow without bound under pressure. Islands of width Δρ ∝ √(K/τ) need τ ≠ 0.

* **q selection:** nearest continued-fraction convergent p/q of Ω₀ with q ≤ q_max = 6 and |Ω₀ − p/q| < 1/(2q²); outside every tongue, q = q_max. K crossfades over 50 ms (**[ITERATE]**) when q changes so petals don't pop.
* **Precession:** off-lock the islands precess by δ = Ω₀ − p/q turns per frame — the beat in bass-period units, scaled by frame-rate/f_bass. **[ITERATE]** a slow-motion factor if 60·δ turns/s reads as flicker on wide intervals.
* **MIDI binding (Kessho default):** Press (0xD0) → K; Ω₀, q from pitch. Gesture route: `sumi_add_arnold`.
* **Twin:** Suzu kicked rotor (§13.2). Same (K, Ω) semantics on purpose; a Suzu circle-map voice is out of scope for v3.

### 2.3 `SUMI_DEFORM_CHORD_WAVE` — chord standing-wave lattice

**Class: Exact (route a).** Per held voice k: axis ê₁(k) at the pitch-class angle on the circle of fifths, wavenumber ω_k ∝ octave; coordinates (s₁, s₂) along (ê₁, ê₂). One ripple-tine per voice:

$$s_1' = s_1 + A_k\,\sin(\omega_k s_2 + \phi_k)\; w(s_2 - s_{2,k})$$

windowed **only in s₂** — a sinusoidal tine that spans the canvas along ê₁, exactly like Jaffer's comb and v2's ripple. Voices compose as separate passes. One note: parallel ripples; two: a lattice where the bands cross; three+: the crossing region of three bands.

**[ITERATE] route (b) — local rosettes:** generating-function shear windowed in both coordinates, 2×2 Newton per note per texel. Exact and compactly supported; this is the only route to the quasicrystalline 3/5/6-fold rosettes in the first draft. Prototype only after route (a) has been seen on paper.

* **MIDI binding:** Swirl (0xA0, per note) → A_k. Gesture route: none in v3 (reuse ripple).

### 2.4 `SUMI_DEFORM_BRAID` — Kirchhoff chord braiding

**Class: Exact.** The ink is advected by the superposed stream function ψ(x) = Σ_k Γ_k ψ_LO(x − x_k; r_c). We never integrate that flow directly: composing the **existing exact Lamb–Oseen maps**, one per voice, with per-frame circulation Γ_k·Δt in **Strang order** (half-step 1…N, then N…1) is a second-order splitting of the superposed flow. Every factor is det 1, so the composition is det 1 exactly; the hyperbolic saddle and the cat's-eye separatrix emerge in the composed field. No sub-step Jacobian gate, no pair-merging: 15 voices = 30 passes ≤ 64.

**Circulation from consonance.** C_k = consonance of voice k against the bass on a continuous curve (Sethares/Plomp–Levelt roughness over the first 6 partials, mapped to C ∈ [−1, +1]: unison/octave → +1, minor second → −1, tritone ≈ 0). Then

$$\Gamma_k = \operatorname{sign}(C_k)\;\Gamma_{\text{vel}}\;(0.3 + 0.7\,|C_k|) \qquad\text{[ITERATE] curve and floor}$$

and the bass takes Γ_vel with positive sign. Same-sign pairs braid (co-rotating saddle between them); opposite-sign pairs jet (dipole). Bending an interval through the consonance curve flips the topology continuously; `SUMI_CTL_BRAID_COUPLE` scales the non-bass circulations.

**A/B item B-1 — vortex kinematics (author: build both)**, one mode dimension `SUMI_MODE_BRAID`:

| | **A: Pinned** | **B: Free (Kirchhoff)** |
| :---- | :---- | :---- |
| Vortex centres | the voice's layout cell, always | x_k integrated on CPU under the N-vortex Hamiltonian H = −(1/4π) Σ_{j<k} Γ_jΓ_k ln(‖x_j − x_k‖² + r_c²), implicit midpoint (symplectic; 3 fixed-point iterations; N ≤ 15 is trivial) |
| Visual | ink braids/jets between fixed keys | consonant dyads literally orbit each other; dissonant dyads translate as a dipole and carry their ink |
| Layout invariant | kept | **broken by design**: the vortex leaves the key; note-on re-anchors at the cell, note-off removes the vortex over the release |
| Boundary | n/a | **[ITERATE]** clamp with 5% margin for v3; image vortices later |
| Cost | 0 extra | ~N² per frame on the render thread; zero on audio |
| Conserved | – | H, Σ Γ_k x_k (centre of vorticity), Σ Γ_k ‖x_k‖² — all gated |

**Decision gate (step 87):** both modes implemented, same replay in both, renders archived under `evidence/braid_ab/`, decision folded. Free mode may survive as a non-default "dance" mode even if pinned wins.

* **MIDI binding (Kessho default):** bend_mode 5 — bends change C_k live; `SUMI_CTL_BRAID_COUPLE` on a global CC. No new gesture entry point: `sumi_add_vortex` already composes.

### 2.5 `SUMI_TRACE_POINCARE` — stroboscopic surface of section (trace/ mode 2)

Voxo keeps a clean **reference phasor** at the normalizer's bass frequency (bend included) — not a zero-crossing detector on the rendered signal, which is ill-defined for a Verlet chain or a bore. On each wrap (sub-sample interpolated) it emits the current (x_j, y_j) of every traced voice into the existing trace ring (zero-allocation, bit-identical audio thread). The drive clock of a Duffing/rotor/Boris voice may replace the bass phasor per voice.

On the canvas each return stamps a micro-drop (Sumi) or micro-swirl (Anod/Kessho) at the voice's cell; on the `sumi_set_scope` overlay, a dust point.

* 1:1 → one stationary point; 3:2 → 2-point orbit; incommensurate → dotted KAM ring; high-K rotor → fractal dust.

---

## 3. How the three media read the same performance (revised)

| Property | Sumi | Anod | Kessho |
| :---- | :---- | :---- | :---- |
| Reads of J = R_φ U(ψ,λ) | pre-image (u,v) | λ (as σ) | λ, ψ, cos φ |
| Lights up | pigment rings | stretching filaments | rotating Maltese crosses in the shear annulus, Michel-Lévy fringes, iso-trace caustic folds |
| Rigid rotation | spiral bands only via differential rotation | dark | **dark in Layer A; elliptic glow in Layer B** |
| Substrate | washi | black glass + phosphor | frosted calcite, backlit |
| Default chord gesture | drops + glide tines | spark shear + Chladni stir | **Braid + Arnold lock** |
| Built-in diagnostic | – | – | **z = 1 flatness = det J gate** |

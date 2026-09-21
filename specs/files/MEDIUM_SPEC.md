# MEDIUM SPECIFICATION: The Media System & the Anod Medium
**Phase 6, output side. Companions: `PROJECT_SPEC.md`, `INSTRUMENT_SPEC.md`, `QUALITY_OF_LIFE_SPEC.md`. Iteration expected — open points are marked `[ITERATE]`.**

---

## 1. The media abstraction

The engine was never fluid at heart — it is **symplectic**: bijective, det J = 1, inverse-lookup maps composed on a coordinate field. Smoothness was never required, only invertibility; suminagashi was the first *medium*, not the engine itself. Phase 6 makes that explicit.

**The engine owns** (shared by every medium): the ping-pong coordinate field and pass machinery, the operator library (ALL operators — media choose defaults, none are locked away), the MIDI normalizer and voice model, layouts and the play surface, hostmpe, the strip, transports, the dip/readback machinery, §4.6 orientation discipline, the budgets and the delta rule.

**A medium owns:** the composite pass (how the field is *read*), its palette family, its print styling (what a dip means visually), and the **default binding table** (which operator each MIDI dimension drives, under the existing one-consumer rules — bend_mode/slide_mode/press_mode gain medium-scoped defaults, user-overridable as today).

ABI: `sumi_params_t` gains `uint32_t medium` (0 = `SUMI_MEDIUM_SUMI`, 1 = `SUMI_MEDIUM_ANOD`). `[ITERATE: the medium's name — candidates: Anod; Biri (ビリ, from biribiri, the Japanese onomatopoeia for electric crackle — keeps sound-symbolic register with sumi); Ichifu (honoring Ichisuke Fujioka, the Meiji electrical pioneer). Whatever wins, the docs' acknowledgments can honor Fujioka regardless — an homage may sit better in the acknowledgments than carried by a product name.]` **This lands in the single Phase-6 ABI event**, batched with INSTRUMENT_SPEC's probe-state change — one break, not two; the deferred SDK waits behind it.

Medium switching: live-switchable; the field is medium-agnostic (coordinates + phase + aux), so switching reinterprets the same deformation history — a marbled sheet re-read as a discharge record. `[ITERATE: is live-switch a feature or should a dip be forced between media? The re-read is eerie and wonderful in prototype-imagination; it may be confusing in practice.]`

Field payload reinterpretation in Anod: `ink phase` reads as **charge phase** (filament banding), `aux` as per-event hue/energy. Same texels, different composite semantics — no field format change.

---

## 2. The Anod operators

Five additions to the shared operator library (all media may use them; Anod binds them by default). Corrected math relative to the draft — the corrections are part of the spec.

**Operator classes — every operator, present and future, declares one:**

| Class | Definition | Members | Discipline |
|---|---|---|---|
| **Exact** | det J = 1 at any magnitude; the map is closed-form invertible as-is | drop, tine, vortex (all profiles), pinch (saddle), ripple, swirl, scroll, torsion, Chladni, spark shear, Chirikov | no sub-stepping; ±k pairs invert analytically |
| **Sub-stepped displacement field** | an exactly divergence-free field **d** applied as a finite step — area-preserving to first order only | wake, **viscous multipole burst** (§2.3) | per-pass displacement ≤ a fraction of the core radius a (the wake's ≤ a/4 rule); ±D pairs cancel to first order; soak-tested under the wake's gate, not the ripple's |

The second class is legitimate — the wake proved it — but membership must be declared, never discovered in a failing soak.

### 2.1 Wave torsion (radial angular shear) — the proof brick
θ′ = θ + A·sin(k·r − φ)·e^{−γr},  r′ = r. A pure rotation by θ(r): **exactly area-preserving, exact at any amplitude, no sub-stepping ever** (the vortex lemma verbatim). Implementable as a third vortex profile or a standalone pass — `[ITERATE: profile vs standalone; profile is cheaper, standalone reads better in the operator book]`. Note-on triggers an outward phase sweep (φ = ωt episode with decay); per-note bend → k. Visual: concentric diffraction rings, shockfronts.

### 2.2 Chladni lattice (quadrature kick-drift)
x₁ = x + (A/k_y)·cos(k_y·y)·cos(ωt);  y₁ = y + (B/k_x)·cos(k_x·x₁)·sin(ωt). Kick-drift: the second shear evaluates at the displaced x₁ — that ordering is what makes det J = 1 exact; a "simultaneous" version is wrong and must not be "simplified" into being. Lives at both insertion points like the ripple: **live** (quadrature time-dependence = breathing nodal lattice, nothing accumulates when A returns to 0 at fixed k, φ) and **bake** (delta-driven). **The musical core: chord intervals set k_x : k_y** — 3:2 a fifth, 4:3 a fourth, 5:4 a major third — harmony literally becomes geometry. `[ITERATE: which voices' interval? Proposal: the two lowest sounding voices, recomputed on voice begin/end, smoothed.]` Poly pressure → A, B.

### 2.3 Viscous multipole burst (the strike operator)
The time-integrated displacement field of an impulsive viscous 2D multipole — **our extension of Jaffer's own move**: his Lamb–Oseen paper (arXiv:1810.04646) time-integrates the viscous angular velocity into a net closed-form rotation; this applies the identical integration to the sin(mθ) viscous multipole family, where it turns out to go **elementary for m ≥ 2** (the dipole m = 1 carries a logarithmic E₁ kernel and is excluded; for m ≥ 2 the viscous cutoff γ_m(s) = 1 − e^{−s}Σₖ₌₀^{m−1} sᵏ/k! vanishes fast enough at s → 0 that no special function survives the integration). The result was a **serendipitous derivation** — found by iterating on the real code in pursuit of how the music looks in the author's head, not by hunting a theorem — and the docs present it in exactly that voice: an extension of Jaffer's method, literature-checked (viscous multipole vortex decay, impulsive Stokes flows) before any wording implies priority, with no claim beyond "this is what we needed and here is its closed form."

**The quadrupole (m = 2), the primary voice.** Integrated stream function and displacement, with the dual-time Gaussian-core regularization (t₀ = a²/4ν, ℓ² = a² + 4νt, S₀ = r²/a², S₁ = r²/ℓ², χ(S) = (1−e^{−S})/S):

   Ψ₂ = (D₂/8π)·sin(2(θ−θ₀))·[χ(S₁) − χ(S₀)],   **d** = ((1/r)∂θΨ, −∂rΨ)

   d_r = (D₂/4πr)·[χ(S₁)−χ(S₀)]·cos(2(θ−θ₀)) with the matching d_θ; θ₀ aligns the ejection lobes (the streamer axis — pen azimuth or glide direction). Normalize D₂ from the peak lobe displacement at r = a (the stagnation origin makes the naive center-normalization meaningless). General m ≥ 2 via Φ_m(S) = (1/S)[1 − e^{−S}Σₖ₌₀^{m−2}(1−k/(m−1))Sᵏ/k!]; m from pitch class `[ITERATE: pitch-class→m table — quadrupole default, hexapole/octupole on upper classes?]`.

**Class: sub-stepped displacement field** (see the class table): **d** is exactly divergence-free as a field, but the finite map is area-preserving to first order only — the wake's family, the wake's ≤ a/4-style rule, the wake's soak gate. Evaluate χ via expm1 (the Lamb–Oseen small-r lesson applies verbatim).

**The unification, stated because it is the point:** near the core the blob reduces to pure hyperbolic strain (d_x ≈ λx, d_y ≈ −λy) — **the pinch operator is this burst's r → 0 limit** — while the far field decays as cos(mθ)/r, the algebraic lobe's profile. The viscous multipole is to the pinch/lobe pair what Lamb–Oseen is to solid-body/far-field rotation: the physical bridge. (The earlier algebraic lobe r′² = r² + A·cos(mθ) is superseded by this operator and does not ship.)

**Performance mapping — the age knob:** ℓ² = a² + 4νt is a *diffusion age*. A strike fires the event at ℓ = a (sharp); the release envelope grows ℓ over the following seconds, per-frame passes emitting the increment — the discharge visibly *diffuses* as it dies, straight out of the mathematics. Visual: directional lobed ejection with entrainment wakes — the drop's electric cousin, with a lifetime.

### 2.4 Spark shear (piecewise kick-drift) & the spark composition
**The full spark strike is a composition: one exact drop pass (the Joule blast) + rotated quadrupole burst sub-passes (the pinch lobes, §2.3) + this operator's jagged shears.** Note the physics that forces the drop's role: a radial "monopole blast" displacement field cannot be divergence-free — a source IS divergence, and no stream function produces radial outflow — but the engine's oldest operator already solves radial expansion exactly (Jaffer's √-form displaces outward while conserving area). The newest operator composes with the first.

x₁ = x + A·tri(k_y·y + φ_y);  y₁ = y₁ + B·tri(k_x·x₁ + φ_x). The theoretical statement worth printing: **shears are exactly invertible for ANY profile** — triangle, sawtooth, noise — because x′ = x + f(y) inverts as x = x′ − f(y) regardless of f's smoothness. Kinks in the map become creases in the ink: that IS the dielectric aesthetic, legally. Strike-triggered with exponential decay episodes (A, B ∝ e^{−t/τ}); CC74 → k (thick channel ↔ fine streamers) under a slide_mode-style Anod default. Frequency stacking (k, 2k, 4k octaves) per the draft — bind stack depth to a params field, not a magic constant.

### 2.5 Chirikov standard map (scaled) — the boss fight
y′ = y + K·sin(k·x);  x′ = x + **ε·y′**. The ε-scaled drift is mandatory: the textbook map lives on a torus, and x′ = x + y′ on a non-wrapping canvas is a canvas-scale shear; ε·y′ keeps det J = 1 (still a shear pair) at usable magnitudes. K is **delta-driven from the mod wheel / breath** (never absolute); the payoff is playing the KAM transition live — below Greene's threshold (K_c ≈ 0.9716) smooth invariant sheets, above it chaotic filamentation and island chains. **Honest caveat, stated up front:** chaotic stretching is exactly what resampling erosion punishes; the #33-style erosion soak is this operator's hardest gate and may force a per-pass K ceiling. `[ITERATE: acceptable erosion budget vs. how deep into chaos the instrument may go.]`

---

## 3. The Anod composite: strain-glow

The medium's identity in one idea: **read the field's strain, not its phase bands.** The accumulated map's Jacobian is recoverable by finite-differencing the stored source coordinates between neighboring texels — the field already carries it. Compute the Frobenius norm ‖J‖_F = sqrt(Tr(JᵀJ)); regions of high accumulated strain glow like ionized gas; charge phase modulates filament color; aux drives per-event hue.

* Background: near-black (the vacuum/glass to sumi's washi). The screen-locked-substrate invariant holds: any background texture (glass grain, phosphor speckle `[ITERATE: substrate design]`) samples in screen space, never through the field.
* Palettes: an Anod family (electric blue/violet, plasma orange, phosphor green as the initial three, ids continuing the existing table). QUALITY_OF_LIFE_SPEC's user palettes apply to both media.
* Live insertion point: torsion sweeps and Chladni quadrature ride the composite like the live ripple — non-destructive, dip samples the un-shimmered field.
* Prints: a dip in Anod is "the photograph of the discharge" — same readback machinery, medium-styled output. `[ITERATE: long-exposure look? strain accumulation buffer?]`

---

## 4. Default binding tables (per medium, one-consumer rules unchanged)

| Dimension | Sumi default | Anod default |
|---|---|---|
| strike | drop | spark composition: drop + rotated quadrupole burst (+ spark-shear episode) |
| press feed (0xD0) | boundary growth | torsion sweep feed `[ITERATE]` |
| swirl (0xA0) | Lamb–Oseen | Chladni amplitude |
| per-note bend (mode 0/1) | glide / ripple amp | torsion k / spark k |
| CC74 (slide modes) | aux / pinch | spark frequency |
| mod wheel / breath | vortex | Chirikov K (delta) |
| master bend | shear tine | scroll-compatible shear `[ITERATE]` |

Every cell above is a *default*; the CC map and mode params override exactly as today. `[ITERATE: this table is the file most likely to change after the first hour of playing.]`

---

## 5. Roadmap sketch (to be formalized after iteration)
Torsion (proof brick, desktop) → Chladni (live+bake) → viscous multipole burst (wake-class sub-step + age envelope; its soak is the wake's gate) → spark shear + the composed strike → Chirikov (erosion soak = the boss gate) → strain composite + palettes → medium param + binding tables + surface/strip integration → docs: the Anod operator book with live scenes, the burst page carrying the serendipity note and the Jaffer-lineage line.

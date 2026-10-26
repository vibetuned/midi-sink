#!/usr/bin/env python3
"""Step 92 — fold the lessons drafts into the book (site/src/content/docs/lessons/).

Reads site/drafts/lessons/module_0N/module_0N.md (the drafts stay), applies the
review's fixes (site/drafts/lessons/REVIEW_modules_02-06.md) and the site's
link conventions, writes lessons/module-0N.md with Starlight frontmatter.
Every targeted replacement asserts exactly one occurrence.
"""
import re, sys, pathlib

ROOT = pathlib.Path("/Users/osf/pprojects/midi-sink")
DRAFTS = ROOT / "site/drafts/lessons"
import os
OUT = pathlib.Path(os.environ.get("FOLD_OUT", str(ROOT / "site/src/content/docs/lessons")))   # FOLD_OUT: a check run writes elsewhere
OUT.mkdir(parents=True, exist_ok=True)

TITLES = {
    1: ("Exterior calculus, the exactness rule and area-preserving phase space",
        "Differential forms in 2D and 4D, why the engine stores displacements, the exactness rule and its four operator families, the Jacobian read by Sumi, Anod and Kessho, and Suzu's magic circle — with a standalone WebGL2 + WebAudio lab."),
    2: ("Hamiltonian vector fields and symplectic integrators",
        "Hamiltonian flows and their splittings on the canvas (Chladni, the braid, the wake), the port-Hamiltonian Dirac network, symplectic Euler, Störmer–Verlet and RK4 compared, the shadow Hamiltonian at audio rate, the Toda ring's leapfrog in double, and the four ways a symplectic integrator still fails."),
    3: ("Darboux's theorem, Moser's trick and generating functions",
        "Riemannian strain against symplectic flatness inside Kessho, Darboux coordinates on the crystal drop, Moser's homotopy, the four types of generating function and the engine's uses of them: the Arnol'd kick, the Chirikov map, the sheared cell and its calibration, the retune."),
    4: ("Integrable systems, action-angle coordinates and KAM theory",
        "Liouville integrability on the billiard, the Toda lattice and the modal bank; the perturbed torus, Poincaré sections, rotation numbers, Poincaré–Birkhoff islands and Greene's threshold; the Arnol'd lock as a tongue on the water."),
    5: ("Resonance overlap, cantori and phase-space diffusion",
        "Chirikov's overlap criterion, the breakup of the golden torus, Aubry–Mather cantori and their turnstiles, the regimes of momentum diffusion — on the canvas's Chirikov operator and Suzu's kicked rotor."),
    6: ("High-dimensional dynamics and Arnold diffusion",
        "The dimensional divide past two degrees of freedom, whiskered tori and the Arnold web, Nekhoroshev stability and the FLI, read on the free braid and the coupled modal lattice — the capstone."),
}

def rep(s, old, new, count=1):
    n = s.count(old)
    if n != count:
        print(f"!! expected {count} of {old[:90]!r}, found {n}")
        sys.exit(1)
    return s.replace(old, new)

def rx(s, pat, new, count=1, flags=0):
    found = len(re.findall(pat, s, flags))
    if found != count:
        print(f"!! regex expected {count} of {pat[:90]!r}, found {found}")
        sys.exit(1)
    return re.sub(pat, lambda _m: new, s, flags=flags)

LAB_LINK = lambda m: f"[the module's lab](../module_0{m}_lab.html) (a standalone page — a teaching re-implementation, never the engine)"
LAB_FILE = lambda m: f"[`module_0{m}_lab.html`](file:///Users/osf/pprojects/midi-sink/site/drafts/lessons/module_0{m}/module_0{m}_lab.html)"

def head_2_6(s):
    s = rep(s, "*Target codebase: `midi-sink` as shipped (`libsumi 1.12.0, Voxo 0.26.0`, 2026-10-25). All code citations and gate numbers refer to ",
            "*Lecture notes written against `midi-sink` as shipped (libsumi 1.12.0, Voxo 0.26.0, 2026-10-25) and checked claim by claim against the engine before this fold (2026-10-26; libsumi is unchanged since, and Voxo's later versions add the sampler's second generation and touch no synth claim here). Code citations and gate numbers refer to ")
    s = rep(s, "`_work/DECISIONS_13.md`", "`docs/DECISIONS.md` Part XIII (cited `DECISIONS_13 #n`)")
    # the head line ends with ".*": add the lab sentence inside the italics
    lines = s.split("\n")
    assert lines[2].endswith(".*"), lines[2][-40:]
    lines[2] = lines[2][:-1] + " The lab beside the module is a standalone teaching re-implementation, never the engine; the engine's own lab is [the Suzu lab](../../suzu/lab/).*"
    return "\n".join(lines)

def lines_generic(s):
    s = re.sub(r"(`voxo/src/suzu\.h`) lines? \d+(?:[–-]\d+)?", r"\1", s)
    s = re.sub(r" \(lines \d+[–-]\d+\)", "", s)
    s = re.sub(r", lines \d+[–-]\d+", "", s)
    s = re.sub(r" at lines \d+[–-]\d+", "", s)
    s = re.sub(r", line \d+", "", s)
    s = re.sub(r" line \d+\)", ")", s)
    return s

def globals_all(s):
    s = s.replace("`_work/DECISIONS_13.md #", "`DECISIONS_13 #")
    s = s.replace("`DECISIONS_13.md #", "`DECISIONS_13 #")
    return s

# ---------------------------------------------------------------- module 2
def fix2(s):
    s = head_2_6(s)
    # 1. the Chladni listing → the shader's one wave per pass
    s = rx(s, r"   ```glsl\n   // From libsumi chladni_fs \(PROJECT_SPEC\.md §10\.3\):\n.*?float u2 = u1 \+ s2, v2 = v1 - s2;\n   ```",
           "   In the tree each wave is its own pass: `chladni_fs` applies ONE wave per pass, for any pair of wavevectors (a rectangular pair is Taylor–Green's, an oblique pair the Jankó's staggered rows), stage 0 then stage 1:\n"
           "   ```glsl\n"
           "   // deform.glsl, chladni_fs — one wave of ψ = ½Ψ[cos(w1·P') + cos(w2·P')]\n"
           "   float s = wx * (P.x - p0x) + wy * (P.y - p0y);\n"
           "   vec2 P_src = P + 0.5 * psi * weight * sin(s) * vec2(wy, -wx);\n"
           "   ```\n"
           "   The flow of $\\frac{1}{2}\\Psi\\cos s$ is a pure shear along $(w_y, -w_x)$, the direction where $s$ is constant, so the lookup is exact at any amplitude; the second stage reads the field the first already advected.",
           flags=re.S)
    s = rep(s, "the exact inverse used by `sumi_invert_deform_point` applies the negated shears in **reversed order** ($s_2$ first, then $s_1$), round-tripping to float precision (`3.1e-8`).",
            "the exact inverse of a step is both shears negated in **reversed order** ($s_2$ first, then $s_1$) — a sign flip alone is not (`DECISIONS_5 #17, #23`); it is the conservation gate (`midi-sink --dev --soak chladni`) that holds the operator to its class, not a round-trip function.")
    # 2. the braid
    s = rep(s, "with circulation $\\Gamma_k$ (sign from pitch class mod 2, magnitude from MIDI velocity) and core radius $r_c$",
            "with circulation $\\Gamma_k = \\operatorname{sign}(C_k)\\,\\Gamma_{\\text{vel}}\\,(0.3 + 0.7\\,|C_k|)$ — $C_k$ the Sethares consonance of the note against the bass, $\\Gamma_{\\text{vel}} = 0.036$ canvas²/s at full velocity — and core radius $r_c$")
    s = rep(s, "in *free* mode (`braid_free = 1`)", "in *free* mode (`braid_mode` = `SUMI_BRAID_FREE`, the D key; pinned is the default)")
    s = rep(s, "\\ln\\!\\left( \\|x_j - x_k\\|^2 + r_c^2 \\right)$$",
            "\\ln\\!\\left( \\|x_j - x_k\\|^2 + R_j R_k \\right)$$\n   (the regulariser is the product of the two cores, `rc2[j][k]`, not one $r_c^2$).")
    s = rep(s, "(resolved by fixed-point iteration to $10^{-13}$ in `double`)",
            "(`sumi_braid_step`: resolved by fixed-point iteration in `double` to $10^{-15}$, at most sixteen rounds)")
    s = rep(s, "In `tests/physics_test.cpp`, a 3-vortex chord braided for 36,000 frames (10 minutes at 60 Hz) holds the linear impulse $\\sum_k \\Gamma_k x_k$, the angular impulse $\\sum_k \\Gamma_k \\|x_k\\|^2$, and $H_{\\text{Kirchhoff}}$ to parts in $10^9$.",
            "In `tests/normalizer_tests.cpp`, a consonant triad braided for 72 000 steps at 1/120 s (ten minutes) holds $H_{\\text{Kirchhoff}}$ within $|dH/H| < 10^{-6}$, and the linear impulse $\\sum_k \\Gamma_k x_k$ and the angular impulse $\\sum_k \\Gamma_k \\|x_k\\|^2$ within $10^{-9}$ (`sumi_braid_invariants`).")
    # 3. the wake
    s = rep(s, "(`wake_fs`, `viscous = 0`)", "(`wake_fs`, `wake_profile` 0 — the default)")
    s = rep(s, "(`wake_fs`, `viscous = 1`)", "(`wake_profile` 1)")
    s = rep(s, "That is why the engine splits any stroke with $d > a/4$ into $m = \\lceil 4d/a \\rceil$ sub-steps, and why the default viscous Stokeslet mode (`viscous = 1`, $\\ell/a \\ge 1.5$) keeps $|\\nabla d| \\le 0.25\\,d/a$.",
            "The sub-step is the engine's — a stroke is cut into $n = \\lceil \\text{len}/(\\text{tip}/4) \\rceil$ steps of at most $a/4$, and the shader's comment says only \"area-preserving to first order per sub-step, hence the ≤ a/4 budget (measured)\"; the fold at $d = a/2$ from the rear stagnation point is this lesson's own derivation of why that budget is the right one. The Stokeslet is the viscous profile (`wake_profile` 1); the inviscid doublet (0) is the default.")
    # 4. the Dirac gate; dim x verified against chain()
    s = rep(s, "so $\\dim x = 2m + 3$ is **odd**!",
            "so $\\dim x = 2m + 3$ is **odd** (`Dirac::chain`: $m + 1$ extensions, $m$ momenta, the bridge's extension and momentum)!")
    s = rep(s, "and `tests/suzu_test.cpp` asserts `gain_q == 1.0` to **`2e-16`** across 12 random skew matrices $J$ when $R = 0$, and strictly `< 1` when $R \\succeq 0$.",
            "and `tests/voxo_suzu_tests.cpp` (gate 59) checks twelve random skew matrices $J$ ($n$ from 6 to 16, at a step fifty times the engine's): $|\\text{gain}_q - 1| < 10^{-9}$ when $R = 0$ — measured **`2e-16`** — and strictly `< 1` when $R \\succ 0$.")
    # 5. Gromov
    s = rep(s, "it requires the oriented area projection onto *every* conjugate $(q_i, p_i)$ plane to sum to an invariant, known as **Gromov's Non-Squeezing Theorem**.)",
            "it preserves the *sum* of the oriented area projections onto the conjugate $(q_i, p_i)$ planes — that much is linear algebra, $\\omega$-preservation itself — and, far deeper, it forbids embedding a ball of radius $r$ into the cylinder $B^2(R) \\times \\mathbb{R}^{2n-2}$ for any $R < r$, which volume preservation alone would allow: **Gromov's non-squeezing theorem**.)")
    # 6. RK4's determinant
    s = rep(s, "\\frac{\\varepsilon^6}{36}", "\\frac{\\varepsilon^6}{72}", count=2)
    s = rep(s, "\\varepsilon^6/36", "\\varepsilon^6/72", count=3)
    # 7. the Störmer–Verlet shadow: the factor on the drifted variable
    s = rep(s, "$$\\tilde{H}_{\\text{SV}}(x, y) = \\frac{1}{2}\\!\\left(x^2 + \\left(1 - \\frac{\\varepsilon^2}{4}\\right)y^2\\right)$$",
            "$$\\tilde{H}_{\\text{SV}}(x, y) = \\frac{1}{2}\\!\\left(\\left(1 - \\frac{\\varepsilon^2}{4}\\right)x^2 + y^2\\right)$$")
    # 8. the breaker
    s = rep(s, "either blowing the oscillator past the `+14 dBFS` safety breaker or killing the note into silence!",
            "either blowing the oscillator up — until its block is no longer finite and the bus breaker cuts it (the engine has no level threshold; the Suzu lab's worklet mutes at full scale and restarts) — or killing the note into silence!")
    s = rep(s, "until the `+14 dBFS` safety breaker clamps the voice!",
            "until the lab's safety breaker clamps the voice (the lab's own, at +14 dB over the strike's energy; the engine's bus breaker has no level threshold — it cuts a block that is no longer finite)!")
    # 9. the live cap was never a shipped control
    s = rep(s, "As proven in `DECISIONS_13.md #1` (and demonstrable via the `live_cap` red control in the Suzu Lab), modulating the Toda cap from the live peak compression acts like a child pumping a swing: the time-varying potential does net positive work on each soliton transit, **pumping `+162 dB/s`** into the ring until the breaker trips!",
            "As found during step 83 (`DECISIONS_13 #1`, item 3 — a development finding, never a shipped control; this module's standalone lab offers it as its own red control), modulating the Toda cap from the live peak compression acts like a child pumping a swing: the time-varying potential does net positive work on each soliton transit, **pumping `+162 dB/s`** into the ring. The ring's shipped red control is a different one: `toda_gate` 0 lifts the resolution rule, and the energy climbs +19 dB in a second at C8 (gate 52).")
    s = rep(s, "(the exact failure mode from `_work/DECISIONS_13.md #1`)",
            "(the failure mode found at step 83, `DECISIONS_13 #1` — the lab's own control; the engine never shipped one)")
    # 10. toda_exp
    s = rep(s, "(`0.000004 dB` with the 5-term minimax `suzu::toda_exp`)",
            "(`0.000004 dB` re-measured with `suzu::toda_exp`, a degree-11 Taylor after Cody–Waite range reduction, $10^{-14}$ relative — the same bits on every platform)")
    # 11. the stability bounds
    s = rep(s, "Suzu's `VerletString` and `TodaRing` enforce $\\lambda \\le 0.95$, or $\\varepsilon \\le 1.90$).",
            "Suzu's bounds are each voice's own: the chain's Courant number $s \\le 1$ (`string_cfl`; 1.05 is the red control), the ring's $k \\cdot \\text{cap} \\le 0.9$ at the note together with the resolution rule $\\sqrt{k}\\,\\hat{c}\\,b \\le 0.15$, and $f < 1.9$ the SVF's).")
    # 12. the membrane
    s = rep(s, "`voice_kind` 9, §13.12):**", "`voice_kind` 9, §13.9):**")
    s = rep(s, "this periodic tension modulation turns the highest grid mode's equation of motion into the **Mathieu equation**! When the top mode sits near the $\\operatorname{tr} J = -2$ stability boundary ($\\lambda_{2D} \\in [1.82, 1.95]$), its conjugate unit-circle eigenvalues collide at $\\lambda = -1$ and split onto the real axis ($\\det J = 1$ still holds, while the top mode's energy grows by $+90\\text{ dB}$!). Lowering the gong's Courant ceiling from `1.95` to **`1.80`** moves the top mode's Floquet multiplier safely away from $-1$, keeping 10-minute energy drift inside **`0.0002 dB`**.",
            "in this lesson's reading this periodic tension modulation turns the highest grid mode's equation of motion into the **Mathieu equation**: when the top mode sits near the $\\operatorname{tr} J = -2$ stability boundary ($\\lambda_{2D} \\in [1.82, 1.95]$), its conjugate unit-circle eigenvalues collide at $\\lambda = -1$ and split onto the real axis ($\\det J = 1$ still holds while the top mode's energy grows without bound). The record says only what was measured — a swelling sheet at 1.95 diverged — and the gong's bar is **`1.80`** (`MEMBRANE_CFL_GATE_GONG`), which in this reading keeps the top mode's Floquet multiplier away from $-1$; the ten-minute energy drift measured inside **`0.0002 dB`** ($-0.00016$ dB on the 8 × 6 grid at 96 kHz).")
    # the listing's comment, the chapter link, the lab link
    s = rep(s, "// From voxo/src/suzu.h (lines 722–733) — TodaRing::step:",
            "// From voxo/src/suzu.h — TodaRing::step (Voxo 0.23.0; checked word for word at the review):")
    s = rep(s, "`site/src/content/docs/suzu/toda-ring.mdx`", "[the Toda ring chapter](../../suzu/toda-ring/)")
    s = rep(s, LAB_FILE(2), LAB_LINK(2))
    # the displacement field, the composite, the reader
    s = rep(s, "`libsumi`'s Layer A (`RGBA32F`) stores the **total cumulative Lagrangian displacement vector**",
            "`libsumi`'s displacement field — the ping-pong texture, RGBA16F by default (RGBA32F behind a desktop flag) — stores the **total cumulative Lagrangian displacement vector**")
    s = rep(s, "the ping-pong shader (`advect_fs`) updates the displacement vector by **re-basing**:",
            "every operator pass (`deform.glsl`'s `*_fs`) updates the displacement vector by **re-basing**:")
    s = rep(s, "in the composite pass (`composite_fs`)", "in the composite pass (`composite.glsl`)")
    s = rep(s, "and Kessho (`kessho_fs`) can directly inspect the cumulative Jacobian determinant $\\det(I + \\nabla d)$ via `dFdx`/`dFdy` (`--kessho-flat` at $z = 1$) to verify $|\\det J - 1|$.",
            "and Kessho (`composite.glsl`'s `kessho_col`) reads the cumulative Jacobian by central differences over texels — the short stencil for $\\psi$ and $\\sigma$, ±3 texels at 512 for $\\operatorname{tr} J$ — and its determinant $\\det(I + \\nabla d)$ in the `--kessho-flat` view at $z = 1$ to verify $|\\det J - 1|$.")
    return s

# ---------------------------------------------------------------- module 3
def fix3(s):
    s = head_2_6(s)
    s = rep(s, "(`kessho_fs`, `PROJECT_SPEC.md §16`, `site/src/content/docs/operators/minerals.mdx`)",
            "(`composite.glsl`'s `kessho_col`, the spec draft's §10.7, [the Minerals page](../../operators/minerals/))")
    s = rep(s, "After $N$ exact suminagashi passes (`drop_fs`, `vortex_fs`, `chladni_fs`, `tine_fs`), Layer A stores the cumulative Lagrangian displacement $d(x, y)$",
            "After $N$ exact suminagashi passes (`drop_fs`, `vortex_fs`, `chladni_fs`, `tine_fs`), the displacement field — the ping-pong texture, RGBA16F by default — stores the cumulative Lagrangian displacement $d(x, y)$")
    s = rep(s, "with Jacobian matrix $J(x) = I + \\nabla d(x)$. From this single Jacobian",
            "with Jacobian matrix $J(x) = I + \\nabla d(x)$, read by central differences over texels. From this single Jacobian")
    s = rep(s, "(Layer A `--kessho-flat` at $z = 1$)", "(the `--kessho-flat` view at $z = 1$)")
    s = rep(s, "2. **The Riemannian Metric Pullback (Layer A Birefringence & Layer B Caustics):**",
            "2. **The Riemannian Metric Pullback (Layer A, the birefringence) and the Rotation Side (Layer B, the trace discriminant):**")
    s = rx(s, r"   Meanwhile, in Layer B, the deformed sheet's heightfield.*?\(`conical_ring`, `minerals\.mdx` line 40\)!",
           "   Meanwhile, Layer B reads the *rotation* side of the same Jacobian, where the metric is blind. The **trace discriminant** $\\Delta = (\\operatorname{tr} J)^2 - 4$ sorts each texel as elliptic ($\\Delta < 0$, a cool tint) or hyperbolic ($\\Delta > 0$, a warm one): a pure rotation has $C = I$ and no birefringence at all, yet $\\Delta = 4(\\cos^2\\phi - 1) < 0$ tints it. The caustic is the determinant of the sheet lifted to a viewing depth $z$ — for an area-preserving $J$, $\\det(I + z\\nabla d) = 1 + z(1 - z)(\\operatorname{tr} J - 2)$ — so its fold is the iso-trace contour $\\operatorname{tr} J = 2 + 1/(z(z - 1))$, read at three depths $z_R, z_G, z_B$ by Barite's dispersion. There is no heightfield and no curvature in the medium. Where $|\\Delta| < 0.15$ and $\\sigma > 0.05$ — near the parabolic line of $\\Delta$, where the two eigenvalues of $J$ coincide — Barite's `conical_ring` reads the caustic through a ring kernel of eight points: **Hamilton's conical refraction**, a stylisation of the hollow cone on the fold (Module 1 §1.5 lists these observables with their thresholds).",
           flags=re.S)
    s = rep(s, "[`site/src/content/docs/operators/drop.mdx`](file:///Users/osf/pprojects/midi-sink/site/src/content/docs/operators/drop.mdx#L38-L60) and [`minerals.mdx`](file:///Users/osf/pprojects/midi-sink/site/src/content/docs/operators/minerals.mdx#L6-L13)",
            "[the Drop page](../../operators/drop/) (the crystal drop's habits) and [the Minerals page](../../operators/minerals/)")
    s = rep(s, "barite desert-rose rosette $R(\\theta) = r_0(1 + \\beta \\cos^8 m\\theta)$)",
            "barite desert-rose rosette $R(\\theta) = r_0(1 + 0.6\\cos^8 m\\theta)/(1 + 0.6 \\cdot 35/128)$, mixed toward the circle by the roundness)")
    s = rep(s, "If you simply overwrite `eps = e` (`Cell::retune_plain`, line 159), the fixed point $(x, y)$ lands on a new $\\varepsilon$-ellipse with a different amplitude $A_{\\text{new}} \\neq A_{\\text{old}}$, causing an audible amplitude bump across the glide! `Cell::retune(e)` (lines 149–156) applies the exact linear Darboux rescale `s = amp / sqrtf(a2)` onto the new $\\varepsilon$-ellipse of the same action $\\pi A^2$, keeping the orbit's peak amplitude invariant across any MPE slide.",
            "If you simply overwrite `eps = e` (`Cell::retune_plain`), the state $(x, y)$ lands on a new $\\varepsilon$-ellipse with an amplitude that depends on the phase at the retune — $\\sqrt{I_{\\varepsilon'}(x, y)/(1 - \\varepsilon'^2/4)}$, the old state read under the new $\\varepsilon'$ — a ripple across the glide. `Cell::retune(e)` reads that implied amplitude and rescales both coordinates (`s = amp / sqrtf(a2)`) onto the new ellipse of the held peak $x$-amplitude $A$: a homothety that keeps the **amplitude**, not the action — the orbit $I_\\varepsilon = A^2(1 - \\varepsilon^2/4)$ encloses $\\pi A^2\\sqrt{1 - \\varepsilon^2/4}$, which changes with $\\varepsilon$ (Module 1 §1.6.2; the suite's ripple figures are there).")
    s = rep(s, "On `suzu::TodaRing`, each node $i$ has lattice index $\\xi = i$ and physical lab position $x(\\xi) = \\xi + q(\\xi)$. As noted in `voxo/src/suzu.h` lines 708–710, reading the compression in the **Eulerian lab coordinate** $x = \\xi + q(\\xi)$ (a nonlinear coordinate transformation on the ring whose Jacobian is $1 + \\partial_\\xi q = 1 + r$) sharpens a hard soliton's peak where the lattice nearly touches ($r \\to -1$), adding **`-43 dB` of top-octave harmonic brightness** while the soliton's transit frequency around the ring stays locked!",
            "On `suzu::TodaRing`, each node $i$ has lattice index $\\xi = i$ and physical lab position $x(\\xi) = \\xi + q(\\xi)$. Reading the compression in the **Eulerian lab coordinate** $x = \\xi + q(\\xi)$ (a nonlinear coordinate transformation on the ring whose Jacobian is $1 + \\partial_\\xi q = 1 + r$) sharpens a hard soliton's density spike where the lattice nearly touches ($r \\to -1$) — and that reading was **rejected** at step 83: it put $-43$ dB of content in the top octave, which aliases. What ships reads the compression in the lattice's own coordinate, the pickup standing still against the mean drift ($\\xi$ runs back by $\\sum p / N$ a step, a Catmull–Rom cubic over the bonds). Two charts, the same physics, two spectra: the coordinate change is audible, and the engine chose the one that does not alias.")
    s = rep(s, "(anti-aliased by the two-point cubic `polyBLAMP` residual at lines 872–875)",
            "(anti-aliased by the two-point polyBLAMP residual of each kink through a one-sample delay — $-80.5$ dB of alias, gate 61)")
    s = rep(s, LAB_FILE(3), LAB_LINK(3))
    s = rep(s, "**`[RED] Uncalibrated In-Loop Force Shear (suzu.h line 187)`**", "**`[RED] Uncalibrated In-Loop Force Shear (Cell::step_sheared)`**")
    s = rep(s, "has unit determinant ($\\det = 1$, so it is a linear symplectomorphism: $dx \\wedge dy = dQ \\wedge dP$) and pulls",
            "has determinant $1/\\sqrt{1 - \\varepsilon^2/4}$ — it is **not** a linear symplectomorphism; it is the conjugation that turns the tilted ellipse into a circle, and the problem is to say exactly that — and pulls")
    s = rep(s, "whereas `Cell::retune_plain(e)` amplitude-modulates the note by $\\sqrt{(1 - \\varepsilon_{\\text{old}}^2/4)/(1 - \\varepsilon_{\\text{new}}^2/4)}$.",
            "whereas `Cell::retune_plain(e)` changes the amplitude by $\\sqrt{I_{\\varepsilon'}(x, y)/(1 - \\varepsilon'^2/4)}\\,/A$ — the old state read under the new $\\varepsilon'$ — which depends on the phase at the retune and is not a fixed ratio.")
    return s

# ---------------------------------------------------------------- module 4
def fix4(s):
    s = head_2_6(s)
    s = rep(s, "and in `tests/suzu_test.cpp` (`_work/DECISIONS_13.md #3`), the Poincaré collision strobe $(s, \\sin\\psi)$ stays locked on a 1D curve occupying only **24 / 576** histogram cells over 66,393 collisions.",
            "and in `tests/voxo_suzu_tests.cpp` (gates 61–62, `DECISIONS_13 #3`) the engine's strobe — the $(x_1, x_2)$ pair sampled at the bass's reference phasor — stays on a line occupying only **24 / 576** cells (a central kick is radial), the 66 393 collisions of a minute lossless within $2.4 \\times 10^{-12}$ dB; the collision map $(s, \\sin\\psi)$ used below is this lesson's own coordinate on the same orbit, not what the suite counts.")
    s = rep(s, "2. **The Integrable $N$-Node Toda Lattice (`suzu::TodaRing`, `voxo/src/suzu.h` lines 700–767):**",
            "2. **The Toda Lattice — integrable in continuous time, near-integrable as the engine's ring (`suzu::TodaRing`):**")
    s = rep(s, "Because Hénon and Flaschka (1974) proved that `TodaRing` possesses",
            "Because Hénon and Flaschka (1974) proved that the continuous-time lattice possesses")
    s = rep(s, "multi-soliton collisions are simply linear phase shifts on an $N$-torus $\\mathbb{T}^N$.",
            "multi-soliton collisions are simply linear phase shifts on an $N$-torus $\\mathbb{T}^N$. The engine's ring is the discrete leapfrog on this Hamiltonian: it conserves a modified $\\tilde{H}$, not the Lax invariants — near-integrable, which is why its gate measures the soliton's shape (amplitude × width within 0.72 %) instead of asserting that the ring never breaks into chaos (`DECISIONS_13 #1`, the shape gate).")
    s = rep(s, "[`site/src/content/docs/operators/arnold.mdx`](file:///Users/osf/pprojects/midi-sink/site/src/content/docs/operators/arnold.mdx)",
            "[the Arnol'd lock page](../../operators/arnold/)")
    s = rep(s, LAB_FILE(4), LAB_LINK(4))
    s = rep(s, "1. **Bijectivity Bound of `arnold_fs` (`site/src/content/docs/operators/arnold.mdx` lines 21–36):**",
            "1. **Bijectivity Bound of `arnold_fs` ([the Arnol'd lock page](../../operators/arnold/)):**")
    s = rep(s, "Arnol'd Lock Operator (`arnold.mdx`)", "Arnol'd Lock Operator (`arnold_fs`)")
    return s

# ---------------------------------------------------------------- module 5
def fix5(s):
    s = head_2_6(s)
    s = rep(s, "(`site/src/content/docs/operators/chirikov.mdx`, `voxo/src/suzu.h` line 1365)?",
            "(Greene 1979 — the engine quotes no $K_c$: it caps a canvas throw at $K = 1.25$ a step and the rotor's $K$ at 2.5 on the wheel)?")
    s = rep(s, "[`site/src/content/docs/operators/chirikov.mdx`](file:///Users/osf/pprojects/midi-sink/site/src/content/docs/operators/chirikov.mdx#L36-L65) and [`suzu::Rotor`](file:///Users/osf/pprojects/midi-sink/voxo/src/suzu.h#L1358-L1386):",
            "[the Chirikov page](../../operators/chirikov/) and `suzu::Rotor` (`voxo/src/suzu.h`):")
    s = rep(s, "produces an effective half-integer resonant potential $V_{1/2}(2\\theta - 2\\pi t) = -\\frac{K^2}{4\\pi^2}\\cos(2\\theta - 2\\pi t)$ with separatrix half-width $\\Delta p_{1/2} = \\frac{K}{\\pi}$. Show that requiring the primary separatrices ($\\Delta p_0 = 2\\sqrt{K}$) and the secondary $1:2$ separatrix ($\\Delta p_{1/2} = K/\\pi$) to fill the half-spacing $\\pi$ gives the improved overlap equation:",
            "produces an effective half-integer resonance in $2\\theta - 2\\pi t$ whose separatrix half-width is $\\Delta p_{1/2} = K/2$ (§5.1; derive the resonant potential's amplitude on the way). Show that requiring the primary separatrices ($\\Delta p_0 = 2\\sqrt{K}$) and the secondary $1:2$ separatrix ($\\Delta p_{1/2} = K/2$) to fill the half-spacing $\\pi$ gives the improved overlap equation:")
    s = rep(s, "$$2\\sqrt{K} + \\frac{K}{\\pi} = \\pi \\quad \\Longrightarrow \\quad K_{\\text{overlap}}^{(2)} \\approx 1.46$$",
            "$$2\\sqrt{K} + \\frac{K}{2} = \\pi \\quad \\Longrightarrow \\quad K_{\\text{overlap}}^{(2)} \\approx 1.46$$")
    s = rep(s, LAB_FILE(5), LAB_LINK(5))
    return s

# ---------------------------------------------------------------- module 6
def fix6(s):
    s = head_2_6(s)
    s = rep(s, "(`braid_free = 1`)", "(`braid_mode` = `SUMI_BRAID_FREE`, the D key; `sumi_braid_step` takes $n \\le 16$ vortices, fifteen voices in practice)")
    s = rep(s, "while the joint symplectic integrator preserves total energy to machine precision.",
            "while the joint symplectic integrator holds the bank's energy — the gate: the bell's eight modes with every decay zero, ten minutes, $-0.0026$ dB on the energy ledger, the partials within 0.08 cent at any admitted coupling. (Machine precision, $2 \\times 10^{-16}$ in the $Q$-norm, is the Dirac network's claim in Module 2, not the lattice's.)")
    s = rep(s, LAB_FILE(6), LAB_LINK(6))
    return s

# ---------------------------------------------------------------- module 1
def fix1(s):
    s = rep(s, "`module_01_lab.html`, beside these notes, is a single-file WebGL2 + WebAudio",
            "[`module_01_lab.html`](../module_01_lab.html), beside these notes, is a single-file WebGL2 + WebAudio")
    # the book's links climb two levels from lessons/module-01/ (the drafts' ../operators/ were written beside the book's root)
    s = re.sub(r"\]\(\.\./(operators|suzu|guide|reference)/", r"](../../\1/", s)
    return s

FIX = {1: fix1, 2: fix2, 3: fix3, 4: fix4, 5: fix5, 6: fix6}

def yaml_str(x):
    return '"' + x.replace('\\', '\\\\').replace('"', '\\"') + '"'

leftovers = []
for m in range(1, 7):
    src = (DRAFTS / f"module_0{m}" / f"module_0{m}.md").read_text(encoding="utf-8")
    first, rest = src.split("\n", 1)
    assert first.startswith("# Module "), first
    body = FIX[m](rest if m == 1 else first + "\n" + rest)
    if m != 1:
        body = body.split("\n", 1)[1]
        body = globals_all(body)
        body = lines_generic(body)
    title, desc = TITLES[m]
    fm = f"---\ntitle: {yaml_str(f'Module {m} — {title}')}\ndescription: {yaml_str(desc)}\n---\n"
    out = fm + body.lstrip("\n")
    (OUT / f"module-0{m}.md").write_text(out, encoding="utf-8")
    for i, line in enumerate(out.split("\n"), 1):
        if re.search(r"\blines? \d+|file://|_work/DECISIONS_13|suzu_test\.cpp|physics_test|kessho_fs|advect_fs|composite_fs|sumi_invert|braid_free|viscous = |dBFS|live_cap|§13\.12|6}\{36}|\^6/36|Layer A \(", line):
            leftovers.append(f"module-0{m}.md:{i}: {line[:160]}")
    print(f"module-0{m}.md: {len(out.splitlines())} lines")

print("--- leftovers for manual review:")
for l in leftovers: print(l)

# ---------------------------------------------------------------- the index
INDEX = """---
title: The lessons
description: Six modules of symplectic geometry taught on the engine as shipped — exterior calculus and the exactness rule, Hamiltonian vector fields and integrators, Darboux and generating functions, integrable systems and KAM, resonance overlap and cantori, high-dimensional dynamics — each with a standalone WebGL2 + WebAudio lab.
---

The engine is a course in symplectic geometry that happens to draw and to
sound. Every operator on the water is an area-preserving map with its class
declared and gated; every voice in Suzu is a Hamiltonian system stepped by a
symplectic integrator whose invariant is read live. These six modules teach
the mathematics on that material: each claim about the engine was checked
against the code and the suites as shipped (libsumi 1.12.0, Voxo 0.26.0 at the
writing, 2026-10-25; reviewed before the fold on 2026-10-26), and every number
quoted is a gate's printout, never an estimate. They are graduate-level notes:
exterior calculus and a first course in Hamiltonian mechanics are assumed.

| Module | What it teaches | Where it lives in the engine |
|---|---|---|
| [1 — Exterior calculus, the exactness rule and area-preserving phase space](module-01/) | forms and symplectomorphisms in 2D and 4D; why the field stores displacements; the exactness rule and its operator families; reading the Jacobian; the magic circle | the deformation chain, Sumi / Anod / Kessho, `suzu::Cell` |
| [2 — Hamiltonian vector fields and symplectic integrators](module-02/) | Hamiltonian flows and their splittings; the port-Hamiltonian network; Euler, Störmer–Verlet and RK4; the shadow Hamiltonian at audio rate; what still breaks | the Chladni, braid and wake passes, `suzu::Dirac`, `suzu::TodaRing`, the membrane |
| [3 — Darboux's theorem, Moser's trick and generating functions](module-03/) | Riemannian strain against symplectic flatness; Darboux coordinates on the drop; Moser's homotopy; generating functions and the engine's uses of them | Kessho's two layers, the crystal drop, the Arnol'd kick, the sheared cell, the retune |
| [4 — Integrable systems, action-angle coordinates and KAM theory](module-04/) | Liouville integrability; the perturbed torus; Poincaré sections and rotation numbers; islands and Greene's threshold; the tongue on the water | the billiard, the Toda ring, the modal bank, the Arnol'd lock |
| [5 — Resonance overlap, cantori and phase-space diffusion](module-05/) | Chirikov's criterion; the breakup of the golden torus; cantori and turnstiles; the regimes of diffusion | the Chirikov operator, `suzu::Rotor` |
| [6 — High-dimensional dynamics and Arnold diffusion](module-06/) | the dimensional divide; whiskered tori and the Arnold web; Nekhoroshev stability; the FLI | the free braid, the coupled modal lattice |

## The labs

Each module ends with a lab, a single HTML file beside it (`module_0N_lab.html`)
that runs in any WebGL2 + WebAudio browser: a ping-pong displacement field in
RGBA32F with the module's operators, and a 48 kHz audio thread with the
module's integrators. **They are teaching re-implementations, never the
engine** — the site carries no second implementation of any operator or
voice, and the labs are the one exception, declared as such in each file's
header. Their arithmetic was checked in node against the engine's for module 1
and they all render headless without error; their code was not gated against
the engine. The engine's own lab, the synth compiled to WebAssembly, is
[the Suzu lab](../suzu/lab/), and the water itself is the marble app the
operator pages embed.

## Conventions

* `§4`, `§10.2`, `§10.3`, `§13` are `docs/PROJECT_SPEC.md`; `§10.7`, `§10.8`,
  `§13.9`, `§13.13` are the v3.0 arc's spec draft (`_work/TO_PROJECT_SPEC.md`
  until the release folds it); `KESSHO §n` is the medium proposal
  (`_work/specs/medium_specs_v3.md`).
* `DECISIONS_10 #n` … `DECISIONS_13 #n` are Parts X–XIII of
  `docs/DECISIONS.md`, the record of what shipped.
* Gate numbers are the synth suite's (`tests/voxo_suzu_tests.cpp`); the
  canvas's conservation gates are the bench's `--soak <operator>`; the Kessho
  flatness gate is `--kessho-flat` (as measured, `DECISIONS_10 #4`).
* Where a lesson derives or interprets beyond what the record says (the wake's
  fold, the gong's Mathieu reading, the billiard's collision map), it says so.
"""
(OUT / "index.md").write_text(INDEX, encoding="utf-8")
print("index.md written")

if os.environ.get("FOLD_OUT"): sys.exit(0)   # a check run: the modules only

# ---------------------------------------------------------------- the sidebar
cfg = ROOT / "site/astro.config.mjs"
c = cfg.read_text(encoding="utf-8")
anchor = '            { slug: "suzu/lab" },\n          ],\n        },\n'
assert c.count(anchor) == 1
group = anchor + ('        {\n          label: "Lessons",\n          items: [\n            { slug: "lessons" },\n'
                  + "".join(f'            {{ slug: "lessons/module-0{m}" }},\n' for m in range(1, 7))
                  + '          ],\n        },\n')
if '"Lessons"' not in c:
    c = c.replace(anchor, group)
    cfg.write_text(c, encoding="utf-8")
    print("sidebar: Lessons group added")

# ---------------------------------------------------------------- the home page's card
home = ROOT / "site/src/content/docs/index.mdx"
h = home.read_text(encoding="utf-8")
card = '  <LinkCard title="Performance gallery" href="gallery/" description="Recorded performances, captioned by device, layout and modes." />\n'
assert h.count(card) == 1
if 'href="lessons/"' not in h:
    h = h.replace(card, card + '  <LinkCard title="The lessons" href="lessons/" description="Six modules of symplectic geometry taught on the engine as shipped, each with a standalone lab." />\n')
    home.write_text(h, encoding="utf-8")
    print("home: lessons card added")

# ---------------------------------------------------------------- lab 2's breaker label (the lab's own, not the engine's)
lab2 = ROOT / "site/public/lessons/module_02_lab.html"
t = lab2.read_text(encoding="utf-8")
n = t.count("Safety Breaker (+14 dBFS)")
t = t.replace("Safety Breaker (+14 dBFS)", "Lab breaker (+14 dB, the lab's own)")
lab2.write_text(t, encoding="utf-8")
print(f"lab 2: {n} breaker labels relabelled")

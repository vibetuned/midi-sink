# Module 1 — adaptation notes (2026-10-25)

What changed between `draft_module_01.md` (the author's draft, untouched) and
`module_01.md` + `module_01_lab.html`, checked claim by claim against the
engine as shipped (libsumi 1.12.0, Voxo 0.26.0, the step-86 suite). The
structure of the draft is kept: §1.1–§1.6, the lab with its three tasks, the
problem set.

## Claims in the draft that disagree with what shipped — corrected

1. **`Cell::retune` is a rescale, not a shear-scale.** The draft (§1.6.2, the
   lab's `retuneSuzu`, Problem 1.3b) describes a transformation that keeps $x$
   and moves $y$ onto the new ellipse through $v = (y - \varepsilon x/2)/\sqrt{1 - \varepsilon^2/4}$.
   What ships (`voxo/src/suzu.h`, `Cell::retune`) reads the invariant the
   state would have under the new $\varepsilon$, the amplitude it implies, and
   scales *both* coordinates by $A/a$ onto the orbit of the held amplitude
   (`amp`). It is a homothety (det $= (A/a)^2$); the engine's class table
   calls it "symplectic (a rescale to the same orbit)". The lesson now teaches
   the shipped form, says plainly what it is, and keeps the draft's shear-scale
   as the *comparison* in Problem 1.3(b) and as the lab's third retune option.

2. **The glide numbers.** The draft quotes 0.630 dB on a jump and 0.082 dB
   re-based. The suite's current printout (step 86, `voxo_suzu_tests` release
   run) is: re-based **0.097 dB** (the gate < 0.5); the plain recurrence under
   the per-sample ramp 0.057 dB and stepped once per block 0.062 dB, both
   printed, not gated. The lesson quotes these. **For the author:** the cells
   chapter (`site/src/content/docs/suzu/cells.mdx`) still says "0.097 dB
   against the naive form's 0.63 for a one-block jump"; the suite's printed
   plain-form numbers are now a tenth of that, so either the chapter's 0.63 is
   the first landing's measurement or the glide test's ripple metric no longer
   separates the forms (the plain forms over ±48 semitones should show
   ~1 dB of amplitude change by $A = \sqrt{E/(1 - \varepsilon^2/4)}$ alone — a
   +48-semitone jump from A3 is 0.205 dB in the lab's arithmetic). Worth a
   look before the lesson is published; not touched here.

3. **The Boris step rotates the velocities, not the state.** The draft's lab
   applied the magic circle to both planes and then rotated *positions and
   velocities* in the (1, 2) plane by a cos/sin matrix, and computed the angle
   as $2\arctan(B\,dt/2)$. `suzu::Gyro::step` is: the half kick
   $y \mathrel{-}= (\varepsilon/2)x$, the Cayley rotation of $y$ alone as three
   shears ($t = \tan(\theta/2)$, $s = \sin\theta$), the half kick, the drift
   $x \mathrel{+}= \varepsilon y$, with $\theta_B = \omega_B dt$ exactly (the
   field prewarped). The lesson's §1.6.3 and the lab now follow the engine.
   Checked in node (`lab_math_check.mjs`, scratch): with the lab's arithmetic
   the invariant $x^2 + y^2 - \varepsilon xy$ is exact over ten seconds at
   $B = 0$, the step's two angles at A3 under 20 Hz match the Zeeman formula to
   0.0005 cent with $|\det| = 1.000000000000$, and the rescale lands on
   $A = 0.5$ after a +48-semitone jump. The draft's rotation of the whole state
   would have given $\omega_0 \pm \omega_B$, not the Zeeman doublet.

4. **The field in Hz, 0..50.** The draft's slider ran 0..180 "B". The
   engine's `boris_b` is in hertz, 0..50 (24 full, 2 at rest, the poly
   pressure's). The lab's slider now matches.

5. **The habit profiles.** The draft gave the rosette as
   $r_0(1 + \beta\cos^{2p}(m\theta - \theta_0))$ and its lab used
   $0.72 + 0.45\cos^{2p}$ with $p$ from the roundness, and a four-term
   cosine support for the rhomb. `drop_fs` ships: rhomb
   $R = r_0/\max(|\sin\theta|, |\sin(\theta - 78.1°)|)$ with $r_0$ the inradius;
   hexagon $r_0/\max_i|\cos(\theta - 60°i)|$; rosette
   $r_0(1 + 0.6\cos^8 m\theta)/(1 + 0.6\cdot35/128)$ with $r_0$ the mean
   radius; every max softened by a log-sum-exp of sharpness
   $k = 2 + 40(1 - \text{roundness})^2$, then mixed toward the circle by the
   roundness. The lesson's table and the lab's two functions use the shipped
   forms (the spec draft's §10.7.8 still gives the proposal's general
   $\cos^{2p}$ form; the shader is the record).

6. **Conical refraction is a stylisation, not "the exact phenomenon".** The
   draft's §1.1 said Kessho implements conical refraction. What ships is
   Barite's `conical_ring`: where $|\Delta| < 0.15$ and $\sigma > 0.05$ the
   caustic is read through a ring kernel of eight points — Hamilton's hollow
   cone on the fold, stated in §10.7.8 as a stylisation. Reworded.

7. **The $z = 1$ gate as measured, not as proposed.** The draft's Task 1 asked
   for "$\det J = 1$ everywhere outside the drop rim". `DECISIONS_10 #4`: the
   field cannot hold the proposal's $10^{-4}$; the exact operators read the
   resampler's and the half-float field's floor (a median of 2e-3 at ±3 texels
   to 5e-3 at ±1), the rims own the maximum (8–30) and are masked, the CPU
   red control reads 50 max / 0.35 rms. The lesson's §1.5.3 and Task 1 say so.

8. **Layer A is the mineral's law.** The draft's Layer A formula had one brush
   and $\mathrm{ML}(\Gamma\sigma + \Gamma_0)$. As shipped: isotropic dark,
   uniaxial $\sin^2 2(\psi - \alpha)$, biaxial two brushes $2V$ apart, the
   sensitive-tint plate $\Gamma_{\text{eff}} = |\Gamma_0 + \Gamma\sigma\sin 2\psi|$
   replacing the brush; $\Gamma$ from the mineral under the Exposure knob; the
   chart a 1024-texel LUT from `kessho_lut.h`; since step 76 $\sigma$ read
   through the strain buffer. The lesson's §1.5.1 now lists these, and names
   the three-channel $\sin^2$ form as the web fallback (KESSHO §1.2), which is
   what the standalone lab uses.

9. **The tine profiles' exact shapes.** Twin kink: 1 to $d = H/3$ then linear
   to 0 at $H = 4\alpha$; terrace: $v - \sin(2\pi N v)/(2\pi N)$, $N = 4$;
   every profile a bump, the monotone fault dev-only (`--tine-fault`). Added
   to Family 1 with the −99.9 % soak finding.

10. **The Chladni eddy's $\rho$.** The draft wrote the ring as
    $\Theta(\rho) = \theta_0(4\rho^2(1 - \rho^2))^2$ with $\rho$ also used for
    the action $r^2/2$. `cells_fs` uses $s = r/R$ (normalized radius); the
    lesson writes $s$ to keep $\rho$ for the action.

11. **The naive update's timing.** "Blows up past +0.1 dB in under a second"
    → the gate's printout: over the 0.1 dB bound at 1.0 s, +690.2 dB after
    600 s (440 Hz, $A = 0.5$, 48 kHz).

13. **The lab's orientation convention (a bug in the draft's lab, found by
    measurement).** The draft's vertex shader emitted
    `v_st = (x·½ + ½, ½ − y·½)` — a vertical flip inside the chain. In WebGL's
    bottom-left convention that labels each texel with the coordinate of its
    mirror image, so every pass sampled the previous field upside down and the
    passes cancelled instead of composing — the failure §4.6 records from
    Metal, word for word. Read back from the field in headless Chrome, the
    draft's *exact* operators measured $|\det J - 1|$ at a median of 0.12–0.15
    (the swirl, a pure rotation, 0.148) and the $z = 1$ view glowed over the
    whole canvas. Fixed as §4.6 says: one texture space in every pass
    (`v_st = a_pos·½ + ½`), the only flip at the boundary (the pointer's $y$).
    After the fix, the same readback (±1 texel, the interior 16 texels in, the
    band boundaries skipped): swirl median 3.3e-4 / max 4.8e-4; rosette median
    1.9e-4, max 0.85 at its rim; rhomb median 1.6e-4, max 0.35 at its rim;
    pinch median 7.6e-6 / max 1.6e-2; the windowed-kick red control median
    5.2e-4 but p99 0.25 / max 0.52 — the engine's gate's picture (exact at the
    floor, the rims owning the maximum, the red control red). One more thing
    the fixed view shows and the lesson could use: the canvas's four edges
    read non-exact under the seeded swirl, because its $1/r^2$ tail pulls
    samples from off the sheet and `CLAMP_TO_EDGE` returns the border texel —
    the same "pulls texels in from outside the canvas" failure the monotone
    tine profile had (`--tine-fault`), seen from the other side.

14. **The spec's family count.** The draft's "four canonical constructions"
    are kept as the lesson's taxonomy, but the spec's rule (§10.2 additions)
    names three — twist-type, generating-function, angle-generated kick — and
    the lesson now maps its four onto those (Family 1 is the spec's "shear
    that ignores its own coordinate"; the pinch's streamline window is exact
    but not compactly supported, so it does not contradict the twist family's
    uniqueness).

## Things kept from the draft, verified

* §1.2's forms and the $\det J = 1$ criterion; §1.3's displacement payload,
  the $2^{-11}$ / $2^{-24}$ figures and the re-base formula (§4.2, as shipped
  since 1.2.0); Proposition 1.1 and its determinant; the pinch's determinant
  algebra; the Arnol'd $F_2$ and the five-step Newton; §1.5's $\sigma$, $\psi$,
  the trace discriminant with its tint thresholds (0.05, 0.8), the collapsed
  caustic identity and the iso-trace fold $2 + 1/(z(z - 1))$, $z \in [1.05, 8]$;
  §1.6.1's magic circle, its trace $2\cos(2\pi f/f_s)$ and 0.000 cent; the
  Zeeman doublet formula and the stereo pair; the lab's op list and views; the
  problem set's 1.1, 1.2 and 1.3(a).
* The draft's section numbers are the right documents: `§4`, `§10.2`, `§13`
  are `docs/PROJECT_SPEC.md`; `§10.7.x`, `§10.8.1`, `§13.13` are
  `_work/TO_PROJECT_SPEC.md`. The lesson's head says which is which.

## The lab file

`module_01_lab.html` is the draft's embedded HTML, unescaped from the Docs
export (the `\=`, `\_`, `\<` … escapes removed; its script passes
`node --check`), with the edits above (the habits, the retune's three modes,
the Boris step, the field's range, the HUD showing $I_\varepsilon$ without the
field and $H$ with it) and a header comment saying it is a teaching
re-implementation and never the engine. Rendered once in headless Chrome
(SwiftShader) to confirm the shaders compile and the page draws; the audio
needs a click and was checked in node instead (above). It keeps RGBA32F
(the engine: RGBA16F by default) and a ScriptProcessor (the engine's lab: an
AudioWorklet) — both said in the lesson.

## Added

* A "Beyond the cell" paragraph (§1.6) pointing at the Phase-14 voices'
  symplectic facts with their chapters, as a hook for Module 2.
* Task 3 points at the engine's own lab page (`/marble/suzu/cell.html`: the
  naive checkbox, the retune select) and the desktop's gyroscope patch with
  `boris_prewarp` 0 as the red, before the standalone file.
* Problem 1.3(b) rewritten as the comparison of the shipped rescale with the
  draft's shear-scale (determinants, phase, the measured ripple as the
  criterion).

## Not done, for the author

* The glide-number discrepancy between the cells chapter and the suite (item
  2 above).
* The spec draft's §10.7.8 rosette formula versus the shader's (item 5): a
  wording correction for the fold, if wanted.
* Where the lessons will live on the site (a `lessons/` section in the
  sidebar, MDX for the `<Lab>` component) — the draft stays `.md` in
  `site/drafts/lessons/` as the other drafts do.

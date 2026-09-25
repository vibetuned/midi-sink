# Step 38 — literature check for the viscous multipole burst

Recorded BEFORE the page draft's wording, as ROADMAP_5 Step 38 requires.
Searched 2026-09-22 (web search; abstracts read where linked). The question:
is the time-integrated displacement of an impulsive viscous 2-D multipole,
elementary for m ≥ 2 (MEDIUM §2.3), stated anywhere we could find?

## What is classical (the velocity fields)

* **The Lamb–Oseen vortex** (m = 0): the self-similar solution of the vorticity
  diffusion equation; its swirl (Γ/2πr)(1 − e^{−r²/4νt}) is the γ_1 cutoff.
* **The unsteady 2-D Stokeslet and rotlet** — the fundamental solutions for an
  instantaneously applied point force and point couple in two-dimensional
  Stokes flow: A. T. Chan & A. T. Chwang, "Unsteady singularities of Stokes'
  flows in two dimensions", *Int. J. Eng. Sci.* (1995),
  <https://www.sciencedirect.com/science/article/abs/pii/002072259500028V>;
  the 3-D siblings in Chan & Chwang, "The unsteady stokeslet and oseenlet",
  *Proc. IMechE C* 214 (2000). Our m = 1 (DECISIONS_4 #53) is the Stokeslet's
  velocity field integrated in time.
* **Viscous multipolar vortices in the Stokes approximation** — dipoles and
  quadrupoles as solutions of the vorticity diffusion equation driven by
  localized forces: S. I. Voropayev & Y. D. Afanasyev, *Vortex Structures in a
  Stratified Fluid: Order from Chaos* (Chapman & Hall, 1994),
  <https://www.routledge.com/Vortex-Structures-in-a-Stratified-Fluid-Order-from-Chaos/Voropayev-Afanasyev/p/book/9780412405600>;
  Y. D. Afanasyev & V. N. Korabel, "Starting vortex dipoles in a viscous
  fluid: asymptotic theory, numerical simulations, and laboratory
  experiments", *Phys. Fluids* 16, 3850 (2004) — the abstract speaks of
  "solutions of the diffusion equation for vorticity that account for the
  translational motion of fluid particles" (the dipole's self-propulsion, a
  weakly nonlinear correction — not a displacement map).
* **The Hermite (multipole) expansion of diffusing vorticity** — every
  angular mode of the heat kernel's derivatives decays self-similarly; the
  modes ∂_z^m of the Gaussian are exactly our ω_m: Th. Gallay & C. E. Wayne
  (2002, 2005) on the long-time asymptotics of the 2-D vorticity equation;
  D. Uminsky, C. E. Wayne & A. Barbaro, "A multi-moment vortex method for 2D
  viscous fluids", *J. Comput. Phys.* (2012), <https://arxiv.org/abs/1010.2475>
  (quadrupole perturbations of the Lamb–Oseen vortex, shear-diffusion
  relaxation). Background also in R. C. Kloosterziel, "On the large-time
  asymptotics of the diffusing vortex with a given circulation", *J. Fluid
  Mech.* 215 (1990) — cited from memory, not consulted for this check.
* Checked and NOT relevant: F. Lam, "Viscous flow regimes in unit square, Part
  4: vorticity dynamics from monopoles to multipoles" (arXiv:1808.07328,
  2018) — numerical, no closed forms, no displacement.

So: the **velocity** fields ψ_m ∝ γ_m(s) sin(mθ)/r^m of the viscous
multipoles are known physics (the Stokes-approximation multipoles; the
Hermite modes of the diffusing vortex). We derive them again in
`tools/multipole_verify.py` §2 only to check our constants.

## What is Jaffer's (the method)

* A. G. Jaffer, "The Lamb–Oseen Vortex and Paint Marbling", arXiv:1810.04646
  (2018), <https://arxiv.org/abs/1810.04646>: the displacement pattern of the
  DECAYING Lamb–Oseen vortex integrated over time into a closed-form
  expression, for marbling, "orders of magnitude faster than finite-element
  methods and without the accumulation of errors". The move the burst
  repeats: integrate the viscous flow in time into one displacement map.
* A. G. Jaffer, "Oseen Flow in Paint Marbling", arXiv:1702.02106 (2017) — the
  stroke; the engine's wake lineage.

## What we did not find

The **time-integrated displacement** Ψ_m = ∫ψ_m dτ ∝ r^{2−m} Φ_m(S) with
Φ_m(S) = ∫_S^∞ γ_m(s)/s² ds elementary for m ≥ 2 (and Φ_2 = χ = (1 − e^{−S})/S,
the same χ as the Stokeslet's), as a displacement map for marbling or
otherwise. Searches for the displacement form with its incomplete-gamma /
χ kernels returned nothing on point. It is a short integration by parts once
Jaffer's question is asked of the multipoles; we found no one who had asked
it that way, and we looked no further than a web search.

## The wording rule that follows

The docs say "method after Jaffer, extended here to m ≥ 2" and "we did not
find it stated in this form"; never "new", "first" or "discovered". The
physics of the velocity fields is credited as classical; the integration is
credited to Jaffer's method; the elementary closing for m ≥ 2 is reported as
what we needed and what it is. The author's note (ROADMAP_5, the burst
page's input) already carries the claim-nothing sentence.

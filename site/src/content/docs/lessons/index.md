---
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

* Every `§n` is `docs/PROJECT_SPEC.md` — spec v6 since the fold of 2026-10-26:
  `§10.7`–`§10.10` Kessho, the harmony operators and the strobe, `§13.8`–`§13.14`
  the voices; `KESSHO §n` is the Kessho proposal, in git history
  (`git show 21ff974:_work/specs/medium_specs_v3.md`), its substance §10.7.
* `DECISIONS_10 #n` … `DECISIONS_15 #n` are Parts X–XV of
  `docs/DECISIONS.md`, the record of what shipped (the v3.0 arc's roadmap is
  its Part 10 of `docs/ROADMAP.md`).
* Gate numbers are the synth suite's (`tests/voxo_suzu_tests.cpp`); the
  canvas's conservation gates are the bench's `--soak <operator>`; the Kessho
  flatness gate is `--kessho-flat` (as measured, `DECISIONS_10 #4`).
* Where a lesson derives or interprets beyond what the record says (the wake's
  fold, the gong's Mathieu reading, the billiard's collision map), it says so.

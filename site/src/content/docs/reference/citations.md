---
title: Citations & acknowledgments
description: The papers midi-sink stands on — Jaffer's mathematical marbling and his two closed-form extensions that this project converged on independently — plus the classical fluid mechanics and the MPE specification.
---

## The happiest discovery

midi-sink set out to build Jaffer's mathematical marbling into an instrument,
then added two operators of its own: a **wake** behind the stylus and a
**Lamb–Oseen swirl** driven by per-note pressure. Both turned out to be
Aubrey Jaffer's own published extensions of his closed-form family, arrived at
here independently and found afterwards. The [wake](../../operators/wake/) and
[swirl](../../operators/swirl/) pages each carry their lineage note; the papers
are below. Details were checked against the publications while writing this
page, not recalled.

## Mathematical marbling

1. Shufang Lu, Aubrey Jaffer, Xiaogang Jin, Hanli Zhao and Xiaoyang Mao,
   **"Mathematical Marbling,"** *IEEE Computer Graphics and Applications*,
   vol. 32, no. 6, pp. 26–35, 2012.
   [doi:10.1109/MCG.2011.51](https://doi.org/10.1109/MCG.2011.51) ·
   [author page](http://www.cad.zju.edu.cn/home/jin/cga2012/cga2012.htm).
   The closed-form drop, tine and exponential vortex; the inverse-lookup
   framework this whole engine is.
2. Aubrey Jaffer, **"Oseen Flow in Paint Marbling,"** arXiv:1702.02106, 2017.
   [arxiv.org/abs/1702.02106](https://arxiv.org/abs/1702.02106) ·
   [stroke.pdf](https://people.csail.mit.edu/jaffer/Marbling/stroke.pdf).
   An exact velocity field for Oseen (slow viscous) flow past a stylus and its
   use as a short-stroke marbling homeomorphism — the wake's lineage.
3. Aubrey G. Jaffer, **"The Lamb–Oseen Vortex and Paint Marbling,"**
   arXiv:1810.04646, 2018.
   [arxiv.org/abs/1810.04646](https://arxiv.org/abs/1810.04646) ·
   [vortex.pdf](https://people.csail.mit.edu/jaffer/Marbling/vortex.pdf).
   The closed-form displacement pattern of a decaying Lamb–Oseen vortex — the
   swirl's own paper (and the observation that real vortices are rare in
   marbling practice, because the Reynolds numbers needed want a far thinner
   size than marblers use).
4. Aubrey Jaffer, **Mathematical Marbling** — the marbling pages at MIT CSAIL:
   [people.csail.mit.edu/jaffer/Marbling/](https://people.csail.mit.edu/jaffer/Marbling/).
   The mathematics, serpentine and bouquet patterns, dropping paint, transfer
   effects, and *Pigment Transport in Paint Marbling*
   ([transfer.pdf](https://people.csail.mit.edu/jaffer/Marbling/transfer.pdf)),
   which analyses the Spanish wave and the Turkish moiré. The three
   animations there (*Bouquet*, *Latte*, *Wave*) are the ones the
   [gallery's](../../gallery/) tribute performance answers.

## Classical fluid mechanics

5. Horace Lamb, ***Hydrodynamics***, 6th edition, Cambridge University Press,
   1932 (reprinted unaltered by Dover Publications, New York, 1945; Cambridge
   Mathematical Library, 1993). The two-dimensional doublet behind the wake
   and the vortex motion behind the swirl.
6. William John Macquorn Rankine, ***A Manual of Applied Mechanics***,
   London: Charles Griffin, 1858. The combined vortex — rigid core, $1/r$
   velocity outside — that the [Rankine profile](../../operators/vortex/)
   rotates by.

## MIDI

7. MIDI Manufacturers Association and Association of Musical Electronics
   Industry, **MIDI Polyphonic Expression (MPE) Specification**, version 1.1
   (document M1-100-UM), 14 April 2022; supersedes version 1.0 (RP-053, 2018).
   [midi.org/mpe-midi-polyphonic-expression](https://midi.org/mpe-midi-polyphonic-expression).
   Zones, the MPE Configuration Message (RPN 6), per-note channels, the
   default bend ranges — everything the [implementation chart](../midi-chart/)
   declares.

## Software midi-sink is built with

* [sokol_gfx](https://github.com/floooh/sokol) and
  [sokol-shdc](https://github.com/floooh/sokol-tools) — Andre Weissflog's
  single-header graphics layer and shader cross-compiler: one shader source,
  Metal / HLSL / GLSL / GLES / WGSL.
* [GLFW](https://www.glfw.org/), [Dear ImGui](https://github.com/ocornut/imgui)
  and [libremidi](https://github.com/celtera/libremidi) in the desktop app;
  [glm](https://github.com/g-truc/glm) inside the core.
* [Emscripten](https://emscripten.org/) with Dawn's `emdawnwebgpu` port for
  the web build; [lil-gui](https://lil-gui.georgealways.com/) for its
  settings panel.
* This site: [Astro Starlight](https://starlight.astro.build/) and
  [KaTeX](https://katex.org/).

## Thanks

To Professor Jaffer, for a family of equations generous enough to become an
instrument, and for publishing the two extensions we thought were ours. To the
ROLI, Expressive E, Roland and Odisei engineers whose controllers shaped the
input landscape. And to the marblers of Turkey and Japan, whose craft this is
a small, electric homage to.

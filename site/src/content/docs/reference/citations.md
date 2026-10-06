---
title: Citations & acknowledgments
description: The papers midi-sink stands on — Jaffer's mathematical marbling and his two closed-form extensions that this project converged on independently, the viscous multipoles behind the burst, Chladni, Taylor–Green, Chirikov and Greene, Aref and Ottino — plus the classical fluid mechanics, the acoustics behind Suzu, the MPE specification and the Decent Sampler format.
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

## The electric medium's operators

7. Ernst Florens Friedrich Chladni, ***Entdeckungen über die Theorie des
   Klanges***, Leipzig, 1787. The figures: sand gathering on the nodal lines
   of a bowed plate — the picture the [Chladni operator](../../operators/chladni/)
   draws by stretching instead of gathering.
8. Geoffrey Ingram Taylor and Albert Edward Green, **"Mechanism of the
   production of small eddies from large ones,"** *Proceedings of the Royal
   Society A*, vol. 158, pp. 499–521, 1937. The cellular vortex whose stream
   function is the plate's mode shape — the two-wave Chladni gesture, split
   into two exact shears.
9. Hassan Aref, **"Stirring by chaotic advection,"** *Journal of Fluid
   Mechanics*, vol. 143, pp. 1–21, 1984; Julio M. Ottino, ***The Kinematics
   of Mixing: Stretching, Chaos, and Transport***, Cambridge University
   Press, 1989. Why iterating an exact flow winds the ink along its
   separatrices.
10. Boris V. Chirikov, **"A universal instability of many-dimensional
    oscillator systems,"** *Physics Reports*, vol. 52, no. 5, pp. 263–379,
    1979 (the standard map; its first statement in his 1969 Novosibirsk
    preprint). The [Chirikov operator](../../operators/chirikov/) and
    Suzu's [kicked rotor](../../suzu/strings-and-chaos/#the-kicked-rotor).
11. John M. Greene, **"A method for determining a stochastic transition,"**
    *Journal of Mathematical Physics*, vol. 20, pp. 1183–1201, 1979. The
    residue method and the threshold $K_c = 0.971635$.
12. Jürgen Moser, **"On invariant curves of area-preserving mappings of an
    annulus,"** *Nachrichten der Akademie der Wissenschaften in Göttingen,
    Mathematisch-Physikalische Klasse*, pp. 1–20, 1962. The invariant curves
    that survive below the threshold — the KAM theorem's map-side statement,
    the reason the ink slides along sheets before it shreds.
13. James D. Meiss, **"Symplectic maps, variational principles, and
    transport,"** *Reviews of Modern Physics*, vol. 64, no. 3, pp. 795–848,
    1992. The island chains and the transport the canvas shows past the
    threshold.
14. S. I. Voropayev and Y. D. Afanasyev, ***Vortex Structures in a
    Stratified Fluid: Order from Chaos***, Chapman & Hall, 1994; A. T. Chan
    and A. T. Chwang, **"Unsteady singularities of Stokes' flows in two
    dimensions,"** *International Journal of Engineering Science*, 1995;
    Thierry Gallay and C. Eugene Wayne on the long-time asymptotics of the
    two-dimensional vorticity equation (2002, 2005). The viscous multipoles
    — the velocity fields the [burst](../../operators/burst/) integrates in
    time by Jaffer's method. The literature check behind the burst page's
    wording was recorded before the page was written: the time-integrated
    displacement, elementary for $m \ge 2$, was not found stated in that
    form, and the page claims nothing beyond what it needed.

## The synth's acoustics

15. Arthur H. Benade, ***Fundamentals of Musical Acoustics***, Oxford
    University Press, 1976 (Dover, 1990). The truncated cone with a
    mouthpiece holding the missing apex's volume — the rule that put the
    [saxophone's](../../suzu/winds/) peaks back on the integers.
16. Neville H. Fletcher and Thomas D. Rossing, ***The Physics of Musical
    Instruments***, 2nd edition, Springer, 1998. The jet-edge flute, the
    reed and the lips as pressure-controlled valves, the flared bore's
    inharmonic peaks.
17. Kevin Karplus and Alex Strong, **"Digital synthesis of plucked-string
    and drum timbres,"** *Computer Music Journal*, vol. 7, no. 2, pp. 43–55,
    1983. The averaging loss and the round-trip law the
    [hybrid string](../../suzu/strings-and-chaos/) declares; the modal
    pluck in closed form is its spectrum.
18. Hal Chamberlin, ***Musical Applications of Microprocessors***, Hayden,
    1980 (2nd edition 1985). The state-variable filter that is Suzu's one
    declared dissipation in the cell.
19. John W. Gordon and Julius O. Smith, **"A sine generation algorithm for
    VLSI applications,"** *Proceedings of the International Computer Music
    Conference*, pp. 165–168, 1985. The magic-circle oscillator — two shears
    per sample, $\det = 1$, exact in pitch — that is Suzu's
    [cell](../../suzu/cells/).
20. Loup Verlet, **"Computer 'experiments' on classical fluids,"** *Physical
    Review*, vol. 159, pp. 98–103, 1967. The integrator of the chain and
    the bore.

## MIDI and the sampler's format

21. MIDI Manufacturers Association and Association of Musical Electronics
    Industry, **MIDI Polyphonic Expression (MPE) Specification**, version 1.1
    (document M1-100-UM), 14 April 2022; supersedes version 1.0 (RP-053, 2018).
    [midi.org/mpe-midi-polyphonic-expression](https://midi.org/mpe-midi-polyphonic-expression).
    Zones, the MPE Configuration Message (RPN 6), per-note channels, the
    default bend ranges — everything the [implementation chart](../midi-chart/)
    declares.
22. **Decent Sampler** by Dave Hilowitz (Decidedly Sound) — the
    `.dspreset` / `.dslibrary` format the sound engine's sampler reads:
    [decentsamples.com](https://www.decentsamples.com/). What midi-sink
    reads of it, and what it bundles, is on the
    [licensing page](../licensing/).

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
* In the sound engine: [miniaudio](https://miniaud.io/) (David Reid),
  [pugixml](https://pugixml.org/), [miniz](https://github.com/richgel999/miniz),
  and David Reid's dr_wav and dr_flac; the Dan Tranh demo from the
  [Versilian Community Sample Library](https://vis.versilstudios.com/vcsl.html)
  (CC0).
* This site: [Astro Starlight](https://starlight.astro.build/) and
  [KaTeX](https://katex.org/).

## Thanks

To Professor Jaffer, first and always: for a family of equations generous
enough to become an instrument, for publishing the two extensions we
thought were ours, and for the method the burst only had to follow. To
Ichisuke Fujioka, the Kanazawa suminagashi master whose floating ink this
project watched before it wrote a line, and to the marblers of Turkey and
Japan, whose craft this is a small, electric homage to. To the ROLI,
Expressive E, Roland and Odisei engineers whose controllers shaped the
input landscape; to Dave Hilowitz for a sampler format open enough to read;
and to the Versilian Studios community for the zither that makes the first
sound.

# Part X — v3.0, Phase 11: Kessho and symplectic harmony (steps 71–75: the third medium, the Arnol'd lock, the braid, the chord wave, the Poincaré strobe — opened at the fold of the proposals, 2026-10-14)

Opened 2026-10-14, the day `v2.0.0` shipped (#1 the fold of the proposals, #2 the order); merged into `docs/DECISIONS.md` as Part X at the phase's fold. The spec is `docs/PROJECT_SPEC.md` (v5) with `_work/TO_PROJECT_SPEC.md` as the arc's draft, corrected per step; the roadmap `_work/ROADMAP_6.md`; the author's proposals `_work/specs/` (`KESSHO §n` means `medium_specs_v3.md`). `DECISIONS_10 #n` means this file; the next phase opens `_work/DECISIONS_11.md`, and so on, one file per phase as the v2 arc did.

1. **The v3.0 arc opens: the three proposals folded into `_work/ROADMAP_6.md`,
   the spec draft opened at the START of the arc, and what the fold corrected
   against the tree.** The author delivered `_work/specs/` on 2026-10-14 —
   `roadmap_voxo.md` (Phases 11–13: Suzu's deferred physics, four new
   voices, Voxo's Decent Sampler coverage; and a first draft of the
   carry-over and release phases), `roadmap_voxo_gate_delta.md` (its review
   fold), `road_map_medium_v3.md` (Phases 14–16: Kessho and symplectic
   harmony, the carry-overs, the release) and `medium_specs_v3.md` — with
   the word that version 3 has both features. The fold: one roadmap in the
   house form (working rules, the ABI block, the binding table, a step
   per section with its gates and its DONE clause, the deferred list, the
   open decisions), the gate delta's amendments applied in place and
   marked, the first draft's carry-over and release rows (its 85–87)
   superseded by the medium roadmap's Phases 14–16 (90–92), which carry
   the same items plus Kessho's; and `_work/TO_PROJECT_SPEC.md` opened
   now rather than at a phase's close — the proposals ARE the spec until
   a step corrects its subsection to what shipped, so the author
   transcribes a true text at the fold (the v2 arc wrote this file at each
   close and the specs left the tree; the proposals stay in `_work/specs/`
   until the author removes them). THE ORDER OF THE PHASES IS NOT DECIDED
   HERE: the proposals number Voxo-first and say they work in either
   order; the author asked for the fold's opinion (harmony-first; the
   roadmap's open decision 1 records it) and decides before step 71; the
   step numbers keep the proposals' numbering until then and renumber on
   the author's word. What the fold corrected against the tree, each a
   fact the proposals got wrong or left to the header: (a) the control
   dimensions — the proposal's `SUMI_CTL_POLARIZER_ANGLE` = 14 … `_ARNOLD_K`
   = 17 collide with the torsion's, the Chladni's, the spark's and the
   Chirikov's (14–19; `SUMI_CTL_COUNT` 20), so they are 20–23 and the count
   24, the chart's rows likewise; (b) the trace header is
   `trace/include/sumi_trace.h`, not `trace.h`, and the mode is a field of
   `sumi_trace_config_t` — the proposal's text says "mode 2" and its enum
   says 1: it is the SECOND mode, value `SUMI_TRACE_POINCARE` = 1; (c) the
   export API is a flags word (`SUMI_EXPORT_ANOD_ALPHA` = 1u is a bit), so
   one new flag, `SUMI_EXPORT_KESSHO_CAUSTIC` = 2u (Layer B alone over
   alpha); the proposal's `SUMI_EXPORT_KESSHO` is dropped — a Kessho
   export without the flag is the full composite, as an Anod export is;
   (d) the consonance curve's home is `core/src/midi_normalizer.cpp`, beside
   the voice table it needs — the proposal's `midi_processor.cpp` does not
   exist; (e) `press_mode` 3, `bend_mode` 5 and `slide_mode` 3 are the next
   free value of each (the proposals had them right); the two A/B modes
   are fields appended to `sumi_params_t` (`polarizer_mode`, `braid_mode`
   — the Step-33 pattern), not new enums alone; (f) `voice_kind` is a
   number with a comment in `voxo.h` (0–8 today), not the proposal's
   `VOXO_SUZU_TODA`-style constants: 9 the membrane, 10 the Toda ring, 11
   the Boris gyroscope, 12 the Dirac network, 13 the billiard, in the order
   the steps land, named constants if a step adds them; the Suzu params
   struct is `voxo_suzu_params_t` as the proposal says; `bow_mode` is new;
   (g) step 90's "(Linux box)" and step 91's "(Windows box)" are the Mac's:
   both are core and shared-UI work, authored on the desktop harness by
   the one-platform rule, the Linux box is out of commission and the
   Windows box runs the lane only; (h) Kessho's Layer A is served by the
   strain buffer of step 90, which lands AFTER step 85: the layer reads
   instantaneous σ first and the buffer feeds it unchanged; (i) the web's
   medium parameter is numeric (`?medium=1`, DECISIONS_9 #6), so step 92's
   `?medium=kessho` is `?medium=2`; (j) `voxo_fuzz`'s unit is seconds
   (`--seconds`), not the proposals' "25 M iterations"; (k) step 75's Scala
   loader is Voxo's (the sound follows the table) — the layouts'
   microtonal mapping (§14.6) is a core item and stays carried, or rides
   with step 91; (l) the SoA SIMD refactor of step 71 meets the web gate's
   bit-for-bit contract (`suzu_web_gate.mjs` against the native
   reference): a working rule now says the contract is restated before the
   refactor lands — the scalar path as the reference build, or a tolerance
   gate — recorded, not discovered; (m) "Voxo 2.0" is the sampler's
   format coverage, not the library's version, which follows its ABI
   (0.16.1 today) — the phase is named "Voxo DS 2.0" and the author names
   the library; (n) the braid's pass count is gated against the pass
   budget's table (`sumi_core.h`, DECISIONS_5), the proposal's "≤ 64" being
   its reading of it; (o) the release split of `road_map_medium_v3.md`
   §5.1 (3.0 = harmony alone, the Voxo phases as 3.x) is superseded by the
   author's framing — both features in 3.0, one release — and survives
   only as the order question; (p) the tag `3.0.0-alpha.kessho` passes the
   spine (`X.Y.Z[-pre]`, any suffix); the v2 pattern was `-alpha.N`; the
   author's naming; (q) the decisions files are one per phase as in v2
   (`DECISIONS_10` … `_15`, Parts X–XV), the proposal's "`DECISIONS_10` for
   Phases 11–15" read as the first of them; (r) `site/scripts/build-notes.mjs`
   is extended to render this file in flight — its loop to 10 and its
   path-trim regex to two-digit numbers (`_work/DECISIONS_10.md` was not
   matched by `DECISIONS_\d`). The gate delta's own corrections (the
   billiard's det J = +1 in 4D, the Dirac step Q-orthogonal, the Toda ring
   near-integrable, the Boris voice volume-preserving with both prewarps,
   the membrane's CFL at E_max and its h² scaling, the bore shock
   dissipative, the oversampler a prerequisite) are applied as the
   author's review, not re-decided. Nothing in the tree changed for this
   entry beyond the three `_work/` files, the notes renderer and
   CLAUDE.md; no step has started.

2. **The order of the phases: harmony first, on the author's word.** Asked
   whether to start the Kessho phase before the sampler's (the proposals'
   14 before 13), the fold recommended further — Kessho and harmony before
   everything (the reply of 2026-10-14): the riskiest, taste-decided work
   (two A/B viewings, a Newton kick in the shader, the caustic) gets the
   arc's slack; the core reopens once, early, and freezes for the eleven
   Voxo steps (step 91's Boehm layout the one later touch); the Poincaré
   strobe becomes the instrument every new voice is seen through at its
   own step rather than retrofitted; the one cross-tie — the bus plate
   struck at the canvas — needs nothing from Kessho. The author: "no
   particular order is preferred for me, so I will listen to you — we can
   start with 14, then the others". RESOLVED: Phase 11 = Kessho and
   symplectic harmony (steps 71–75: the composite, the Arnol'd lock, the
   braid, the chord wave, the strobe), Phase 12 = Suzu foundation
   (76–80), Phase 13 = the new voices (81–84), Phase 14 = Voxo DS 2.0
   (85–89), Phase 15 = the carry-overs (90–91), Phase 16 = the release
   (92) — the proposals' other phases in their own order. The
   renumbering is mechanical and final (`_work/ROADMAP_6.md`,
   `_work/TO_PROJECT_SPEC.md`; no evidence existed); its consequences in
   the roadmap: the Boris gyroscope's spirograph is verified in Kessho
   (the gate delta's 77c, now step 82's), each Phase-13 voice carries a
   strobe-portrait gate, the membrane's bus plate (77) is seen as well as
   heard from its first session, this file is Phase 11's and the next
   phase opens `_work/DECISIONS_11.md`. The release split of
   `road_map_medium_v3.md` §5.1 is not taken: both features ship in 3.0.
   Next: step 71, the Kessho composite, on the Mac.

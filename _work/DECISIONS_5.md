# DECISIONS_5 — Phase 6: Medium (the Anod operators, the ABI event, palettes and presets)

Ambiguities resolved during Phase 6 (steps 35–46 of `_work/ROADMAP_5.md`).
Prior history: `docs/DECISIONS.md` (Parts I–IV; references written as
`DECISIONS_4 #n` mean Part IV). This file merges into that document as Part V
when the phase ships. The specs are the `specs/` set (`MEDIUM §n`,
`INSTRUMENT §n`, `SOUND §n`, `QOL §n`); where an entry here and a spec
conflict, the entry is the record of what shipped — and the conflict is
flagged to the author, who owns the specs.

## Step 35 — Phase opening & the conservation gate (macOS)

1. **The medium is named Anod.** The author's decision of 2026-09-21, closing
   the `MEDIUM §1` iterate point (candidates were Anod, Biri, Ichifu).
   Ichisuke Fujioka is honoured in the documentation's acknowledgments
   regardless of the name — an homage sits better there than carried by a
   product name (the spec's own suggestion). The ABI value is
   `SUMI_MEDIUM_ANOD = 1`, landing in step 41.

2. **Four phases, one public release, version 2.0.0.** The `specs/` set is
   four programs (medium, sound, instruments, publish) and the author split
   them into Phases 6–9 in that order; only Phase 9 releases to the public
   channels. Each earlier phase ends with a pre-release tag
   (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a
   pre-release and keeps the lanes proven) that the author installs on every
   device; Phase 9 runs `v2.0.0-rc.N`. Nothing reaches a stable channel
   before step 66.

3. **The ABI break is one step (41) and carries the probe's state argument
   two phases early.** `sumi_layout_probe` gains INSTRUMENT §1's
   `const sumi_layout_state_t*` and `sumi_cell_info_t` gains `flags` in the
   same bump that adds `medium` and `sumi_set_palette`, although no stateful
   layout uses them until Phase 8 (shells pass zeros). One break, then
   additive growth only — `libsumi` is **1.0.0** from that step, the settled
   ABI the deferred SDK needs.

4. **Operator families for the Anod set.** Wave torsion is a third vortex
   PROFILE in the core (the vortex passes already rotate by a profile of r)
   with its own page in the operator book — the `MEDIUM §2.1` iterate point
   closed as "profile in the core, standalone in the book". Chladni and the
   spark shear are passes of the ripple's shear family at both insertion
   points (live and bake). The burst lives in the wake's sub-stepped family
   (`core/src/displacement.cpp`).

5. **Fingering CCs (Phase 8, decided now so the chart and the specs can
   settle).** Valves CC 110 / 111 / 112 (≥ 64 = pressed) on the MASTER
   channel — global state, recorded where a DAW records the mod wheel. The
   slide is 7-bit **CC 113 with normalizer smoothing**, not a 14-bit pair:
   the pair's MSB would land in CC 0–31, the Airwave's block (DECISIONS_4
   #50), and 128 steps over six semitones is ≈ 4.7 cents per step, under the
   ~5-cent just-noticeable difference — the arithmetic is the justification.
   CC 102–119 are undefined in the MIDI specification and already carry
   midi-sink's own controls (102/103 ripple).

6. **Presets share one host-side serializer** (pure C, beside `hostmpe`, step
   43), so the four shells cannot drift; the core stays stateless about
   files.

7. **Per-device default presets are OFFERED, never auto-applied** (`QOL §6`
   iterate point).

8. **Replay files record gestures too** (`QOL §5` iterate point: yes), so pen
   performances replay complete, and they carry FRAME BOUNDARIES: playback
   drives the scripted clock through them (step 61). Re-bucketing recorded
   bytes by wall time is the documented anti-pattern and the negative test.

9. **New-feature documentation lands in step 63 and merges with the release
   tag.** The live `/marble/` is the newest stable tag (DECISIONS_4 #82), so
   an operator page merged early would embed a scene the released wasm does
   not know. Guide fixes ship from `main` at any time; pages for new
   operators, layouts and Voxo are drafted in the step's evidence folder.

10. **The palette curve is fixed per medium in 2.0** (`QOL §1`: the
    "advanced fold" is deferred); **the theremin is a `flags` bit** on the
    cell info, not a radius sentinel (`INSTRUMENT §5`).

11. **The fretboard generalises to `SUMI_LAYOUT_STRINGS`** with three FIXED
    tuning presets — standard guitar, a whole-tone tap grid, all-fourths — as
    a params enum over fixed arrays; user-editable tunings stay deferred with
    microtonal. Wicki–Hayden stays its own layout (not a string layout).
    **"Harpejji" is Marcodi's trademark:** the docs may say "inspired by
    tapping instruments such as the Harpejji"; the word never enters an enum,
    a setting label or a product name.

12. **The phase invariant.** `tests/fixtures/field_512_metal.bin` stays
    bitwise on Metal from step 35 to step 66: new operators add passes, media
    change the composite, layouts change the probe — none touches an existing
    pass. A step that believes it must change the fixture stops and records
    the decision first.

13. **The four-part gate is one `--dev` command per operator, and its (b)
    is measured per class.** `midi-sink --dev --soak <operator|all>` (in
    `desktop/src/dev_tools.cpp`, beside the step-19 pinch soak it generalises)
    drives the nine v1.0 passes through their REAL routes on the fixed 512²
    scripted clock — the glide tine by note bend, the two pinches by CC 74
    under `slide_mode = 1`, the wakes as stylus segments of ≤ a/4 per frame,
    the ripple bake by its amplitude CC, the swirl by channel pressure under
    `press_mode = 1`, the two vortex profiles by the mod wheel — with one
    stream shape for all (a 0.5 Hz wobble at 120 Hz, at most one pass per
    frame) and prints four verdicts plus a machine-readable `SUMMARY` line
    that `tools/soak_report.py` tabulates. **(a)** is the class declaration
    (`MEDIUM §2`) with its one-line det J = 1 argument. **(b) for EXACT
    operators:** 500 strong (+k, −k) gesture pairs at one centre hold ink
    mass (Σ phase) within **±5%** AND return the pre-image to within **4
    texels** mean displacement — mass is the coarse guard (a broken pair
    loses everything: the negative control reads −99.7%) and the pre-image
    return is the sharp one, the observable that tells a non-inverting pair
    from two different area-preserving passes (the negative control leaves it
    204 texels away; the v1 references sit at 1.5–2.7). **(b) for SUB-STEPPED fields:** the class
    promises first-order area preservation per step, so that is measured —
    one a/4 sub-step on an identity field, pre-image Jacobian det > 0.5
    everywhere outside the swept capsule and mean within 2·10⁻³ of 1 (the
    Stokeslet test's own bar, DECISIONS_4 #53; the capsule is excluded as in
    the flick test because the doublet's slip surface is a genuine tangential
    discontinuity, not a fold — measured across it the doublet reads −1.44,
    outside it 0.73). The ±D pair drift is printed beside it, not gated:
    one-sub-step pairs already lose 4.7% over 500 pairs because every step
    resamples the slip surface, and a long stroke per pair would be forty
    passes against the exact operators' two. **(c)** the stream never grows
    mass by more than 0.5%. **(d)** per-pass mass loss ≤ 2× the glide-tine
    control under the identical stream shape, from a FRESH copy of the same
    scene (erosion is front-loaded — sharp structure fades fastest — so a
    control run on the field the operator already worked would inflate the
    baseline), gated only at ≥ 3000 passes (default 6000, the #33 window)
    because of #15 below; a shorter run prints (d) as information.

14. **The (c) negative control uses the v1 tine's clamp legacy, not a core
    switch.** The roadmap asked for "the pinch soak with the ingress mask
    disabled"; the mask lives in the core and the core is frozen, and a debug
    switch would be a core change for a test. The pinch's pre-ingress
    fabrication (+9.5% over 12 000 passes) stays history's red run (#33); the
    gate's own red uses the mechanism #33 kept fixture-pinned in the v1
    operators: ink laid on the left edge and tines dragging it inward
    duplicate the clamped edge column every pass — **+14.0% in 300 passes**.
    The other two reds: a (+k, −k) pinch pair whose −k sits at an offset
    centre (204 texels, −99.7%); an over-stepped wake stream — a stroke of
    fifteen tip radii every frame, fifteen internal sub-steps, over the full
    6000 window — for (d), which is the class rule (≤ a/4 PER FRAME at
    gesture rate) stated as a failure. A chaotic pinch schedule was tried
    first and dropped: its erosion is front-loaded and dilutes below 2× over
    any window long enough for the control to be meaningful (1.4× over 3000
    passes at k = 0.3). `--soak-negative` asserts that each part fails.
    Flagged against the roadmap's literal wording.

15. **The resampled medium is not mass-neutral, in both directions, and the
    (b) mass bar is ±5%, not #33's ±0.5%.** Measured while calibrating, on
    Metal: the incumbent tine's strong ±pairs (z = 0.05, ~25 texels at the
    line) GAIN mass steadily after the first ~200 pairs, +0.76% at 500 — all
    of it interior (the 16-texel edge band contributes exactly zero, so this
    is not the clamp legacy): 6 900 units leave the initially inked texels
    and 7 550 appear in the water around them, a net gain of 650 at the
    ink/water boundaries under bilinear gather. The gain scales with the
    boundary length a pass moves: the ripple bake, an exact whole-canvas
    shear of only 12.8 texels at full amplitude (it carries the ingress rule,
    so this is not clamp either), gains +2.69% over the same 500 pairs while
    its pre-image returns to 2.6 texels — every ring edge on the canvas moves
    on every pass. The pinch pairs, exact by construction (#33), drift
    −0.43%/+0.15% with a 2.3-texel pre-image wander; the swirl and both
    vortex profiles stay within ±0.5% (1.5–2.7 texels). A FRESHLY laid drop gains mass over its
    first few hundred glide passes (−1.2·10⁻⁵/pass "erosion" at 300 passes,
    i.e. growth) before the steady fade of ~1.2·10⁻⁵/pass sets in; #33's
    control never saw this because it ran on a field aged by 6000 pinch
    passes. So: the exact-class mass window is ±5% — it has to clear a
    whole-canvas exact shear, and Chladni, the spark and Chirikov are all of
    that family (the references: tine +0.76%, ripple +2.69%, a broken pair
    −99.7%); the pre-image bar is 4 texels (references at 1.5–2.7) and is
    the criterion that actually decides; (d) is gated only in the steady
    window. The
    mechanism behind the gain is not established here — the gather of a
    det = 1 map conserves Σ in exact arithmetic, so it is in the filtering
    precision or the half-float representation of the phase — and it is
    recorded as a Phase-6 question, not chased in this step: the gate needs
    the medium's floor, not its explanation. Flagged against the roadmap's
    ±0.5%.

16. **The ripple soaks through its CC route.** The bend-driven bake drifts
    the ripple phase a little per pass ON PURPOSE (#36 permanence: an
    excursion never retraces exactly, so vibrato leaves a mark), which makes
    bend-driven ±pairs non-inverting by design. The amplitude CC (102,
    `bend_mode = 0`, `ripple_bake = 1`) keeps φ fixed — the composing-back
    group the operator's exactness claim is about — so both the pairs and the
    stream run through it. A future operator whose route drifts a parameter
    by design has the same choice to make, and this entry is the precedent.

17. **The crossed-tine pinch is exact per pass but NOT sign-reversible
    through its ABI, and it fabricates under torture — recorded red in the
    baseline table, flagged as a Phase-6 fix candidate.** The gate's first
    full run: `pinch-cross` (b) leaves the pre-image 92 texels adrift and
    GAINS 25% of its mass over 500 ±k pairs, while the saddle passes. The
    reason is in `sumi_deform_crossed_pinch` (`core/src/displacement.cpp`):
    the variant is a composition of two perpendicular infinite-line tines
    T₁∘T₀, and a negative k reverses BOTH drags but keeps the ORDER — the
    inverse of T₁∘T₀ is T₀⁻¹∘T₁⁻¹, and crossed shears do not commute, so the
    sign flip leaves a second-order kick-drift residual every pair (the same
    order-matters fact `MEDIUM §2.2` states for Chladni). That residual
    walks ink to the canvas edges, and the tines carry the v1 edge-clamp
    legacy (#33 kept it fixture-pinned) — hence the fabrication. Two
    consequences: DECISIONS_4 #69's "a squeeze-and-release nets out in exact
    math" holds for the saddle only; and the fix — emit the tines in reversed
    order for k < 0 — is a one-line core change that would make release
    retrace exactly, which the author must weigh against the look (#69 also
    calls what the release does not retrace "marbling"). Not touched in this
    step (core frozen; the gate needs the baseline, not the fix); the (c)
    and (d) columns are green for the variant. The gate now also asserts that
    the stream MOVED the field (> 100 texels changed) inside (c), the
    step-19 soak's own sanity check, so a mis-wired route can never pass
    trivially.

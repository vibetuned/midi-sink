# DECISIONS_7 — Phase 8: Instruments (the displacement field, then the layouts)

Ambiguities resolved during Phase 8 (steps 55b–62 of `_work/ROADMAP_5.md`).
Prior history: `docs/DECISIONS.md` (Parts I–VI; references written as
`DECISIONS_5 #n` / `DECISIONS_6 #n` mean Parts V / VI). This file merges into
that document as Part VII when the phase ships. The specs are
`docs/PROJECT_SPEC.md` (`SPEC §n`; its §10–§12 drafted in
`specs/TO_PROJECT_SPEC.md`), `specs/INSTRUMENT_SPEC.md` (`INSTRUMENT §n`) and
`specs/QUALITY_OF_LIFE_SPEC.md` (`QOL §n`); where an entry here and a spec
conflict, the entry is the record of what shipped — and the conflict is
flagged to the author, who owns the specs. The roadmap's Phase-8 header
points the author's fingering confirmation at "`DECISIONS_7 #1`"; step 55b
came first, so that confirmation is the first entry of step 56.

## Step 55b — The field as displacement (the Mac)

1. **The field stores each texel's DISPLACEMENT, not its pre-image; the
   fixture moved once, by construction; the Sumi print did not move at
   all.** The payload of `tex_A`/`tex_B` (SPEC §4.1–§4.2) is now
   (u − x, v − y, ink, aux): the pre-image coordinate minus the texel's own,
   then the phase and the selector as before. Every pass computes its
   inverse lookup P_src from its own texel exactly as it did, reads the
   field there and re-bases what it read — d′(P) = d(P_src) + (P_src − P),
   which is u(P_src) − P, the same pre-image the absolute payload carried —
   through ONE helper (`sumi_fetch` in `deform.glsl`, a shared `@block`);
   the identity, the ingress rule and a drop's interior write zero. Bilinear
   filtering commutes with the change (the weights interpolate the texel
   centres exactly), so the map is the same map to rounding — and the
   rounding is the point: the half-float quantum of a stored value follows
   the displacement (2^-24 canvas heights at rest, 2^-15 at a sixteenth of
   the canvas, 2^-11 only for content that has travelled half the sheet)
   where the coordinates carried 2^-11 over the outer half of the sheet
   whatever the motion. THE ONE SEMANTIC TRAP, found by the fixture: the
   drop, the tine, the vortex and the swirl have no ingress branch and lean
   on the sampler's clamp-to-edge for a source past the edge, which under
   the absolute payload meant "the edge texel's pre-image"; re-basing with
   the raw P_src would instead extrapolate the edge's displacement off the
   canvas (a 0.09-canvas difference in the corners of the canonical script,
   a different map at the rim). So the helper re-bases with the coordinate
   the sampler actually read — P_src clamped to the edge texel centres,
   [½/W, 1 − ½/W] per axis, from `textureSize` — and the value is the 1.1.0
   value exactly. THE INDEPENDENCE that makes the fold cheap: no pass reads
   the coordinates to decide where to sample, so the ink and aux channels
   never depended on how the coordinates were stored — the re-captured
   `field_512_metal.bin` is bitwise the old fixture in ink and aux
   (0 of 262,144 texels differ in either), its pre-images agree with the old
   ones to 1.6e-3 at worst (seven passes of the OLD rounding, 2^-12–2^-11
   each) and to 1e-4 over the bulk, and the composite fixture
   `composite_512_metal.rgba` — a Sumi print, which reads only ink and aux —
   is bitwise untouched, as the roadmap promised. What reads the
   coordinates: the Anod composite (the strain estimator now rebuilds J =
   I + ∇d from the displacement, the fresh class is "no phase and under half
   a texel" with the ULP term gone, the rounding bias is taken at the
   displacement's magnitude, the water's grid reads the payload directly),
   the harness (its reader rebuilds u = dx + (x + ½)/W once in float32, so
   every check keeps reasoning in pre-images; the raw reader and the dumps
   keep the stored payload), the two gate tools (channel names dx/dy, the
   tolerances carry: a difference in dx is the same difference in u) and
   the print ledgers, which keep a field only within a session (no shell
   persists one) — so nothing converts and `sumi_read_field` /
   `sumi_export_begin` simply hand the new payload over; libsumi is 1.2.0
   for that change of bytes (no signature moved). MEASURED on the Mac:
   the field gate green on Metal against the new fixture with its negative
   control, a second dump bitwise; the composite gate bitwise; every harness
   self-test green (the 17 suites, 86 checks) with no threshold touched; the
   Anod four of the palette test moved (they read the strain off a field 60×
   finer) and Metal's column is recaptured with this entry's evidence — the
   GL and D3D11 columns and tiers are the boxes' to re-measure (the handoff);
   THE WEB TIER against the new fixture: max 7.6e-5 / 1.2e-4 in dx / dy,
   mean 3.9e-9 — under the old payload the same Chrome sat ~6e-4 from the
   Apple fixture "through half-float rounding alone" (DECISIONS_4 #20); the
   rounding that separated the backends was the coordinates'. SPEC
   §4.1–§4.2's "(u, v, ink, aux)" and "initial state: identity coordinates"
   are the author's to transcribe (`specs/TO_PROJECT_SPEC.md`, the pointers);
   §4.6's regression test is unchanged in kind. The phase invariant restarts
   from this fixture (DECISIONS_5 #12, #87). Evidence:
   `docs/evidence/step55b/`.

2. **The emission floors, re-derived from the displacement's quantum: a
   quarter of the old step, kept as a pass economy — and the #61
   experiment repeated says the rounding-back they were built against is
   gone.** Three floors were the coordinate quantum's: the burst's and the
   spark's 5e-4 canvas heights (one 2^-11), the stir's 1e-3 (two). The
   displacement's spacing follows the displacement — 2^-24 at rest, 2^-15
   at a sixteenth of the canvas — so ONE quantum is defined for the floors:
   `SUMI_FIELD_QUANTUM` = 2^-13 = 1.22e-4, the spacing at a QUARTER canvas
   of displacement, which a stirred playable sheet does not exceed; a pass
   whose peak reaches it lands whole anywhere. The burst's and the spark's
   floors are one quantum, the stir's two (`voice_mapper.h`), each a
   quarter of what it was: a strike's tail deals four times as many, four
   times finer stages, the stir's clock on the small-disc layouts (the
   Jankó, the fifths — every 50–60 ms before) is now the frame. MEASURED,
   #61's test on the chromatic grid (150 frames of 0.0125 rad at 512²,
   the harness): under 1.1.0's coordinates with no floor the ring turned
   0.07 rad (nothing accumulated — #61); under the displacement with NO
   floor it turns **1.36 rad** and the ring's radial scatter |log r′/r|
   reads 0.05; with the old floor (a pass every third frame on this grid)
   1.45 rad and 0.03; with the new floor (a pass every frame here, as with
   none) 1.36 and 0.05. So the floors no longer protect the motion — they
   trade resampling scatter for temporal resolution: three times the
   passes cost six percent of the rotation and twice the ring's radial
   blur on this grid, the bilinear medium's erosion per pass summed. FOR
   THE AUTHOR: the roadmap asked for the re-derivation and "may go"; they
   could go to zero (the same numbers as the new floors on the grid, more
   passes on the tails) or back up to the old values purely as an erosion
   economy — the choice is now about the look and the GPU budget, not
   about correctness, and this entry holds the three measurements to make
   it with. Every episode-driven self-test (the burst, the spark, the
   torsion sweep, the gestures, the Anod prints) passes with no threshold
   touched; the canonical script has no episodes, so the fixtures and the
   palette hashes do not see the floors. Evidence:
   `docs/evidence/step55b/floors/`.

3. **The soak's pre-image tier was the coordinate payload's freeze; it is
   re-derived to 32 texels, and the (b) gate now measures what it says.**
   The first soak under the new payload went red on (b) for every exact
   operator — tine 11.9 texels of pre-image deviation after 500 (+k, −k)
   pairs against the 8.0 tier (was 1.6), pinch-saddle 13.2 (2.3), swirl
   12.1 (1.5), the vortices 14.2 / 16.1 (1.8 / 2.7), the torsion 16.4
   (5.5), the Chladni stir 22.1 (5.0), the spark shear 20.3 (3.6), the
   ripple bake 9.1 (2.6) — while the ink-mass half of (b) and all of (c)
   and (d) were BITWISE the Phase-6 numbers (the ink never read the
   coordinates). A growth-law hook (`--pair-drift <op>`: the deviation
   after 1, 2, 5 … 500 pairs and its 8×8 map) run on both payloads — the
   old from a worktree at 7ceb111 — says why: the single-pair residual is
   the same on both (the tine 0.194 vs 0.184 texel; the stir 2.18 vs 2.17)
   — the resampler's smoothing of the pre-image map, deterministic, the
   same sign every pair; under the coordinates it FROZE after ~50 pairs
   (0.54 at 10, 1.10 at 50, 1.37 at 100, 1.60 at 500; the map: 0.01 texel
   in the water, 5–9 in the ink) because a smoothing step of ~0.02 texel is
   under half a 2^-11 quantum and rounds back — DECISIONS_3 #37's
   "protective property", named then as such; under the displacement it
   accumulates as ~N^0.7 everywhere (0.77 at 10, 2.35 at 50, 3.84 at 100,
   6.28 at 200, 11.87 at 500; the map 6–23 texels, water and ink alike).
   The non-inverse control, the crossed pinch, grows linearly from its
   first pair (1.48, 8.2 at 10, 36.0 at 100, 72.9 at 500; the mass +25 %)
   on either payload. So the 8-texel tier — "eight keeps the oscillatory
   family green with a 25× margin" — was calibrated on frozen numbers and
   never measured the resampler; the roadmap's "the tiers re-measured" is
   read to include it. `SOAK_DEV_EXACT` = 32: 1.45× the worst exact
   operator, 0.44× the failure it must catch (`--soak-negative`'s
   negative-inversion control stays red at 73). FLAGGED for the author:
   `specs/TO_PROJECT_SPEC.md` §10.2 quotes the tier as "the pre-image
   within 8 texels over 500 strong pairs" — 32 from here, with this entry
   as the reason. And a consequence to carry: the pre-image map now erodes
   as honestly as the ink does under long stirs — an Anod charge's strain
   reading will fade with hundreds of resampling passes where the frozen
   coordinates held it — the same physics the ink always had, no longer
   masked. Evidence: `docs/evidence/step55b/drift/`, the two soak reports.

4. **The Tab: the Adreno's loss was the coordinates', and the six strikes
   now print alike on the Tab and the Mac.** The GLES field tier against
   the new Metal fixture (the `fieldDump` intent, 512²): dx max 1.1e-3,
   dy 1.2e-3, ink 1.5e-2, aux 4.9e-4, mean 7.9e-5 — under #88's
   coordinates the same Tab sat at max 1.51e-2 / mean 6.08e-4, so the
   coordinate channels are 14× closer to Metal and the mean 7.7× (the ink
   channel's 1.5e-2 at a rim texel is the Adreno's bilinear on the phase,
   as before; PASS at the mobile tier 2.5e-2 / 1e-3). THE JUDGEMENT the
   roadmap named: #88's frame-locked script (six velocity-100 MPE strikes
   18 frames apart at 1/120 s, 150 frames, the dip) at 1024×1024 through a
   temporary `--es strikes 1 [--ef sparkShear f]` intent on the Tab and the
   bench's `--anod-strike-render` on the Mac, both under the Tab's own
   session (`--preset`, new on the bench: the author's preset with
   anod_drop 0.33, no grid, the phosphor palette), the prints compared by
   lit blobs (`tab/strike_compare.py`, 60 levels over the glass, ≥ 200
   px): under the session's shear 0.6 the Mac keeps 6 charges of
   6 423 … 5 014 px (3.25 % lit) and the Tab 6 of 6 263 … 4 964 (3.18 %);
   under the harsh shear 2.0 of #88 — where the Tab had kept six with two
   thinned to 1 253 and 763 px against the desktop's 10–16 k — the Mac
   prints 6 of 8 986 … 5 301 (4.08 %) and the Tab 6 of 8 684 … 4 802
   (3.80 %): every charge within 10 %, none thinned. The strike's charge
   back at the thin proportions (#88's plan B undone) is the author's call
   to make with these prints (`mac/`, `tab/`); nothing in the core waits on
   it. THE THIN STRIKE ITSELF, run as the author asked ("change the tablet
   to the spark version to see if it really helps"): a lab switch
   (`sumi_debug_set_anod_strike_thin`, a mapper flag off by default, dev
   only — the bench's `--strike-thin`, the hook's `--ei strikeThin 1`)
   restores #71's composition — the small charge alone, the shear's band
   and kick on the Sumi radius, no burst — and the same script and session
   ran again on both machines. At the session's shear 0.6 the Mac prints 6
   charges of 9 600 … 4 256 px (4.24 % lit) and the Tab 6 of 7 651 … 2 680
   plus a 209-px splinter (3.31 %): the threads survive, the two thinnest
   at half the Mac's. At #88's harsh shear 2.0 — where the coordinates left
   the Tab TWO fragments of 151 and 127 px, 0.02 % lit, against the
   desktop's six at 1.93 % — the Mac prints 6 of 5 642 … 409 px (1.50 %)
   and the Tab 6 of 2 201 … 302 (0.68 %): six where there were two, 34×
   the lit share, at 45 % of the Mac's — the displacement field carries the
   thin strike's coordinates through the Adreno; what still thins it is the
   phase channel's own resampling on that GPU (the ink's, which no payload
   touches: the field tier's ink maximum, 1.5e-2, is the same as ever). So
   it helps, decisively at the author's shear and by a large factor at the
   harsh one, without making the Tab the Mac. The classic spark of #88
   stays the default; the switch stays in the core, documented, for the
   author to judge the thin strike by eye and restore it if they want (a
   one-line change in the mapper: the thin branch as the only branch). The
   hook is removed and the Tab carries the hook-free build, as after #88.
   Evidence: `docs/evidence/step55b/tab/` (the `*_thin` prints),
   `mac/session_thin*/`.

5. **The thin strike is the strike again — the author's call on #4's
   prints — and the lab switch goes.** "The thin strike as default." So
   the Anod strike is #71's composition once more: the charge at
   `anod_drop` × the Sumi radius (the default back to #71's 0.33 from
   #88's 0.57) and the spark shear episode with its band and kick on the
   SUMI radius, no burst — in the mapper's strike and in `sumi_gesture_tap`
   alike (#75's tap follows the strike); `ANOD_STRIKE_BURST_D` leaves the
   mapper a second time; `burst_order_by_class` stays in the ABI, unused,
   as #71 left it. #88's plan B — the classic spark on a bigger charge, so
   that a lossy renderer had thick streamers to keep — answered a loss the
   displacement payload has since removed (#4: six charges on the Tab
   under both shears where the coordinates left two specks). The switch
   that ran #4's experiment (`sumi_debug_set_anod_strike_thin`, the
   bench's `--strike-thin`) is removed: the tree carries one strike, the
   entry carries the measurements. Tests: `test_medium_binding_tables`
   expects no burst piece and the shear's band at twice the SUMI radius
   (a new check), the charge at 0.33; the bench's tap check reads "a third
   of the Sumi drop". The user's sessions keep whatever `anod_drop` they
   hold — the author's Tab already held 0.33. A LOOK, MEASURED SO THE PAYLOAD
   IS NOT BLAMED FOR IT: under the author's iPad session (shear 2.0, τ 0.5
   — the harsh preset of #88, with the thin strike) the six strikes leave
   ONE bright charge on the Mac; the pre-#88 bench at 76e2f5e, the same
   thin strike on the COORDINATE payload, built in a worktree and given the
   same session, leaves the same one (6 239 px against 5 829); at τ 0.25
   the old payload keeps three and the new four; at shear 0.6 both keep
   six. The tearing at that shear and time constant is the composition's
   own, on either payload — a setting to choose by eye (shear, τ), not a
   regression; the Tab's session (shear 0.6) prints all six. The author,
   with the build on the iPad: "they look as expected, this is working quite
   nicely." Evidence: `mac/ipad_session/`. FLAGGED for the spec:
   `specs/TO_PROJECT_SPEC.md` §10.5's strike row and §10.6's tap are
   rewritten to this; MEDIUM §4's strike row as #71 had it ("a small
   charge + the spark shear") is true again.

6. **The GL tier under the displacement payload, measured on the Linux box;
   the GL column's Anod four recaptured.** Step 55b's handoff, run at
   `6d1e034` (libsumi 1.2.0; ctest 9/9). The §4.6 field gate on GL (NVIDIA
   RTX 5090, driver 610.43, GNOME on Wayland) against the re-captured Metal
   fixture: **dx 2.44e-4, dy 3.66e-4, ink 3.91e-3, aux 0, mean 2.26e-6** —
   green at the unchanged 1e-2 / 1e-4 tier, where the coordinate payload
   sat at max 1.5e-2 / mean 6e-4 on the same box (DECISIONS_5 #44, #83):
   the mean fell some 270×, the max 4×, the same picture as the web tier's
   fall on the Mac (#1). The composite gate holds its tier (max diff 1, the
   8-bit step of #78/#83). The palette test's Sumi four matched without an
   edit; the Anod four moved as predicted and their hashes are the `gl`
   column now (`gate-55b/palette_hashes_gl.txt`: 7bc658dd87238d03,
   b3121824077f8525, bc0fc6df5b03f797, eb3e16793e8608db; 5/5 after). The
   seventeen self-tests pass. The soak at the 32-texel tier: 47 of 48, the
   crossed pinch's pair the known red (73.10 texels), the exact operators'
   pre-image deviation 8.11–21.98 texels, each a few tenths under Metal's;
   the ink-mass pairs bitwise this box's Phase-6 ones for burst, chladni and
   chladni-field, the spark's moved (−8.66 → −8.12 %) with its emission
   floor (#2). Two things learned about running the soak on this box: it
   steps frames through the bench window's swaps, so it runs at the display's
   pace (~7 min per operator under the step-55 pacer) and a bench window
   hidden behind another gets no Wayland frame callbacks and stalls — one
   window at a time, uncovered; and stdout block-buffered into a file lets
   stderr's lines splice a SUMMARY line (`stdbuf -oL` avoids it). The D3D11
   column waits for the Windows box. Evidence: `gate-55b/` at the repo root,
   as the handoff asked.

7. **The D3D11 tier under the displacement payload, measured on the Windows
   box; the D3D11 column's Anod four recaptured — and the bench's paper
   dip found to take a variable number of frames on D3D11, which leaves
   the Anod prints frame-dependent (a bench finding for the author, not
   fixed here).** Step 55b's handoff, run at `fd7a196` (libsumi 1.2.0;
   ctest 10/10, `abi_c_compile_static` the tenth). The §4.6 field gate on
   D3D11 (NVIDIA RTX 5090, driver 616.64, MSVC 14.44 Release) against the
   re-captured Metal fixture: **dx 2.44e-4, dy 3.66e-4, ink 3.91e-3, aux
   0, mean 2.26e-6** — green at the unchanged 1e-2 / 1e-4 tier. On this
   box the coordinate payload sat at max 3.9e-3 (the ink) / mean 6.85e-6
   (DECISIONS_5 #83 — the D3D11 field was bitwise Step 11's, not the
   1.5e-2 / 6e-4 of the GL box): the ink's maximum is the same 3.906e-3
   (ink is bitwise the old fixture, its D3D11 deviation unchanged), the
   mean fell 3× with the coordinates. The four channel maxima land on the
   SAME texels as the Linux box's GL run (#6: (359,228), (335,152)) and
   the means agree to four digits (2.2579e-6 here, 2.2582e-6 there) —
   the same silicon's arithmetic through two APIs. The composite gate
   holds its tier (max diff 1, 25 617 samples of 1 048 576 differ; GL:
   25 672). The palette test's Sumi four matched without an edit; the Anod
   four moved as predicted and the first run's hashes are the `d3d11`
   column now (`gate-55b/windows/palette_hashes_d3d11.txt`:
   9a3d35df09e0c37f, 1b72baa0345056a9, 1024f97d03e48f64,
   493b66a2e532a203). But the rerun after the rebuild said 4/5 with four
   NEW hashes, and 25 runs in all show each Anod case at its column value
   about four times in five and at one of two other values otherwise
   (`diag/`: p0 21 : 4, p1 20 : 5, p2 20 : 5, p1-morph 22 : 2 : 1), the
   cases deviating independently of one another; the Sumi four never
   moved. Traced (`diag/trace_palette_*.txt`, a temporary env-gated print
   since removed): the bench's `t19_dip_print` steps frames until
   `sumi_read_print` reports the dip's print, and on D3D11 that takes **5
   frames usually, 4 sometimes, 3 for a fresh instance** — the readback is
   a staging `CopyResource` polled with `Map(DO_NOT_WAIT)` (§5.3), so the
   frame it lands on is the GPU's timing. A Sumi scene does not care how
   many idle frames precede it; an Anod scene does (its state runs on the
   clock: the strain-glow's phase, the episodes), so a 4-frame dip before
   an Anod case prints a different picture. The recorded column is the
   5-frame outcome, the majority; a run that draws a 4 in an Anod case
   says `!= recorded` for that case. GL and Metal poll the same way (a
   fence at timeout 0, a completion handler) and happened to be constant
   on their boxes. The same variance reaches the gesture test: its Sumi
   check compares a scene after the FIRST dip (4 frames, the fresh
   instance's) with the same scene after a later one (5), and the two
   fields differ at 1–18 samples by a payload ULP (`FAIL … (1 samples
   differ)`; `diag/trace_gesture_*.txt`, five runs); its Anod-twist check
   differs at a constant 78 samples (row 0 and three texels near
   (254,33), up to 3e-3), the cause not pinned in the timebox — both
   D3D11-only, both 0 on the Linux box. **Proposed, not done:** the
   bench's dip helper should cost a fixed number of frames on every
   backend (step until ready, then pad to a constant), which makes the
   Anod prints and the gesture pairs deterministic wherever the readback
   is asynchronous — it would move the Metal and GL Anod columns too if
   their boxes' constant differs from the pad, so it is the author's edit
   with a Metal recapture, not this box's. The other fifteen self-tests
   pass with 0 `^FAIL`. The soak at the 32-texel tier: 47 of 48, the
   crossed pinch's pair the known red (73.10 texels), the exact
   operators' pre-image deviation 8.11–21.98 texels — **the table is the
   Linux box's to the second decimal in every (b) and (c) cell** (tine
   11.84, pinch-saddle 13.16, torsion 16.36, chladni 21.98, spark-shear
   20.21; the erosion rates differ in the last digit for four operators),
   the ink-mass pairs bitwise the Phase-6 ones for burst, chladni and
   chladni-field, the spark's −8.12 % with its floor (#2); the three
   negative controls red as required. The soak ran 22 minutes here
   (stdout to its own file, stderr to another — no spliced lines).
   Evidence: `gate-55b/windows/` (the Linux box's files sit at
   `gate-55b/`; a folder per box keeps both).

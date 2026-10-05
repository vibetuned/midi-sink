# DECISIONS — Part VIII (in flight): Phase 9, Instruments (steps 60–66)

Ambiguities resolved during Phase 9 (steps 60–66 of `_work/ROADMAP_5.md`):
the stateful probe in use, five layouts, the strip widgets, session replay.
References written as `DECISIONS_8 #n` mean this file; at the phase's end it
merges into `docs/DECISIONS.md` as Part VIII. The specs are
`docs/PROJECT_SPEC.md` (`SPEC §n`; its §10–§13 drafted in
`specs/TO_PROJECT_SPEC.md`), `specs/INSTRUMENT_SPEC.md` (`INSTRUMENT §n`) and
`specs/QUALITY_OF_LIFE_SPEC.md` (`QOL §n`); where an entry here and a spec
conflict, the entry is the record of what shipped — and the conflict is
flagged to the author, who owns the specs. The core is reopened for feature
work this phase (the layouts' probe); the phase invariant is the field
fixture, bitwise on Metal. Evidence per step under `docs/evidence/step<n>/`
until the fold.

## Step 60 — Stateful layout cores: the trumpet and the trombone (the Mac, headless; libsumi 1.4.0) — 2026-10-05

1. **The fingering is MIDI as Part V fixed it — valves CC 110 / 111 / 112
   (≥ 64 = pressed) and the slide CC 113, global state on the master
   channel — decoded by the normalizer into the engine's own copy of the
   layout state; the author confirms or overrides the numbers here.** The
   roadmap's provisional numbers are `DECISIONS_5 #5`'s (CC 102–119 are
   undefined in the MIDI specification and already carry midi-sink's own
   controls; a 14-bit slide pair would put its MSB in the Airwave's CC 0–31
   block; 128 steps over six semitones is 4.7 cents a step); they stand
   unless the author overrides them in this entry, and step 62's strip
   emits them. AS BUILT (`midi_normalizer.cpp`, INSTRUMENT §1): the
   normalizer — the one MPE decoder, shared with Voxo — keeps a
   `sumi_layout_state_t`: CC 110/111/112 set or clear `buttons` bits 0..2
   at once, CC 113 sets the slide's target 0..1. GLOBAL means: in MPE mode
   the zone's MASTER channel only — a member channel's 110–113 stay what
   they were, per-note controllers the CC map may route, nothing by default;
   in classic and wind mode any channel, there being no master. Every CC is
   still forwarded to the map as before (the valves could be mapped to a
   control as well; one source, two readers is the rule, not one consumer).
   THE SLIDE'S SMOOTHING (#5's "7-bit with normalizer smoothing"): the
   decoded slider follows its target through a 10 ms one-pole stepped once
   per drain on the drain's clock (dt clamped to a quarter second so a
   paused host resumes without a jump) and settles EXACTLY on the target
   once within 1e-4 — the 7-bit staircase reads as a ramp, within 2 % of a
   step in 40 ms at 60 Hz. What it smooths is the engine's copy, which has
   two consumers: the placement of a note that arrives while the slide
   moves (#2's rule tolerates the lag) and the probe answers a shell reads
   through the getter; the PITCH is not among them — a play surface carries
   the slide's continuous pitch in the voice's bend (INSTRUMENT §3), so the
   smoothing is inaudible by construction. THE ENGINE'S COPY: `sumi_update`
   takes the state from the drain before the mapper places the frame's
   notes (the valve CCs precede the note in a stream, so a note sent under
   a fingering lands in that fingering's cell), and `sumi_get_layout_state`
   (1.4.0, additive, render thread like `sumi_get_params`) hands it to the
   shell — the desktop's orbit trace places the brass layouts' notes with
   it; a play surface mirrors the bytes it sends into its own snapshot
   (step 62's hostmpe), the probe answering alike for both: one source of
   truth, the byte stream (gated, #4). The numbers are in the header as
   `SUMI_CC_VALVE_1/2/3` and `SUMI_CC_SLIDE` so the shells and the chart
   share them.

2. **The trumpet is eight partial cells that sound their partial minus the
   valves' offset, the trombone seven that sound theirs minus the slide's
   continuous semitones; a column by default, the trumpet's on an arc by a
   params flag; the lip bend is the cell's axis; a note from the wire lands
   in the partial its fingering selects, else the standard fingering, else
   the nearest partial.** INSTRUMENT §2–§3 as built (`layouts.cpp`). THE
   PARTIALS are SOUNDING MIDI notes of a B♭ instrument — the trumpet's
   harmonic series B♭1 46 (the pedal, "the fundamental" the spec counts),
   B♭2 58, F3 65, B♭3 70, D4 74, F4 77, A♭4 80 (the flat 7th idealised to
   equal temperament), B♭4 82; the trombone's the 2nd through the 8th, 58
   … 82 (the spec's "7 typical" — the chart's seven rows; the pedals are
   not cells). THE VALVES: offsets 1 = −2, 2 = −1, 3 = −3, combinations
   summing — open, −1, −2, −3 (1+2 or 3), −4 (2+3), −5 (1+3), −6 (1+2+3) —
   a table by `buttons & 7`; the real combinations' sharpness is
   deliberately not modelled (the spec's "realistic intonation" toggle
   stays a later flavour). THE SLIDE: `slider` 0..1 → 0..6 semitones
   CONTINUOUS; the probe's note is the nearest semitone of the cell's
   partial minus the slide — a midpoint rounds to the NEXT position (a
   thousandth-of-a-semitone bias makes the convention float-proof) — and
   the fraction is the shell's bend, so playing between positions is in
   tune with itself (62 sends the note-on at the probe's note with the
   bend that lands the exact pitch, then the slide's bend continuously).
   THE GEOMETRY: a COLUMN — the lowest partial at the bottom, the cells a
   tenth of the canvas height tall for eight (0.8/7 for seven) inside the
   chroma grid's 0.10 inset, the touch band 2.5 cells wide (0.25 canvas
   heights about the centre), R_max the cell's half-height (0.05; 0.057) —
   or, for the trumpet, an ARC over the top of the sheet
   (`params.trumpet_arc`, 1.4.0): the cells on a circle of radius 0.42
   about (0.5, 0.58), the lowest at the left (y 0.58) rising over the top
   (y 0.16) to the right, each a disc of radius 0.95 × 0.42 × sin(π/14) =
   0.0888 (under half the chord between neighbours), the radius shrunk on a
   portrait sheet so the ends stay on it; the hit is the band's row or the
   nearest centre within its radius. The spec's `[ITERATE: column or arc]`
   is answered by building both; the author chooses by eye at step 63 (the
   desktop's checkbox, the INI's `trumpet_arc`). THE PROBE answers the
   partial's note under the state (NULL = open valves, the slide in), the
   cell's centre and R_max, flags 0, and the SEMITONE AXIS +x with one
   semitone per cell radius — INSTRUMENT §2's lip bend, "X = per-note bend,
   ±1 semitone default scaling", so a drag to the cell's edge bends a
   semitone through hostmpe's gradient, further beyond it (the knee is a
   deadband, not a limit); the scale is the spec's `[ITERATE]`, the
   author's by ear at 63. THE PLACEMENT of a note on the wire: the partial
   the CURRENT fingering sounds it from (note + offset in the series; the
   trombone within a semitone of note + slide, which tolerates #1's ramp);
   failing that the standard fingering — the trumpet's smallest valve
   offset that reaches the note (A3 is valve 2 on the 4th partial, E3 is
   1+2+3 on the 2nd), the trombone's lowest partial within the slide's six
   semitones (C4 with the slide in is F3's partial at the 6th position);
   failing that the nearest partial by pitch — the gaps a real horn has
   between its pedal and its low register, and whatever lies above the
   series. A keyboard on the trumpet layout therefore draws at the partial
   a player would use. THE GLIDE on these layouts: the mapper's pitch axis
   is the probe's (+x, one radius a semitone) under the non-lattice cap
   (0.03 canvas heights a semitone — the brass layouts are not a lattice,
   their partials are 12, 7, 5, 4, 3, 3, 2 semitones apart), so the
   valves' and the slide's bends NUDGE the drop sideways, never move it a
   cell: INSTRUMENT §1's visual-echo `[ITERATE]` resolves as the roadmap
   says — the strip shows the fingering (62), the canvas stays ink — and
   the valve-change retune ramp is the shell's (62, the piano grid's
   machinery). THE CELLS the stir turns and the shells draw
   (`sumi_layout_cells`) are the partials themselves — eight or seven
   discs, the checkerboard alternating up the series; the state changes
   what they sound, never where they are, so the engine's cell cache is
   keyed by the arrangement, not the state. The spark gesture probes with
   the engine's state, so a strike on a brass layout reads its cell's
   note under the current fingering.

3. **libsumi 1.4.0, additive: the two layouts unreserved, the state
   getter, the arrangement flag, the CC constants; the internal layout API
   takes the state; the desktop lists the layouts, keeps the flag and keys
   the orbit trace's table by the fingering.** `sumi_layout_t` 8 and 9
   answer the probe and place notes; 10–12 still clamp to FIFTHS in
   `sumi_set_params`, the warning now naming step 61. `sumi_params_t`
   gained `trumpet_arc` at its end (clamped to 0/1); `sumi_get_layout_state`
   joined the configuration calls; `SUMI_CC_VALVE_1/2/3` and `SUMI_CC_SLIDE`
   the header (the C ABI test takes the getter's address and asserts the
   numbers and the version). INSIDE: `sumi_layout_position` and
   `sumi_layout_semitone_delta` take `const sumi_layout_state_t*` after the
   aspect (NULL = zeros; the 28 calls of the headless suite moved
   mechanically), the mapper keeps a copy the engine sets before each
   normalize (`sumi_voice_mapper_set_layout_state`), the probe's and the
   cells' signatures are unchanged. THE DESKTOP: "Trumpet (valves)" and
   "Trombone (slide)" in the picker (ten entries; the INI's `layout` wraps
   at ten, was eight since #64), "Partials on an arc" under the trumpet,
   the INI's `trumpet_arc`; the plate guide (the N key, `sumi_debug_cells`)
   draws their cells with no change of its own; the orbit trace's placement
   table is rebuilt when the engine's state changes on a brass layout (the
   96 × 54 probe scan, well under a millisecond); the bench's
   `--layout-shot <png>` (with `--layout`, `--trumpet-arc`) draws a layout
   as the visualizer's overlay — the guide over a scripted fingering phrase
   through the normalizer, the valve CCs on the master channel, the notes
   on a member channel, the trombone's slide CC with the bend a surface
   would send, then a dip and its print — the evidence's three shots. THE
   TABLETS AND THE WEB are untouched (their pickers list 0–7, their shims
   probe stateless) and compile against 1.4.0: iOS (`build-ios`), Android
   (`assembleDebug`), the web (`build-web`, the marble's gate on the
   rebuilt tree in #4).

4. **The goldens and the gates: every valve combination × every partial,
   the slide at the detents and the midpoints, a recorded fingering stream
   replaying into identical probe answers and placements; the field
   fixture bitwise.** `tests/normalizer_tests.cpp` (24 483 checks): the test's own tables and geometry formulas, stated apart from
   `layouts.cpp` — (a) the eight cells of the column and of the arc at two
   aspects, the seven of the trombone, where the goldens place them; (b)
   the 8 × 8 fingering table on both arrangements: the probe at every cell
   under every valve state answers the partial minus the offset, the axis
   +x at one radius a semitone, and the note placed under that state lands
   back in the cell; (c) the standard fingerings and the gaps (A3, E3, the
   pedal E1, a gap note, above and below the series, a note under a held
   valve that is and is not its partial); (d) the trombone's seven
   positions and the six midpoints between them at every partial — the
   note, the radius, the axis, the round trip — and C4's standard position;
   (e) the refusals (above the column, beside its band, the arc's empty
   centre, layouts 10–12) and the band's full width; (f) the normalizer:
   the MCM then the valves on the master channel (the threshold at 64, a
   member channel's CC 112 ignored, four CCs forwarded), the slide's ramp
   one drain short of its target and settled exactly four drains later,
   classic mode taking channel 4; (g) THE REPLAY: two normalizers fed the
   same bytes agree on the state bitwise at every drain, a shell's mirror
   of the bytes agrees with them at every cell's probe, and through the
   mapper a ten-note fingered phrase (the open series, then a chromatic run
   fingered on the 4th partial) and the trombone's seven positions each
   land in the partial cell the fingering selected. `abi_c_compile`:
   1.4.0, 28 symbols, the trumpet answering, 10–12 refused. ctest 10 of
   10. THE GATES on Metal: the field gate max 0.0 / mean 0.0 against the
   fixture (bitwise — no field pass changed; the negative control red), the
   composite gate 0 of 1 048 576 samples differing (red as required); the
   fixture stands as 55b captured it. THE SHOTS (`docs/evidence/step60/`):
   the trumpet's column — the open series up the eight cells, the fingered
   run's seven strikes as concentric rings on the B♭3 cell; the arc; the
   trombone's seven partials and B♭3's drop nudged left by the slide's
   bend. THE DEVICES, installed and launched on the author's word (the
   tablets are tested on every core change, 63/64 notwithstanding): the
   Tab through adb — the shell logs "sumi 1.4.0 ready", Play mode on the
   piano grid, the session config, the Dan Tranh, the process alive — and
   the iPad through `build-ios`, xcodegen and xcodebuild into `ios/build-dd`
   with the rebuilt `libsumi.a`, devicectl install and launch both exit 0;
   their shells reach no new code path (layouts 0–7, the probe stateless)
   and the field is bitwise, so the look is a regression check, the
   author's. FLAGS for the author: (1) the fingering numbers — #1's
   confirmation line; (2) the arrangement (column or arc) and the lip
   bend's ±1 semitone, by eye and by ear at 63; (3) the partials as
   SOUNDING pitches of a B♭ instrument — a C-trumpet series or a
   written-pitch table is one params field away if the author's reading of
   the cells wants it; (4) the probe's note at a slide midpoint rounds to
   the next position (a convention; the pitch between positions is the
   bend's, exact); (5) the brass layouts' glide visual is the capped
   sideways nudge — the drop does not travel between partials under a
   gliss, by the roadmap's rule; the author may want the opposite after
   playing 63; (6) INSTRUMENT §2 says "8 partial cells (fundamental through
   the 8th)", built literally with the pedal, while the trombone's seven
   start at the 2nd — the spec's "7 typical" — the asymmetry is the
   instruments' charts', noted; (7) step 62 inherits the contract: the
   strip's valve buttons emit CC 110–112 and the slider CC 113 on the
   master channel, hostmpe mirrors them into the snapshot it probes with,
   the note-on is the probe's note and the slide's fraction the bend.

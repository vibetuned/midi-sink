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

## Step 61 — Stateless layouts: Wicki–Hayden, strings, the theremin (the Mac, headless; libsumi 1.5.0) — 2026-10-05

5. **Wicki–Hayden is a hex button-field six buttons wide and fifteen rows
   tall on which every note has exactly one button; pitch is a plane over
   the sheet, so the probe's axis is its gradient.** INSTRUMENT §4 as built
   (`layouts.cpp`): a step right is a whole tone, up-right a fifth, up-left
   a fourth, the octave two rows straight up — the Hayden duet's field. The
   rows alternate the two whole-tone scales and sit half a button apart (the
   even rows right); a row holds SIX buttons because the lattice repeats a
   note six buttons left and two rows up (the kernel of 2Δc + 6Δr − (the
   parity's 1) = 0), so six is the width at which every note has one
   button and the layout is one echo by construction — the spec's "hex cell
   math beside Jankó's, one echo", and the roadmap's "kept: the concertina
   button-field, not a string layout". Fifteen rows from G0 — the row
   below C1, which carries C♯1, D♯1 and F1 — to F8 cover C1..B7 (odd notes
   19–113, even 24–106; the rows' ends reach past the grids' range, a
   FLAG). A note off the grid keeps its whole-tone button on the nearest
   row of its parity: the pitch class kept, as the grids clamp (G♯0 lands
   on G♯1's button). The stagger's half-button ends are off the field
   (Jankó's rule). Geometry: the chroma grid's insets, a button
   0.84/6.5 wide and 0.8/15 tall, R_max the half height (0.027; the width
   governs only under aspect 0.41). THE AXIS: with the stagger the pitch
   is exactly linear in position — two semitones a button, six a row — so
   the probe answers the plane's gradient as the semitone vector (the
   step 1/|∇p| ≈ 0.0088 canvas heights at aspect 1, pointing up and a
   little right); the shortest-neighbour rule would have picked a
   semitone's neighbour three buttons away on the next row (the departure
   Jankó took in DECISIONS_3 #18, for the same reason). The mapper renders
   the glide at that true step: twelve of them is two rows straight up —
   the drop lands on the octave's button.

6. **STRINGS is string-rows × chromatic frets under one of three fixed
   tuning presets; a note's echoes are its lowest-fret sites, and the axis
   runs along the string.** INSTRUMENT §4's fretboard generalised, as the
   roadmap cut it: `SUMI_LAYOUT_STRINGS` = 11 (the 1.0.0 name `FRETS` kept
   as a define — no host used it); `params.string_tuning` (1.5.0):
   `SUMI_STRINGS_STANDARD_GUITAR` (E2 A2 D3 G3 B3 E4),
   `SUMI_STRINGS_WHOLE_TONE_TAP` (twelve strings E2 … D4 a whole tone
   apart — the tapping-grid isomorphism; the trademark stays out of the
   enum and the names, the docs may say "inspired by tapping instruments"),
   `SUMI_STRINGS_ALL_FOURTHS` (E2 A2 D3 G3 C4 F4, the Stick's and the
   bass's world) — the spec's `[ITERATE: tuning table in params or fixed?]`
   resolved FIXED, the user-editable table deferred (§6). The frets 0..24:
   the open string and two octaves; the lowest string at the BOTTOM (tab's
   way), the nut at the left; R_max half a fret's width (0.017 at aspect
   1, 0.030 at 16:9 — Jankó's family). THE ECHOES: a note sits on every
   string that reaches it within 24 frets — six for a guitar's E4, up to
   twelve on the whole-tone grid — and the ABI carries three
   (`SUMI_MAX_ECHOES`, the deform budget's reason), so the engine places
   the three LOWEST-FRET sites, the first position first (echo 0 on the
   highest string that reaches the note): the common positions draw, and a
   note played high on a low string lands at its first-position site
   instead — the inherent limit of a redundant layout under a MIDI-only
   loopback (FLAG; a wider echo cap is a core change if the author wants
   every site). The edges clamp: under the lowest open string its open
   cell, over the top string's last fret that fret. THE AXIS is +x, one
   fret a semitone — "dragging along a string is literally a string bend"
   — in the mapper's lattice set, so a glide travels a fret per semitone
   on every echo at once (a fret is wider than the rendering cap, 0.0336
   against 0.030 at aspect 1: the test's proof). The cells the stir turns
   are enumerated as the grid has them, every (string, fret), not through
   the placements (a high fret is nobody's echo, yet a key the shells
   draw).

7. **The theremin has no cells: the field is the cell, the probe's note
   the nearest semitone of five octaves across the width, its centre that
   semitone's x, Y the bipolar press axis, the flag CONTINUOUS.**
   INSTRUMENT §4–§5 as built: C2 at the left edge to C7 at the right — 61
   semitone slots, 0.0138 canvas heights each at aspect 1 — the probe
   answering anywhere on the field with the note of the slot under x, the
   cell centre at that slot's x and the middle height, R_max the half
   height (0.4: the press axis's travel bound, the knee's 3 % at 0.012),
   the axis +x one slot wide, and `flags = SUMI_CELL_CONTINUOUS` — the
   1.0.0 bit, read at last (the spec's sentinel-versus-flags `[ITERATE]`
   was settled as flags at #45). Off the field refused. The engine places
   a note at its slot and renders the glide at the true step (the lattice
   set): the drop travels under the hand — the shot's octave streak.
   Notes outside C2–C7 clamp to the ends. The stir's cells are 61
   imaginary discs of half a slot along the middle. The surface (62) reads
   pitch from X through the legato re-anchor machinery — the note from the
   probe, the fraction from the step — and the bipolar press from Y; the
   range and Y's sign convention are its to settle by hand (FLAG).

8. **libsumi 1.5.0 (additive) and the goldens: every named layout ships;
   the fixture bitwise.** `sumi_layout_t` 10–12 answer the probe and place
   notes; `sumi_set_params` warns on an unknown id (> 12) and clamps
   `string_tuning` to the three; the cells cache is keyed by the tuning;
   `sumi_version` 1.5.0. THE DESKTOP: "Wicki-Hayden", "Strings" and
   "Theremin" in the picker (thirteen entries; the INI's `layout` wraps at
   thirteen), the tuning sub-picker under Strings, the INI's
   `string_tuning`, the shot's `--string-tuning`; the plate guide draws the
   new cells unchanged. The tablets' and the web's pickers stay on 0–7
   (their own steps, 63/64/66) and compile against 1.5.0. THE GOLDENS
   (`normalizer_tests`, 41 600 checks from 24 483; the test's own tables
   and formulas): Wicki–Hayden's 90 buttons at two aspects — the probe's
   note, centre, radius and gradient at every button, the bijection over
   C1..B7, the intervals +2 / +7 / +5 / +12 at an interior button, the
   dead half-buttons, the parity clamp; STRINGS under the three presets —
   every (string, fret), every note's echoes against the test's own site
   rule (the first position first, each probing back to the note), the
   edge clamps, the cell counts 150 / 300 / 150; the theremin — the flag,
   the note and the centre at off-centre probes across the field at three
   heights, R_max, the step, the refusals, the range clamps, 61 cells; the
   mapper's true step on the three (E3 on the guitar as three echoes with
   a fret's axis above the cap, the theremin's slot, the Wicki gradient).
   `abi_c_compile` 1.5.0, 28 symbols. ctest 10 of 10. THE GATES on Metal:
   the field gate max 0.0 / mean 0.0 (bitwise), the composite gate 0 of
   1 048 576 — no field pass changed; the marble's web gate PASS at 59c's
   numbers. THE SHOTS (`docs/evidence/step61/`): the hex field with the C
   major scale climbing its buttons, the three string grids with the
   scale's echoes along their strings, the theremin's slot line with C4
   glided an octave. THE DEVICES: both tablets installed and launched with
   the rebuilt core (the regression look the author's; their shells
   unchanged). FLAGS for the author: (1) the Wicki–Hayden rows' ends
   beyond C1–B7 (G0–F8), and its six-button width — the no-duplicate
   choice over the twelve-button rows some controllers use; (2) STRINGS'
   three-echo cap, the first position first; (3) the theremin's five
   octaves and Y's sign; (4) the roadmap's "thirteen entries in every
   settings list" is the desktop's here, the tablets' and the web's at
   their steps; (5) INSTRUMENT §4 still names the layout FRETS — the
   roadmap's STRINGS is the enumerator, FRETS the alias.

## Step 62 — hostmpe: the fingering on the wire, the brass retune, the theremin surface, the small UX items (platform-neutral; ctest) — 2026-10-05

9. **The fingering widgets: three momentary valve buttons and a POSITIONAL
   slider on the strip, their CCs on the master channel; the announce
   restates them; a strip reset is the panic's strip half.** INSTRUMENT
   §2–§3 and the roadmap as built (`hostmpe.h`): `hostmpe_strip_valve_press
   / _release` emit CC 110 / 111 / 112 (libsumi 1.4.0's numbers, #1) at
   127 down and 0 up, change-only (a held valve repeats nothing), the
   bitmask `hostmpe_strip_valves` being the shell's MIRROR of the bytes it
   sent — the `buttons` of the state it hands the probe; buttons, so the
   shell passes them to the limiters EXEMPT (a decimated valve-up is a
   stuck valve, as a decimated sustain-off is a stuck pedal). THE SLIDE is
   a new widget variant, the latch wheel's opposite: a latch ACCUMULATES
   deltas so a regrasp cannot jump its value; the slide's hand IS the
   value — `hostmpe_strip_slide_set(position 0..1)` → CC 113 =
   round(position · 127), change-only within a CC value (the seven
   positions at k/6 read 0, 21, 42, 64, 85, 106, 127; the detents are the
   shell's ticks, UI only — the value is continuous between them, "in tune
   with itself"); a continuous dimension the limiters police as any
   master-channel CC (under the rate policy a second move inside the
   period waits for the drain). THE ANNOUNCE (`hostmpe_strip_announce`)
   grew from five messages to nine — the valves then the slide follow the
   spring, the three wheels and the pedal — so a DAW re-synced mid-phrase
   agrees with the fingering too (the shells' announce buffers are 8 until
   their steps: the slide's line is the one truncated meanwhile, harmless).
   THE RESET (`hostmpe_strip_reset`): sustain off, every valve up, the
   spring home at once — a panic is the one jump that is right — emitting
   what changes and nothing twice; the latches and the slide keep their
   values (positions, not held states). The shell's panic action is
   `hostmpe_panic` (every voice released in the §5.1 order, the zone
   silenced) then the reset. Gated: the table above, the announce's nine
   on the master, the reset's three then nothing, the limiter classes.

10. **The brass retune: a per-voice PITCH OFFSET beside the joystick's bend,
    moved by what the probe reports, ramped over 30 ms for the valves and
    at once for the slide; a touch may begin with an offset so the attack
    is in tune between semitones.** INSTRUMENT §2's "changing valves while
    a voice sounds retunes it: bend ramp over 20–40 ms" as built:
    `hostmpe_voice_t` keeps `pitch_cur / pitch_target` (semitones relative
    to the note on the wire) that ADD to the joystick's own bend
    (`joy_semis`, kept so a ramp can re-emit the sum) — one 14-bit message
    carries both, change-only. `hostmpe_voice_retune(voice, now, delta,
    ramp_s)`: the shell computes `delta` by re-probing the voice's cell
    under the new state (the new note minus the old — geometry has one
    source of truth; no offset table lives in hostmpe), `ramp_s` =
    `HOSTMPE_RETUNE_S` (0.030, the piano grid's 20–40 ms legato number) for
    the valves — the ramp's bends surface from `hostmpe_tick` each frame,
    monotone, the FINAL message exactly the target whatever the cadence
    (the strip spring's rule), a re-ramp continuing from the current value
    — or 0 for the slide, which emits at once: the hand is the ramp.
    `hostmpe_touch_begin_offset` starts a voice with a non-centre first
    bend (the fraction's) so the attack is in tune at the hand's pitch —
    the trombone between positions (the probe's note is the nearest
    semitone, the slide's exact pitch the fraction), the theremin's hand
    between semitones; §5.1's "centre bend first" becomes "the bend first:
    centre, or the attack's own fraction", and `tools/midi_asserts.py`
    accepts a first bend within half a semitone of centre (86 counts at
    ±48). THE TRACES (`docs/evidence/step62/traces/`, written by the
    goldens under `HOSTMPE_EVIDENCE`, the chart's rows for step 67):
    `trumpet_phrase.csv` — B♭3 held on the 4th partial, valve 2 down (the
    strip's CC 111 on the master, the voice a semitone down over eight
    5 ms ticks, monotone, landing exactly at −1 st), the lip bend riding on
    top (half a radius right adds the knee's share of half a semitone to
    the sum), 1+2+3 (two more strip messages, the voice to −6), the hand
    back to centre reading the fingering alone, the lift, the valves up;
    `trombone_glissando.csv` — the slide at 0.7 st (CC 113 = 15), the attack
    on 69 with the +0.3 fraction's bend, the slide out to the 7th position
    in a hundred 10 ms steps (a strip CC and a voice bend each), the pitch
    following 70 − s within 0.004 st (under the 14-bit quantum, 0.0029)
    monotone, landing at 64 = 69 − 5, no ramp having run. Both pass the
    analyser's every assert.

11. **The theremin surface: pitch from X set absolutely through the pen's
    same-channel re-anchor, Y the finger's bipolar press axis.** INSTRUMENT
    §4 as built: `hostmpe_theremin_begin(note, fraction, velocity, R_max)`
    — the probe's note under the hand and its fraction (the hand's x minus
    the cell's over the step), a touch-down with that offset and no lattice
    gradient (the joystick's x is nothing here); `hostmpe_theremin_move(
    note, fraction, dy)` — the probe's answer under the hand NOW: the pitch
    about the anchor note is set absolutely (the bend follows the hand,
    change-only) until the hand is more than `HOSTMPE_THEREMIN_REANCHOR` =
    47 semitones from the anchor, then the pen's legato re-anchor (#39):
    bend(fraction) → Note On(the hand's note, the touch velocity) → Note
    Off(the old) — pitch continuous across it, a terminated note for the
    DAW; no retrigger inside the range: a theremin never retriggers. Y as
    the finger's (the shared `emit_y`): up → channel pressure, down → the
    swirl's poly pressure, the probe's R_max (the half height) as the
    travel. THE TRACE `theremin_stream.csv`: the hand lands at C4 + 0.2
    (the attack at the fraction's bend), slides fifty semitones up in 500
    steps of 4 ms while pushing away then pulling back — one re-anchor
    past 47, never a retrigger inside, the pitch within 0.004 st of the
    hand and monotone throughout, pressure then the swirl seen, the lift's
    three messages (pressure 0, the swirl home, Note Off). The range (five
    octaves) and Y's sign are the shells' by hand (63/64), as #7 said.

12. **The small UX items (QOL §2) and the gates: the panic as an action
    everywhere, the quick-switch subset, left-handed mirroring, the
    per-device offer; ctest green, the field bitwise.** THE PANIC: the
    mapper now honours CC 120 (All Sound Off) and CC 123 (All Notes Off) —
    the channel's held voice ends silently (MPE's member voice, wind's one
    brush on any channel; a classic voice carries no held state), never a
    control — so the channel-mode messages `hostmpe_panic` sends on every
    channel flush the visualizer's voices as they flush a synth's; the
    desktop's settings window gained "Panic (all notes off)" in its MIDI
    section (CC 120 and 123 on sixteen channels through the harness: the
    engine ends every voice, Voxo runs its own panic on the same bytes) —
    QOL §2's "the desktop has none yet" closed; the strip button is the
    shells' (63/64) over `hostmpe_panic` + `hostmpe_strip_reset`. THE
    QUICK-SWITCH: `hostmpe_strip_quick_set / _count / _next` — a
    user-chosen subset of layouts in cycling order, the next after the
    current (wrapping), the subset's head when the current is outside it,
    the current when the subset is empty; pure state the shell applies.
    MIRRORING: `hostmpe_set_mirror` flips the hand's horizontal delta in
    `hostmpe_touch_update` (a rightward drag bends down: the mirrored
    lattice's leftward); the shell flips the surface, the touch's x before
    the probe and the strip's corner. THE PER-DEVICE OFFER (DECISIONS_5
    #7's "offer, never auto-apply"): `hostmpe_device_profile(name)` maps a
    MIDI device name by case-insensitive substring to a family and the
    input mode it plays best in — Seaboard / LUMI / Lightpad → ROLI (MPE),
    Airwave (any mode: its CC map is the default), Osmose, LinnStrument
    and Continuum (MPE), Brisa, Travel Sax and EWI (wind), anything else
    NONE with an empty name — the shells offer "use the Osmose settings?"
    on a device's appearance and apply on a yes; the repo ships no
    per-device preset FILES, so the offer is the input mode (FLAG: the
    author may want more per family). THE GATES: `hostmpe_tests` 3 158
    checks (the widgets, the three traces, mirroring, the profiles; the
    step-18 announce check reads nine), `normalizer_tests`
    41 610 (All Notes Off / All Sound Off in MPE, classic and wind),
    `hostmpe_c_compile` the eighteen new symbols, ctest 10 of 10; the field
    gate and the composite gate bitwise on Metal (the mapper's new branch
    runs only on 120/123); the web gate PASS at 59c's numbers; the tablets
    rebuilt and launched (their shells call none of the new functions;
    their announce truncates the slide's line at 8 until 63/64). The
    roadmap's "the goldens pass on all three desktops' CI" is the boxes'
    CI on the push — the Mac's ctest here. FLAGS: (1) the analyser's
    centre-bend rule widened to half a semitone (the in-tune attack
    between semitones); (2) INSTRUMENT §2's "±1 semitone default scaling"
    for the lip bend stands as #2 built it, by ear at 63; (3) the per-device
    offer's table; (4) the shells' announce buffers (8 → 9) at 63/64.

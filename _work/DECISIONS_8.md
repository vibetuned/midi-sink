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

## Step 63 — The iOS play surface: the instruments on the iPad (the Mac, the iOS agent) — 2026-10-05

13. **The iPad plays the instruments: the strip grows the valves and the
    slide, the overlay hands the probe the fingering it mirrors from the
    bytes it sent, held brass voices retune through hostmpe, the theremin
    is the field; Suzu's bowed string sings under the valves.** INSTRUMENT
    §2–§5 and QOL §2 on the shell, as built. THE PICKER lists the thirteen
    layouts, the eight keyed ones "(playable)"; "Partials on an arc" under
    the trumpet (step 60's flag — the author chooses by eye on the device:
    both captured) and the tuning preset under Strings (61's); the preset
    serializer learned `trumpet_arc` and `string_tuning` (steps 60–61 left
    them out of the field table; the session file persists them now, the
    round-trip test asserts them — a change every shell shares). THE
    FINGERING MIRROR (`SumiCanvasView.fingering`): the strip's valve and
    slide engines are the source of the bytes, so the mirror is read FROM
    them after every change (`hostmpe_strip_valves`,
    `hostmpe_strip_slide_value`) — the shell keeps the state beside its
    params snapshot, as INSTRUMENT §1 asks, and the overlay hands it to
    every probe (the lattice, the touch-down, the pen); the queue's copy
    drives the retune, the main thread's the overlay. THE STRIP: the row
    is `visible` for the moment — the five of step 18, then V1 V2 V3 on
    the trumpet or the Slide on the trombone (2.2 slots wide: a track with
    the seven positions' ticks and a thumb at the hand, POSITIONAL — the
    hand is the value, DECISIONS_8 #9), Next when a quick-switch subset is
    chosen, Panic always; its width follows its widgets; left-handed it
    sits at the top-right. A valve down is `hostmpe_strip_valve_press` →
    CC 110–112 on the master, exempt; the slide `hostmpe_strip_slide_set`
    → CC 113, policed; then `fingeringChanged`: the mirror re-read and
    EVERY HELD BRASS VOICE RETUNED — the trumpet's by re-probing the
    voice's own cell (kept per voice at touch-down) under the old and the
    new state, the difference of the notes ramped over `HOSTMPE_RETUNE_S`
    (the ramp's bends surface from `hostmpe_tick` on the frame drain,
    beside the strip spring's); the trombone's by the slide's delta in
    semitones at once (the hand is the ramp) — DECISIONS_8 #10 on the
    device: the trumpet log shows CC 111 = 127 on the master followed by
    the voice's bend stepping to −1 st over two frames, then 1+2+3 taking
    it to −6 (8192 − 1024), the lift, the valves up. THE TROMBONE's attack
    between positions passes the slide's fraction as the first bend
    (`hostmpe_touch_begin_offset`): the log's 42 slide CCs answer 41 voice
    bends. THE THEREMIN: a CONTINUOUS cell (the flag at last read by a
    shell) is drawn as its semitone slots along the middle instead of a
    0.4-high circle; a touch there — finger or pencil — is
    `hostmpe_theremin_begin` with the hand's fraction, every move
    `hostmpe_theremin_move` with the hand re-probed (the x mirrored
    first) and the vertical delta as the bipolar press; the indicator
    follows Y alone; off the field the last pitch sustains. The log: one
    note on, one off, 77 bends, 81 pressures over a two-octave sweep.
    PANIC is a strip pad and the settings' button: `hostmpe_panic`, the
    held cells forgotten, then `hostmpe_strip_reset` (sustain off, the
    valves up, the spring home) and the mirrors re-synced. THE
    QUICK-SWITCH: the settings' "Quick-switch (the Next pad)" lists the
    eight playable layouts as toggles, the subset persisted as ids; the
    Next pad asks `hostmpe_strip_quick_next` of the current layout and
    sets the session's. LEFT-HANDED: the settings' toggle (persisted)
    flips the lattice layers (a −1 scale about the centre), the touches'
    x before the probe, the held-note highlight, hostmpe's horizontal
    delta (`hostmpe_set_mirror`) and the strip's corner — captured with
    the arc's lowest partial at the right. THE PER-DEVICE OFFER: a source
    newly connected (and those present at launch) runs through
    `hostmpe_device_profile`; a known family posts ONE alert per device
    per launch — "Osmose connected: plays best as MPE. Use that input
    dialect now?" — Use it sets the session's input mode, Not now
    dismisses; nothing is ever applied by itself (DECISIONS_5 #7). SUZU
    ON THE IPAD (step 56's shared UI, consumed): the Sound page's Source
    row — Sampler / Suzu / Both — and a Suzu patch picker of five (the
    bowed string, the bell, the flute, the saxophone, the trumpet: Voxo's
    defaults with the voice kind and the modal preset picked; the bowed
    harmonic string the default, `press_blows` on so a finger's upward Y
    is the breath); the covered-notes mask is the sampler's alone (Suzu
    sounds every cell); `--voxo-source suzu` for the lab. The trumpet runs
    above were played with Suzu as the source: the bowed string under the
    valves' bends — "a bowed patch sings under the trumpet's valves" by
    construction; by ear, the author's. VOXO RE-SOUNDS the trumpet
    because the bytes are the same bytes (the push fans them, Local
    Control on). THE LAB ARGUMENTS for the evidence: `--layout <n>`,
    `--trumpet-arc`, `--string-tuning <n>` (the session's, applied before
    the first save), `--play` and `--mirror` (TRANSIENT overrides — the
    author's stored switches are never written by an argument),
    `--fingering-demo` (a scripted phrase three seconds in, through the
    real path — the cells from a probe sweep, the valves and the slide from
    the strip's engines — then the byte log flushed) and `--capture` (a
    four-frame burst); the settings' Evidence section has "Play the
    fingering demo" too. The author's session file was saved before the
    runs and put back after them (the piano grid on Anod). GarageBand's
    replay of a fingered phrase is the author's hand test; the log is the
    evidence here: CC 110–112 on the master channel beside the voice's
    bends, every assert of `tools/midi_asserts.py` holding on six runs.
    FLAGS for the author: (1) the arrangement — column or arc — by eye,
    now that both are on the device; (2) the lip bend's ±1 semitone per
    cell radius, by ear; (3) the demo's valve legato is scripted — a hand
    on the strip while a finger holds a partial is the real test; (4) the
    Suzu patch list is five — the desktop's full knob set is not on the
    iPad (a Phase-10 polish if wanted); (5) Android's shell (64) inherits
    every mechanism one for one: the strip's row, the fingering mirror,
    the retune on `fingeringChanged`, the theremin path, the offer; (6)
    the per-device offer carries the input mode only (#12).

14. **The author's fixes on the iPad: the fingering is a large panel at the
    side, at mid-height, the strip at the opposite corner on the brass
    layouts; a string note lights every string that reaches it; the sound
    is named by what sounds.** The author, with step 63 on the device: the
    wheels should move to the right and the valves and the slide take the
    left "or mirrored in the mirror form"; the valves and the slide "are
    too small, they need to take more space and be in the middle, not the
    top, to be easy to play"; the strings "only trigger the drop in
    maximum 3 positions of the same note … when you play the note in the
    bottom right only the top left are triggered"; the Sound row "shows
    the sample dslibrary name, it should show the synth name, something
    like suzu: trumpet". AS BUILT: (1) `FingeringPanelView` — on the
    trumpet three pads stacked top to bottom (1, 2, 3: a hand reaching
    from the side rests its three fingers on them), on the trombone a
    VERTICAL track with the seven positions numbered, the 1st at the top
    and the 7th at the bottom (down is out, lower) and a wide thumb at the
    hand; 140 × 400 and 104 × 460 points, vertically centred at the left
    edge; a valve stays down until its finger lifts wherever the finger
    wanders (real valves); the strip keeps the wheels, the pedal, Next and
    Panic and moves to the top-RIGHT on the brass layouts; left-handed
    the two swap sides. The engines are unchanged (hostmpe's valve and
    slide engines, the same CCs, the same retune). (2) `SUMI_MAX_ECHOES`
    3 → 12: the STRINGS layout places a note on EVERY string that reaches
    it (the whole-tone grid's twelve at most), the first position first,
    so the cell under the hand always lights whichever site it was — the
    cap of #6 was the inherent limit of a MIDI-only loopback made visible,
    and the author chose the Jankó rule (every site is the note) over it;
    Jankó stays three; the deform budget's merging absorbs a chord of
    twelve-echo voices as it absorbs Jankó's; the suite's strings golden
    now expects every site (43 474 checks); the field and composite gates
    stay bitwise (the field script has no strings). (3)
    `SoundController.activeName`: "Suzu: Trumpet" when the synth is the
    source, the sampler's instrument otherwise, "Suzu: Bell + <library>"
    when layered — the settings' Sound row and the Sound page's Instrument
    section (headed "Sampler instrument (silent: Suzu is the source)" or
    "(layered under Suzu)") read it. Captured on the device after the
    fix: the trumpet with the pads at the left and the strip at the
    right, the trombone's vertical slide, the guitar's scale lighting the
    low strings' high frets too, the mirrored trumpet with the pads at the
    right; the four runs' logs pass every analyser assert again. FLAG:
    the Android shell (64) takes the panel too.

15. **The author's second round: the fingering panel in both forms —
    vertical at the side or horizontal along the bottom — and the arc
    re-cut, smaller cells with the centre pushed right.** The author, with
    #14 on the device: "I like the size and feel of both"; two things —
    "the ability to show the slider and the valves horizontal and not only
    vertical", and "the partial in arc is way too big and you can push the
    partial a little more to the right". AS BUILT: (1) `FingeringPanelView`
    gained an orientation. HORIZONTAL lays the three pads side by side, 1
    under the index finger — left to right, and right to left when
    mirrored, the hand coming from the other side — and the slide along a
    horizontal track, the 1st position at the hand's near side (the left;
    the right when mirrored) and the 7th away from it: the slide goes OUT,
    as on the instrument; 400 × 140 and 460 × 104 points along the bottom
    edge at the left (the right when mirrored), the width clamped to half
    the sheet. The vertical form stays as #14 built it. The settings'
    toggle "Fingering panel horizontal (along the bottom)" (persisted,
    `fingeringHorizontal`) chooses; the lab's `--fingering-horizontal` is a
    TRANSIENT override like `--play`. The engines and the bytes are
    unchanged — the form is the view's alone. (2) THE ARC (step 60's
    `trumpet_arc`, the core's `brass_geom` / `brass_cell`): the cells were
    under half the chord between neighbours, 0.95 R sin(π/14) ≈ 0.089
    canvas heights for the radius 0.42 — "way too big" by the author's eye;
    they are a fixed 0.055 now (`BRASS_ARC_CELL_R`), and the arc's centre
    sits 0.08 canvas heights right of the sheet's middle (`BRASS_ARC_CX`;
    the fingering panel takes the left side — mirrored, the overlay flips
    the sheet), the radius 0.42 and the centre height 0.58 as before; the
    narrow-sheet clamp keeps the RIGHT end on the sheet, (CX + R)/aspect ≤
    0.46. The column is untouched. The suite's `golden_arc_cell` follows
    (43 474 checks); the field and composite gates stay bitwise (the
    fixture is the piano grid); the header's comment says the new figures.
    A consequence for the ear: the brass axis is one semitone per cell
    radius (INSTRUMENT §2), so on the arc the lip bend now reaches ±1
    semitone over 0.055 canvas heights instead of 0.089 — the column keeps
    its radius; the author's by ear, with #13's flag (2). Captured on the
    device after the fix: the arc with its eight small cells rising from
    the left over the top to the right of centre, the pads at the left; the
    trumpet with the three pads along the bottom; the trombone with the
    slide along the bottom, 1 at the left — the three runs' logs pass every
    analyser assert (eight logged runs in the evidence). FLAG: Android's
    shell (64) takes both forms of the panel.

16. **The author's second look at #15: the ring itself shrinks and centres
    on the sheet; the panel's form is found where the layout is chosen, and
    on the panel.** The author, with #15 on the device: "I don't see the
    toggle horizontal button in the controls of the iPad version, and the
    arc keeps the same size — only the cells changed size." Two misses in
    #15: the toggle sat in the settings' Control strip section, which
    exists only in Play mode — invisible from Marble mode and from the
    canvas; and the arc's cells shrank while its radius stayed 0.42 — the
    ask was the arc. AS BUILT: (1) `BRASS_ARC_R` 0.42 → 0.30 canvas
    heights — as tight as the 0.055 cells allow: the chord between
    neighbours, 2 R sin(π/14) = 0.133, against a cell diameter of 0.11
    leaves a fifth of a diameter of clear sheet between them; the centre
    height 0.58 → 0.65, so the ring's own middle (0.35 to 0.65 of the
    height) is the sheet's; the centre stays 0.08 right of the middle, the
    cells 0.055, the narrow-sheet clamp as #15. On the iPad (aspect 1.44)
    the ring spans 0.35 to 0.76 of the width — the middle-right, clear of
    the panel at the left; the lip bend's semitone stays the cell radius.
    The suite's `golden_arc_cell` follows and the empty-centre probe moved
    to the ring's centre (43 474 checks); the field and composite gates
    stay bitwise on Metal. (2) The toggle "Fingering panel horizontal
    (along the bottom)" moved out of the Control strip section to the
    LAYOUT section, under the Trumpet and the Trombone beside "Partials on
    an arc" — visible in either mode; the Control strip's note says where.
    (3) The panel's own ROTATE BUTTON: SF Symbols' rotate.right at the top
    corner towards the sheet (the right; the left when mirrored) in every
    form — a tap flips the form and persists it through the same user
    default the settings' toggle reads (`toggleFingeringOrientation`;
    @AppStorage follows UserDefaults); the valves' pads sit under a
    30-point header band that holds it (the panel 140 × 420 and 400 × 150
    points), the vertical slide's track inset makes room. Under the lab's
    `--fingering-horizontal` the override wins again on the next SwiftUI
    update. The settings' notes re-worded: the valves are on the panel,
    not the strip; the arc is a ring a little right of the middle, no
    longer "over the top of the sheet". (4) A scripted `--layout` now
    CLEARS the arc flag unless `--trumpet-arc` is given — the author's
    session had the arc on, and the column runs inherited it: the lab's
    arguments define the whole layout. Captured on the device: the ring of
    eight small cells in the sheet's middle-right with the pads at the
    left and the rotate button at the pads' corner; the column; the
    trombone's vertical slide with the button beside its label; the
    trumpet's and the trombone's horizontal forms along the bottom — the
    five runs' logs pass every analyser assert (eight logged runs in the
    evidence). The Tab launched on the new core; the web compiles. The
    author's own session (the trumpet, the arc on) was put back after the
    runs and read back to confirm. FLAG: Android's shell (64) takes the
    rotate button with the panel.

17. **The trombone on the ring, under the one brass flag; the strip stops
    short of the settings gear.** The author, with #16 on the device: "the
    trumpet is perfect"; two new things — "the panic button collides with
    the gear to open the settings so is unreachable", and "I will also
    really like an arc for the trombone". AS BUILT: (1) ONE ARRANGEMENT FOR
    THE BRASS. `brass_geom` reads `params.trumpet_arc` for the trombone
    too: its seven partials sit on the same ring (radius 0.30, the centre
    0.08 right of the middle at 0.65 of the height, cells 0.055 — the
    chord between neighbours 2 R sin(π/12) = 0.155, roomier than the
    trumpet's 0.133), π at the left for the 2nd partial, 0 at the right
    for the 8th; the slide's note per cell, the lip-bend axis and the
    placement are the column's. The flag keeps the ABI's name — a second
    field would be an additive 1.6.0 for one bit, and the author asked for
    the arc, not for two arcs: the header's comment says the name's
    history; `trombone_arc` is a small addition if the two are ever to
    differ. The desktop's "Partials on an arc" shows under both brass
    layouts; the iPad's toggle too, its note naming the seven cells (B♭2
    to B♭5) and the slide; the lab's `--trumpet-arc` arranges both. The
    suite's `golden_arc_cell` takes the cell count and the trombone block
    gained (d2): seven cells on the ring at every aspect, the probe at
    each under the 4th position answering the partial minus three, the
    placement landing on the cell. The engine's cells key already carried
    the flag for every layout. (2) THE STRIP AND THE GEAR: the settings
    gear is the SwiftUI overlay's top-trailing button; the strip at the
    right (the brass layouts, or left-handed elsewhere) sat under it with
    Panic last — the strip now stops 52 points short of the right edge
    there, the gear's column, and keeps its 10 at the left. Captured on
    the device: the trombone's seven cells on the ring with the slide
    panel (in the author's stored horizontal form) and the strip clear of
    the gear; the trumpet's column with the same strip — both runs' logs
    pass every analyser assert (nine logged runs in the evidence); the
    suite at 43 630 checks, the field and composite gates bitwise on
    Metal; the Tab launched on the new core; the web compiles; the
    author's own session put back after the runs. FLAG: Android's shell
    (64) shows the arc toggle under both brass layouts too.

## Step 64 — The Android play surface: the instruments on the Tab (the Mac, the Android agent) — 2026-10-05

18. **The Tab plays the instruments as the iPad does — every step-63
    mechanism one for one, the retune's re-probe on the MIDI thread, the
    panel's mirror exact by construction; the probe stayed pure, the
    touch latency measured against Phase 4's.** INSTRUMENT §2–§5 and QOL
    §2 on the Android shell, as built — the iPad's step 63 (#13–#17) read
    as the specification, the Tab's own architecture (DECISIONS_3 #14,
    #46–#47: hostmpe on the AMidi poller thread, every UI call a posted
    command, touch-down a sync hop, the params snapshot the UI thread's
    probe truth) kept. THE PICKER lists the thirteen layouts, "Partials
    on an arc" under the Trumpet and the Trombone (one choice for the
    brass, #17), the tuning preset under Strings, "Fingering panel
    horizontal (along the bottom)" under the brass; the session carries
    `trumpet_arc` and `string_tuning` through the one serializer already
    (#13), so a change re-cuts the lattice without a layout change (the
    shell's lattice key is the layout, the arc and the tuning). THE
    FINGERING PANEL (`FingeringPanelView.kt`, the Swift view's twin):
    the three pads or the slide, vertical at the side at mid-height or
    horizontal along the bottom, the rotate button at the corner towards
    the sheet, mirrored left-handed; the valves CC 110–112 exempt, the
    slide CC 113 policed (`nativeStripValveDown/Up`, `nativeStripSlideSet`
    — posted, as every strip call is). THE MIRROR: on the iPad the state
    is read back FROM the engines on the MIDI queue after every change;
    on the Tab the engines are a thread away, so the panel's mirror is
    made EXACT BY CONSTRUCTION — the valves are bits, the slide is
    quantised as the engine sends it, round(position · 127) / 127
    (hostmpe.h's `hostmpe_strip_slide_value` contract) — and handed to the
    overlay (`setFingering`), whose every probe carries it
    (`nativeLayoutProbe` and `nativeLatticeSweep` take the valves and the
    slide; `out[7]` is the flags); the engines' own values are re-read at
    mode entry and after a panic (`nativeStripState` grew to ten:
    valves, slide). The brass cells' NOTES follow a fingering change —
    one probe per cell centre (`refreshCellNotes`), their places never —
    so the held-note highlight and the demo's cell lookup agree with the
    hand. THE RETUNE lives natively (`fingering_changed` in
    `sumi_play.cpp`, MIDI thread): the held voices' cells are kept beside
    the voice (`nativeTouchBegin` grew the offset and the cell), the
    trumpet's cell re-probed under the old and the new state with the
    params snapshot and the overlay's aspect (`nativeSetAspect`), the
    difference ramped over `HOSTMPE_RETUNE_S`; the trombone's slide delta
    × 6 at once; `hostmpe_tick` runs in `play_drain`'s 4 ms step beside
    the strip's. THE TROMBONE's attack between positions and THE
    THEREMIN (`nativeThereminBegin` sync, `nativeThereminMove` posted, a
    batch with a Note On exempt) as the iPad's; the theremin's field is
    returned by the sweep as its semitone slots (the radius half a step),
    so the Kotlin lattice draws them with no geometry of its own. PANIC
    is a strip pad and the settings' action: the native panic now
    releases every voice, silences the zone AND resets the strip
    (`hostmpe_strip_reset`, then the mirror) in one posted command — the
    Kotlin side only re-reads. THE STRIP's row is dynamic (Next when a
    subset is chosen, Panic always; `preferredWidthDp` sizes the floating
    palette) and sits at the top-RIGHT on the brass layouts, 62 dp short
    of the gear's column (#17), the sides swapped left-handed — the
    frame's `layoutPlaySurface`, the iPad's. QUICK-SWITCH: the settings'
    toggles over the eight playable layouts, persisted as a preference,
    `hostmpe_strip_quick_set`/`_next` the engine. LEFT-HANDED: the
    lattice and the held highlight drawn under a canvas flip, the
    touches' x mirrored before the probe, `hostmpe_set_mirror` for the
    hand's delta. THE PER-DEVICE OFFER: `MidiInputs.onSourceAppeared`
    runs each opened input's name through `hostmpe_device_profile`
    (`nativeDeviceProfile` → "family|mode"); a known family posts ONE
    alert per device per launch, "Use it" patches the session's input
    mode, "Not now" dismisses (DECISIONS_5 #7). SUZU ON THE TAB: the
    Sound page's Source row — Sampler / Suzu / Both — and the patch
    picker of five (`nativeVoxoSetSource`, the iPad's table: the bowed
    harmonic string, the bell, the flute, the saxophone, the trumpet);
    the Instrument row names what sounds ("Suzu: Trumpet"); the
    covered-notes mask is the sampler's alone. THE LAB EXTRAS: `--ei
    layout N` (now 0–12; a scripted layout is the whole layout — `--es
    trumpetArc 1` the ring, absent the column, `--ei stringTuning N`),
    `--es mirror 1` and `--es fingeringHorizontal 1` (TRANSIENT, the
    author's stored switches untouched), `--es fingeringDemo 1` (the
    scripted phrase three seconds in, through the real path, the log
    flushed after — the iPad's phrase, the same five scripts), `--es
    voxoSource suzu|sampler|both`, `--ei suzuPatch N`; the captures are
    `adb exec-out screencap` from the Mac (no in-app burst needed), the
    logs `run-as cat`; the author's session file AND preferences are
    saved before the runs and put back after (the Tab's lab extras
    persist `playMode`, as step 22 built them). NO CORE CHANGE: libsumi
    1.5.0, the field and composite gates untouched. ON THE TAB: ten runs
    — the column, the ring, the trombone in both arrangements, the
    theremin, the guitar, the Wicki–Hayden, the mirrored trumpet, the two
    horizontal forms — every assert of `tools/midi_asserts.py` holding on
    the nine logged ones (the trumpet log: the announce of nine, CC
    110–112 on the master with the voice's bend stepping under them; the
    trombone's 42 slide CCs and their bends; the theremin's sweep); the
    captures as the iPad's. THE LATENCY GATE, measured: 48 scripted taps
    on the chromatic grid through the in-app marks, the SAME script on
    the committed tree before this step (a scratch worktree) and on this
    one the same evening — touch-down → rendered frame median 4.89 ms
    before, 4.57 after (p90 10.05 → 9.36, the maxima a frame boundary),
    touch-down → push 0.46 → 0.33 ms; Phase 4's record (step 22) 3.73 /
    8.19 / 11.55. Unchanged within noise: the probe stayed pure, the
    fingering rides along as two arguments. FLAGS: (1) the mirror-exact
    rule — the panel's slide value equals the engine's by the quantisation
    contract, not by a read-back; if hostmpe ever changes the slide's
    quantisation the Kotlin `quantised()` follows or the probe drifts
    (one line, named here); (2) the Tab's lab extras persist `playMode`
    (step 22's design) — the runs save and restore the preferences; (3)
    the S-Pen on the brass: the legato re-probes under the fingering, the
    retune covers fingers only (a pen voice is not a held cell) — the
    iPad's rule, carried.

19. **The core's default params were undefined for the two Phase-9 fields;
    the struct is zeroed now and both named — a bug found by the Tab's
    session file, fixed under bug → regression test → fix.** The Tab's
    stored session read `trumpet_arc` 200 and `string_tuning` 5. The
    cause: `default_params()` in `core/src/engine.cpp` filled a
    `sumi_params_t` field by field, and steps 60 and 61 added their fields
    to the header without a line there — the two held the stack's
    leftovers at `sumi_create`. The engine clamps both on `sumi_set_params`
    (the Tab showed the ring, and the standard guitar), so the picture was
    right and the geometry gates never saw it; but a shell that seeds its
    session from `sumi_get_params` right after create (the Tab, the iPad)
    wrote the leftovers to its file when the file lacked the keys, and
    read them back as "off" in a toggle comparing with 1 while the ring
    showed — the author's Tab, exactly. The desktop's INI reads the flag
    as a bool and healed itself; the iPad's toggle compares with 0 and
    agreed with the picture by luck. THE FIX: `default_params()` begins
    with `memset(&p, 0, sizeof p)` — a field added without a default is 0,
    never undefined — and names `trumpet_arc = 0` (the column) and
    `string_tuning = SUMI_STRINGS_STANDARD_GUITAR`; no ABI change
    (libsumi stays 1.5.0), no behaviour change for a defined field. THE
    REGRESSION TEST: the bench's `--defaults-test` (`t64_defaults_test`)
    — `main.cpp` captures the params as `sumi_create` left them, before
    any settings apply, and the test pins the documented defaults
    (`trumpet_arc` 0, `string_tuning` the guitar, the fifths, Sumi,
    palette 0, sim_scale 1, smoothing 30 ms, 120 bpm); it reads
    undefined memory before the fix, so it is deterministic only after it
    — the memset is the guarantee, the test the contract. THE HEAL: the
    Android shell's `apply_session` now writes the core's clamped params
    back into the session (sim_scale, the host's, kept), so a stored
    out-of-range value is replaced by what the core holds at the next
    save instead of living on in the file; the iPad's session reads the
    core's clamped snapshot already (step 45b). FLAG for the author: the
    Tab's trumpet shows the RING today because the stored 200 clamps to
    1 — a toggle away from the column, your choice; and the field and
    composite gates re-ran after the fix (bitwise), the suites and both
    tablets rebuilt and launched.

20. **"When choosing a layout the previous layout persists" — the Tab's
    probe snapshot followed a session patch only when the render thread
    applied it, and the lattice was swept before that; it follows the
    patch at once now.** The author, with step 64 on the Tab. The
    mechanism, read off the logs: a layout pick is a session patch
    (`nativeSessionPatch`, the UI thread) that updates the native session
    and POSTS `apply_session` to the render thread; Kotlin's session
    listener runs on the very next call and sweeps the lattice through
    `nativeLatticeSweep`, which reads `params_snapshot()` — and the
    snapshot was written only by `apply_session`, a frame later. The sweep
    drew the previous layout (the log: `[session] change: layout=10` …
    `layout -> 10` on the render thread … `[lattice] sweep: layout 5`),
    and nothing re-swept until a size change. Pre-existing since step 45b
    (the session through the one serializer): `params_modify` kept the
    UI thread's copy current ("the UI thread's copy IS the probe's ground
    truth", DECISIONS_3 #47) but the patch path never did; a live layout
    switch in Play mode on the Tab has drawn the old lattice since, under
    a core that had already moved — the probe's touches answered the new
    layout a frame later, so the hand played the right cells under the
    wrong picture. THE FIX: `nativeSessionPatch` copies the patched
    params into the snapshot under the same lock (the host's `sim_scale`
    kept), before it returns; `apply_session` still refreshes it with the
    core's clamped copy after. The sweep and the session change log one
    line each now (`[lattice] sweep: layout … -> n cells`, `[session]
    change: layout=… key=…`), so the next such report reads off logcat.
    The iPad has no such gap: its `applySession` sets the core and reads
    the clamped params back on the main thread in one call (step 45b).
    VERIFIED on the Tab through the same path the picker uses: four live
    switches on the running app — the ring (`sweep: layout 8 arc 1 → 8
    cells`), the strings (`layout 11 → 150`), Jankó (`layout 2 → 252`) —
    each sweep the new layout's, the screen the Jankó lattice after the
    last; the author's session put back byte for byte.

21. **"We cannot play the partials with the S Pen while playing the
    valves or the slider" — Android's stylus palm rejection cancels every
    finger gesture the moment the S Pen comes within hover range of the
    glass and drops every finger event until it leaves; the panel's valves
    latch across that cancel now, released by the next touch on the panel,
    and a fingering change needs the pen away from the glass — a platform
    limit the iPad does not have.** The author, with step 64 on the Tab.
    THE MECHANISM, measured with virtual devices through
    `/system/bin/uinput` (a touchscreen and a pen registered with the
    Tab's own classes — INPUT_PROP_DIRECT, protocol B; BTN_TOOL_PEN — on
    the trumpet in Play mode, an instrumented build logging the play
    frame's dispatch; `tab/stylus/before_fix_logcat.txt`, the sequences
    `tab/stylus/*.json` from `uinput_gen.py`): (a) a finger holding valve
    pad 1, the pen brought into HOVER range over a partial — the frame
    receives `ACTION_CANCEL` (device 0, source 0) at the hover enter,
    before any pen touch; the panel released the valve, and the pen's
    touch 50 ms later sounded the open partial (77 where 75 was due); the
    finger's lift never arrived. (b) The pen hovering first and a finger
    pressing a pad during the hover: the press never reaches the app; a
    press after the pen leaves does. (c) The cancelled finger never
    revives: its moves and its lift after the pen has left are dropped
    until it lifts and presses anew. (d) A window focus loss (another
    activity on top) delivers the SAME cancel (device 0, source 0) — the
    cancel alone names no cause. THE RULE, read in AOSP's
    `InputState::shouldCancelPreviousStream` (inputflinger's dispatcher):
    "for compatibility, only one input device can be active at a time in
    the same window" — a new gesture from another device (a DOWN or a
    HOVER_ENTER) cancels the window's current one, and "because stylus
    should be preferred over touch" a stylus stream is kept while a touch
    stream that arrives under it is not tracked at all (`trackMotion`
    returns false, the event dropped as inconsistent); the branch is
    gated by the build-time flag `enable_multi_device_same_window_stream`,
    off on the Tab (SM-X906B, Android 16; the runtime flag
    `enable_multi_device_input` is on and does not cover it). The Tab's
    own dispatcher said so during the hover run: "Canceling pointers for
    device 75 in … com.vibetuned.midisink/…MainActivity". The dispatcher's
    stylus palm rejection proper (`GLOBAL_STYLUS_BLOCKS_TOUCH`) is a
    per-window bit for the system bars; the app's window carries none
    (`inputConfig=0x0`), and no window flag, setting or API opts an app
    window out of the one-device rule. THE FIRST FIX, rejected: the valves a framework cancel took went
    PENDING and, a moment later, LATCHED while a stylus was near (the pad
    half-filled), released by the next touch on the panel — the author:
    "worse than the previous version"; a latch that outlives the finger is
    not a valve. It is out. THE FIX, the author's idea (split the panel's
    input from the pen's): the rule is per WINDOW — an `InputState` per
    connection — so the panel is a window of its own
    (`MainActivity.syncPanelWindow`: a `TYPE_APPLICATION_PANEL` sub-window
    over the frame, `FLAG_NOT_FOCUSABLE | FLAG_NOT_TOUCH_MODAL |
    FLAG_SPLIT_TOUCH | FLAG_LAYOUT_IN_SCREEN`, translucent, no animation,
    laid in screen coordinates at the rect the frame would have given it;
    added on start once the decor has its token, moved with every
    `layoutPlaySurface`, removed on stop and destroy, present only while
    the panel is wanted — the brass layouts in Play mode), and the
    fingers' stream there never meets the pen's on the overlay. The view
    and its wiring are unchanged; `FingeringPanelView` keeps no latch — a
    cancel releases as a lift would, and the pen hovering over the panel
    itself is the one way to get one. The settings sheet (a Dialog, a
    window above the activity's) covers the panel as before
    (`panel_window_sheet.png`). No toggle: the window costs nothing when
    no pen is in use. VERIFIED with the same sequences on the fixed build,
    the panel horizontal under the finger (`after_fix_bytes.txt`,
    `after_fix_logcat.txt`, `pen_moving_bytes.txt`,
    `panel_window_trumpet.png`, `panel_window_trombone.png`): the finger's
    valve down, the pen's note 75 under it, the finger's LIFT arriving (CC
    110 up 1.4 s after the press — lost before); the pen first and the
    finger's press arriving under its note, released after; a press during
    the pen's hover arriving; the dispatcher logging no cancel against the
    app's windows where before it cancelled the activity's at every hover
    enter — and, the finger placed OFF the panel in one run, the same press
    dropped by the activity's connection ("dropping inconsistent event"),
    the rule exactly. WHAT A FINGERING CHANGE DOES UNDER A HELD PEN NOTE
    (`pen_moving_bytes.txt`): the pen's voice is not in the retune's held
    set (`nativePenBegin` carries no cell — the iPad's `penBegin` neither)
    and its pitch is the glide's, absolute: on the pen's next move the cell
    under it is re-probed under the new fingering and hostmpe's
    same-channel legato retrigger sounds the new note (On 75, Off 77 on
    the pen's channel; back to 77 when the valve lifts) — a re-articulation
    with the overlap idiom, not the fingers' 30 ms ramp. FLAG for the
    author: extending the ramped retune to pen voices needs hostmpe's
    glide to absorb the per-voice offset (step 62's library, both tablets)
    — a fix round of its own if the pen's legato should slur. WHAT STAYS:
    the pen hovering over the panel cancels the fingers on it; the strip's
    wheels share the activity's window with the pen (a finger on the pitch
    wheel under a pen note is dropped) — the brass panel was the ask. The
    instrumentation came out. ON THE REAL GLASS (the author's hands, the
    window shipped): "the panel is not working with the S Pen holding a
    partial — unreachable by a finger"; fingers on the partials and on the
    valves together work. Measured three ways (`tab/stylus/real_glass_*`):
    the kernel (`getevent`) reports the finger's contacts while the pen is
    down — the digitizer does not block it; the panel's own window
    receives the finger's DOWN and, 34 ms before the pen's touch, an
    `ACTION_CANCEL` carrying the touchscreen's device id and
    `FLAG_CANCELED` (the author's hint); the Tab's input log names the
    generator — at the pen's hover enter the InputReader itself emits the
    cancel for the touchscreen, then "Skipping touch event while pen is in
    use". That is the READER's stylus-over-touch rule (AOSP's
    `PreferStylusOverTouchBlocker`: a stylus going down or entering hover
    cancels every touch gesture with `FLAG_CANCELED` and skips touch while
    a stylus is active; a cancelled gesture stays dropped after the stylus
    leaves; the one exemption is a device reporting stylus and touch
    itself), below the dispatcher and every window — the virtual pen never
    tripped it, the real S Pen does (the Tab's build keys it on the pen).
    So on this Tab the pen plays alone: no window, flag or setting an app
    can reach lets a finger press a valve while the S Pen hovers or
    touches. The panel's window stands — it is the right structure (the
    dispatcher's per-window rule is real and the reader's rule is the
    build's; a build with multi-device input on skips the reader's) and
    fingers on both hands play the brass as before. `FLAG_CANCELED` on
    the cancel is the reader's signature, usable only for a latch, which
    the author rejected. THE WAY to play the brass with the pen on the Tab
    is the wire: the valves and the slide are the fingering CCs (step 60:
    CC 110–112, 113 on the master channel) from any controller — a pedal,
    a pad, a keyboard's keys mapped to them — which no stylus rule
    touches; the panel then mirrors the engine's state as it does today.
    The author's Tab session was overwritten by the restore rounds (the
    snapshot was the step's first; "please stop or at least take a new
    snapshot") — no restore from a stale snapshot again, a fresh one right
    before a lab launch or none.

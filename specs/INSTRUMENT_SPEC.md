# INSTRUMENT SPECIFICATION: Stateful & Idiomatic Layouts
**Phase 8, input side. Companions: `PROJECT_SPEC.md` (the medium's section drafted in `TO_PROJECT_SPEC.md` until transcribed), `QUALITY_OF_LIFE_SPEC.md`, `SOUND_SPEC.md`. Iteration expected — open points are marked `[ITERATE]`. The ABI half of §1 (the probe's state argument, the cell flags) shipped in Phase 6's step 41 (`DECISIONS_5 #45`).**

---

## 1. The probe-state design (the architectural question, answered)

Valve and slide layouts are **stateful**: the note under (x, y) depends on the current valve combination or slide position, not on geometry alone. The instance-free probe — the design that saved Android's touch latency — must survive this.

**Design: state is an explicit snapshot argument, and state changes are MIDI.**

```c
typedef struct {
    uint32_t buttons;      /* bitmask: valves, register keys (bit 0 = valve 1 …) */
    float    slider;       /* continuous control position 0..1 (trombone slide)  */
    uint32_t reserved[2];
} sumi_layout_state_t;

SUMI_API bool sumi_layout_probe(uint32_t layout, const sumi_params_t* params,
                                float aspect, const sumi_layout_state_t* state,
                                float norm_x, float norm_y, sumi_cell_info_t* out);
```

* The probe stays a **pure function** — `(layout, params, aspect, state, x, y)` — callable from any thread; shells keep the state snapshot beside their params snapshot (they own every state write anyway). Stateless layouts ignore `state` (pass zeros). This is the Phase-6 ABI event, batched with MEDIUM_SPEC's `medium` param.
* **State changes travel as MIDI**, so fingering is recorded and replayed like everything else: valves as CCs (proposal: CC 110/111/112, ≥64 = pressed `[ITERATE: numbers]`), the slide as a high-resolution CC `[ITERATE: 14-bit CC pair vs 7-bit + smoothing]`. The normalizer decodes them into layout state on the engine side; the shells mirror the same bytes into their local snapshot — one source of truth (the byte stream), two synchronized readers. A DAW recording of a trumpet performance replays the *fingering*, not just the pitches.
* The engine's own copy of layout state drives any visual echo of fingering `[ITERATE: should pressed valves render? proposal: the strip's widgets show it; the canvas stays pure ink]`.

---

## 2. Trumpet layout (`SUMI_LAYOUT_TRUMPET`)

The real fingering isomorphism: **partials × valve combination.**

* **Cells: the harmonic series** — 8 partial cells (fundamental through the 8th partial a real Bb trumpet speaks), arranged `[ITERATE: vertical column low→high, or arc?]`. Touching a cell selects the *partial*; the sounding note = partial pitch − valve offset.
* **Valves: three strip buttons** (momentary, the existing widget) emitting the valve CCs. Offsets: valve 1 = −2 semitones, valve 2 = −1, valve 3 = −3; combinations sum (1+2 = −3, 1+3 = −5, 2+3 = −4, 1+2+3 = −6) — the seven real positions plus open. Real combinations' intonation quirks (1+3 sharp, etc.) are **deliberately not modeled** — this is the idealized instrument `[ITERATE: a "realistic intonation" toggle is a cheap flavor option]`.
* **Expression:** Y in the cell = the existing bipolar press axis (embouchure as ink feed / swirl); X = per-note bend (lip bend, ±1 semitone default scaling on this layout `[ITERATE]`).
* Changing valves while a voice sounds retunes it: bend ramp over 20–40 ms (the PIANO_GRID cell-quantized machinery, reused verbatim) — real valve legato.

## 3. Trombone layout (`SUMI_LAYOUT_TROMBONE`)

* **Cells: partials** (7 typical); **slide: a dedicated wide latch-slider** on the strip (a new widget variant: positional, not accumulating — the hand IS the slide) emitting the high-resolution slide CC. Positions 1–7 rendered as tick detents, **continuous between them** — sounding pitch = partial − slide semitones (0..6 continuous), so playing between positions is simply in tune with itself, exactly like a real trombone and unlike every quantized software one.
* Glissando is the identity gesture: hold a partial cell, move the slide — continuous bend across up to 6 semitones, no re-anchor needed.
* Slide state is one float — the probe's `slider` field.

## 4. Stateless additions (cheap, ride along)

* **Wicki–Hayden hex** (`SUMI_LAYOUT_WICKI`): the other great isomorphic layout; hex cell math beside Jankó's, one echo, all existing machinery applies.
* **Fretboard** (`SUMI_LAYOUT_FRETS`): 6 string-rows × fret columns, standard tuning `[ITERATE: tuning table in params or fixed?]`; dragging along a string is *literally* a string bend (per-note bend along the row axis) — the most instantly-understood MPE layout that exists.
* **Theremin** (`SUMI_LAYOUT_THEREMIN`): no cells — continuous X = pitch (the legato re-anchor machinery), Y = the bipolar press axis. The anti-grid: pure expression, and the natural stage for the ripple and torsion operators.

## 5. Surface & probe consequences

* `sumi_cell_info_t` is unchanged for trumpet/trombone (cells are still cells); theremin signals "continuous" `[ITERATE: cell_radius = 0 sentinel vs. a flags field]`.
* Hit-testing, joystick, legato, and echo machinery are untouched — stateful layouts change *which note* a cell means, never how touching works.
* Golden tests extend to state sweeps: every valve combo × every partial; the slide at detents and midpoints.

## 6. Explicitly deferred within this spec
Woodwind key systems (Boehm charts — real but a much larger state space), microtonal/scala tunings `[ITERATE: cheap on theremin and frets — pull forward?]`, two-handed split layouts (different layouts per screen half).

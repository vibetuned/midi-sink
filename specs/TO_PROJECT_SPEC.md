# TO PROJECT SPEC — what Phases 6 and 7 add to `docs/PROJECT_SPEC.md`
**Written at the Phase-6 close (2026-09-26) for the author to transcribe. `specs/CONTEXT.md`, `specs/MEDIUM_SPEC.md`, `specs/chladni.md` and `specs/spark.py` were removed from the tree at the same time (git history keeps them: `git show 76e2f5e:specs/MEDIUM_SPEC.md`, `…:specs/chladni.md`, `…:specs/spark.py`, `…:specs/CONTEXT.md`); the text below is MEDIUM_SPEC corrected to what shipped, plus the quality-of-life items that shipped, with the decision entries that hold each fact (`DECISIONS_5 #n` = `docs/DECISIONS.md` Part V). Where this draft and the merged Part V disagree, Part V is the record.**

Suggested placement: a new **§10 Media** and **§11 Quality of life** after PROJECT_SPEC §9 (the Phase-5 spec), with §4.3's operator list gaining the five new operators and §5.3 (the ABI history) gaining 1.0.0 / 1.1.0. CONTEXT.md's "architecture pillars" (§2 there) are already PROJECT_SPEC's §2–§5 and need no transcription; its acknowledgments registry (§5) belongs on the documentation site's citations page (step 63).

---

## §10 Media

### 10.1 The media abstraction (as shipped, step 41–42)
The engine is symplectic — bijective, det J = 1, inverse-lookup maps composed on a coordinate field — and suminagashi was its first *medium*, not the engine itself. **The engine owns** the ping-pong field and pass machinery, the whole operator library (every medium may use every operator; media choose defaults), the normalizer and voice model, layouts and the play surface, hostmpe, the strip, transports, dip/readback, the §4.6 orientation discipline, the budgets and the delta rule. **A medium owns** the composite pass (how the field is *read*), its palette family, its print styling and its **default binding table** (§10.5).

ABI: `sumi_params_t.medium` — `SUMI_MEDIUM_SUMI` 0, `SUMI_MEDIUM_ANOD` 1 (values above clamp to Sumi); it landed in the one ABI break of the arc, `libsumi` 1.0.0, with `sumi_layout_probe`'s state argument, `sumi_cell_info_t.flags`, `sumi_set_palette` and the reserved layouts 8–12 (#45–#47). The medium is named **Anod** (#1); Ichisuke Fujioka is honoured in the acknowledgments regardless.

**Switching is live and a feature** (#53): the field is medium-agnostic (coordinates + phase + aux), so a switch re-reads the same deformation history — a marbled sheet as a discharge record; the field is bitwise across a switch. The dip stays one key away for a fresh sheet. In Anod the ink phase reads as **charge phase** (filament banding) and aux as the per-event hue.

### 10.2 Operator classes and the conservation gate (step 35)
Every operator, present and future, declares one class in its header comment, its test and its book page:

| Class | Definition | Members | Discipline |
|---|---|---|---|
| **Exact** | det J = 1 at any magnitude; closed-form invertible as-is | drop, tine, vortex (all four profiles), pinch (saddle), ripple, swirl, scroll, torsion, Chladni (disc mode), spark shear, Chirikov | no sub-stepping; ±k pairs invert analytically |
| **Sub-stepped displacement field** | an exactly divergence-free field applied as a finite step — area-preserving to first order | wake, viscous multipole burst, Chladni (field mode, "Inverse Chladni") | per-pass displacement ≤ a fraction of the core (the wake's ≤ a/4 rule; the burst's β_m); soaked under the wake's numbers |

A composition inherits the strictest class of its members (the composed spark is sub-stepped by its burst, #37). **The four-part gate** (`midi-sink --dev --soak <operator>`, #13–#17, #21): (a) the class stated; (b) ±k pairs invert — mass within ±5 % and the pre-image within 8 texels over 500 strong pairs (sub-stepped: one a/4 step keeps the pre-image Jacobian > 0.5, mean within 2·10⁻³ of 1); (c) zero fabrication — mass never grows past 0.5 % or 5·10⁻⁵ per pass, the route proven alive; (d) per-pass erosion ≤ 2× the glide tine's steady 1.15·10⁻⁵. Three negative controls prove the gate red before it is trusted. Findings recorded: the resampled medium gains at ink boundaries under strong pairs (#15); the crossed pinch's ±k is not an inverse (#17, left to the author); drops are excluded by nature — an exact expansion pushes ink off the canvas, which no mass observable can tell from loss (#13, #39).

### 10.3 The five operators, as shipped
* **Wave torsion** (`SUMI_VORTEX_TORSION`, step 36, #18–#20): θ′ = θ + A·sin(k·r − φ)·e^(−r/R), r′ = r — a third vortex profile (exact), reached through every vortex route; k and φ are controls (`SUMI_CTL_TORSION_K` / `_PHASE`, CC 104/105 by default); **the note-on sweep episode** (`params.torsion_sweep`): per-voice, φ advancing at 2π·1.5 rad/s, amplitude 1.2 rad/s·e^(−t/0.6 s), reach 3× the strike radius floored at 0.05, over after 4τ, per-frame deltas merged under the budget — the pattern the burst and the spark reuse. The **press feed** in Anod spends torsion at 1.2 rad/s at full pressure with its own phase clock (#51).
* **Chladni — the cells are the eddies** (steps 37 and the rework before 43, #23–#27, #59–#62; the roadmap's kick-drift lattice and the Taylor–Green flow were superseded): every display disc the shells draw (`sumi_layout_cells`: the key circles; the polar lattice for the fifths; imaginary circles for the rolls, scaled by `chladni_cell`, capped at the display size so discs never overlap) turns about its centre as a ring — θ(ρ) = θ·(4ρ²(1−ρ²))², still at the core and the rim, peak 0.727 at ρ = 1/√2 — through an RGBA16F index map at field resolution (four owner slots); the odd cells' sense from the balance (0 counter-rotate, ½ rest, 1 all the same way). `chladni_mode` 0 **discs** (exact) / 1 **Inverse Chladni** (the rings summed into one sub-stepped flow that may grow past the keys, `chladni_cell` to 1.5). The stir is banked and emitted only when it carries at least the field's quantum on the smallest disc (#61); its rate 1.5 rad/s at full control; **signed** — the bend's sense (#70). The author's ponderomotive derivation (the Kapitza averaging of a vibrating plate into a conservative Hamiltonian and its kick-drift map) is `specs/chladni.md` in git history — worth a paragraph in the book, not the spec.
* **Viscous multipole burst** (`SUMI_DEFORM_BURST`, step 38, #28–#33): the time-integrated displacement of an impulsive viscous multipole — Jaffer's Lamb–Oseen integration applied to the sin(mθ) family, elementary for m ≥ 2 (Φ_m(S) = (1/S)[1 − e^(−S)Σₖ₌₀^(m−2)(1 − k/(m−1))Sᵏ/k!]; the dipole's E₁ kernel excluded), the dual-time Gaussian core (ℓ² = a² + 4νt), D normalised from the peak lobe at r = a, the far field cos mθ/r^(m−1) then 1/r^(m+1) beyond ℓ (a precision on the draft). Class sub-stepped: peak displacement ≤ β_m × the pass's core (β₂ = 0.13). The gesture is a strike with a lifetime: `sumi_add_burst(x, y, a, D, θ₀, m)` starts an episode whose age grows to `burst_age`·a over `burst_life` seconds; m = 0 takes `burst_order` (2..8). The pinch is this burst's r → 0 limit. The literature check preceded any wording (`docs/evidence/step38/literature.md` in git history); the book page carries the lineage line and the author's signed note.
* **Spark shear** (`SUMI_DEFORM_SPARK`, step 39, #34–#39): a kick-drift shear — rows slide by A·w(y)·f(y), then columns by B·w(x₁)·f(x₁) — with f a stack of triangle waves at k, 2k, 4k, 8k (`spark_stack`) or piecewise-linear hash noise (`spark_profile`; an integer hash, bit-identical across backends), w a Gaussian window across each shear, the frame rotated by θ₀. **Exact for any profile** (the draft's "y₁ = y₁ + …" was a typo for y₁ = y + …). The episode: A = B = `spark_shear`·r spent as e^(−t/τ) over 4·`spark_tau`, dealt in kick-drift steps at the field's quantum, the window 2r, k from `SUMI_CTL_SPARK_K` (`slide_mode` 2 routes CC 74 to it). **The composed spark** `sumi_add_spark(x, y, r, D, θ₀, layer)` = the drop (the Joule blast: radial outflow cannot be divergence-free, the oldest operator solves it exactly) + the burst of core r and lobe D + the shear episode; sub-stepped by inheritance.
* **Chirikov standard map** (`SUMI_DEFORM_CHIRIKOV`, step 40, #40–#44): the kick y₁ = y + A·sin(k(x−x_c)+φ) then the ε-scaled drift x₁ = x + ε·(y₁ − y_c), K = A·k·ε; exact, the inverse undoing the drift first. The route (`SUMI_CTL_CHIRIKOV_K`, CC 109; the mod wheel in Anod) is **delta-driven**: a throw of δ is one step at δ²·`chirikov_kmax`, capped per step at the core's ceiling `SUMI_CHIRIKOV_K_CEIL` = 1.25 (the last K where erosion after the flush holds, above Greene's 0.9716) with the remainder following; a negative δ the exact inverse. The erosion sweep found the drift flushing rotating orbits off a non-wrapping canvas at every K (#43) — the operator's geometry, not resampling. **Open (the author, #89):** the feel — "the vibrations are tiny and the displacement is huge; I was hoping for the contrary"; a parameter grid may follow.

### 10.4 The Anod composite (step 42–43, #50–#58, #69)
**Read the field's strain, not its phase bands.** The charge glows 0.22 + 0.78·(1 − e^(−σ/`anod_glow`)), σ = sqrt(‖J‖_F² − 2) from the stored coordinates by a one-sided-min finite-difference estimator that survives the half-float staircase (the stencil widens with the field, the rounding bias is subtracted; #56) — the charge phase bands the filament, aux drifts its hue. **Water never glows by strain: it draws the field's deformed grid** — iso-lines of position + gain·displacement (gains −4 along x, +3.4 along y) from window-averaged displacements, shown where the water has moved by more than a texel, faded where the local pitch would alias (#57); `anod_pitch` is the grid's pitch at rest (default 1/144, 0 = no grid, #58). **The seam mask is found, not stored** (#52): a texel at its own identity coordinates within one ULP with no phase is fresh water, and a charged texel beside it reads no strain. Substrate: near-black glass (`anod_dark`, default 1) with the washi's screen-locked grain as phosphor speckle (`anod_grain`, 0.5) — the §4.5 invariant holds by construction. **The bloom** (#69): the Anod composite rendered once more in linear light at half resolution, the emission above a 0.04 threshold blurred over `anod_bloom_levels` octaves (13-tap down, tent up) and added back times `anod_bloom`, then a per-channel shoulder toward white; prints and exports bloom the same. The author's look: glass 1, grain 0.5, glow scale 0.20, bloom 0.75 over 3 octaves. Three Anod palettes (electric blue, plasma orange, phosphor green) under the same ids; a custom palette reads as a glow (gradient by glow = the core, the accent = the halo).

**Prints** in Anod are the same readback, medium-styled (#54); the export ABI (§11.4) writes them over alpha on request. The `[ITERATE: long-exposure / strain accumulation buffer]` is carried to the Phase-9 beta with its question.

### 10.5 The default binding tables, as shipped (#51, #70–#72, #88)

| Dimension | Sumi default | Anod default | Override |
|---|---|---|---|
| strike (note-on) | the drop | **the classic spark on the charge**: a drop of radius·`anod_drop` (default 0.57), a burst of that core (D = 0.3 r, order `burst_order`) and the shear episode on the charge (#88; `burst_order_by_class` stays in the ABI, unused) | `anod_drop`, `spark_shear` |
| press feed (0xD0) | boundary growth (`press_mode` 0) | the torsion sweep feed (`press_mode` 2) | `press_mode` 0 / 1 |
| poly pressure (0xA0) | the Lamb–Oseen swirl | the torsion's and the spark's wavenumbers from mid-range up (k = ½ + ½·pressure), held while pressed and given back at release, each unless the bend or the slide owns it (#70, #72) | a CC shares the slot, last writer wins |
| per-note bend | glide (`bend_mode` 0) / ripple (1) | **the Chladni stir** (`bend_mode` 4): the bend's distance the rate (±1.5 semitones saturate), its sign the sense; stilled when the last bent note lifts or the mode flips away (#70, #72) | `bend_mode` 0–3 |
| CC 74 (slide) | aux hue (`slide_mode` 0) / pinch (1) | the spark's frequency (`slide_mode` 2) | `slide_mode` 0 / 1 |
| mod wheel / breath | the vortex | the Chirikov throw (the vortex quiet) | the CC map |
| master bend | the shear tine | the shear tine (it composes with the scroll already) | — |

`SUMI_MODE_MEDIUM_DEFAULT` (255) is the default of the three modes and resolves to the medium's column; an explicit mode is the user's override. **A mapping is a handle, not a claim** (#72): the desktop maps CC 104–109 to these dimensions by default, and a mapped, silent CC never owns one — last writer wins.

### 10.6 The marble gestures follow the medium (#75)
Five core calls, `sumi_gesture_tap / _pinch / _twist / _press / _press_end`, each reading `params.medium`; in Sumi each is exactly the 1.0 operator call (bitwise); in Anod: tap = the strike (§10.5's, along the layout's pitch axis at the touch), pinch = the burst (the squeeze accumulates, a burst per 0.06 of it, core a quarter of the finger span), twist = the torsion vortex, long press = the torsion feed (pull = the Chladni stir, reversed; `_press_end` stills it). The comb and the pen's wake are the same in both media. Every shell calls them.

---

## §11 Quality of life — shipped (step 43 and the shells)
* **Palettes** (#63–#65): one model, `sumi_palette_t` — 2..8 linear-RGB stops along the ink-depth axis, the depth curve (γ, floor), `hue_drift` toward `accent_rgb`, `clear_rgb` — through one shader path; the built-ins are presets in it (bitwise-proved); a library of twelve per medium (`sumi_palette_preset_count / _preset`: the three built-ins, Cobalt & amber — Okabe–Ito — Viridis and Cividis); the morph ring includes the custom slot (custom → the three built-ins); the curve is fixed per medium (the "advanced fold" deferred). Editors on every shell.
* **Substrate** (#66): `paper_tint[3]` (cream / white / toned presets), `fiber_scale`, `anod_dark`, `anod_grain` — composite-side, screen-locked by construction, bitwise at their defaults.
* **Presets** (#67, #73, #74, #79): one serializer, `presets/` (pure C11, no allocation): params by name, the input dialect, the custom palette, the CC map, the routed controls' values, the strip's wheel assignments, the layout-state defaults — JSON schema 1 stamped with `sumi_version`; unknown keys ignored, missing keys defaulted (`presets/SCHEMA.md`). The desktop's INI keeps only the shell's own keys; `last_session.json` restores at launch; named presets are files (the desktop's config folder, the iPad's Files, Android's SAF, the web's localStorage with export). A preset made on one shell loads on every other byte-for-byte.
* **Prints** (#68, #77, #84–#86): `sumi_read_field` (the field as it stands), `sumi_export_begin / _poll` (the same field composited at any size up to 8192, `SUMI_EXPORT_ANOD_ALPHA`); the print ledger on every shell — dips kept with their field and look, thumbnails, re-export at Screen / 2K / 4K / 8K; the copy states the bound honestly (detail below a field texel is interpolation; the true re-dip is replay). A background write reports its outcome in the UI.
* **The copy** (QOL §6): "Dip the paper — keep the print" / "Clear the canvas — discard" on every shell; the play surface draws light on Anod's glass.

---

## §12 Sound — Voxo, the internal MPE sampler (Phase 7, steps 47–55)

Drafted from `specs/SOUND_SPEC.md` as it shipped; where the two differ, the
decision named is the record (`DECISIONS_6 #n` = `DECISIONS.md` Part VI).

* **A sibling core, not a libsumi feature** (#2). `voxo/` beside `core/`:
  `voxo/include/voxo.h` is pure C (the `sumi_core.h` rules verbatim — a
  three-state `VOXO_API`, `voxo_version`, no STL, no exceptions, no
  callbacks-into-C++ across it; a `module.modulemap` for Swift), `voxo/src/`
  C++20 without exceptions or RTTI, a static archive. It compiles
  `core/src/midi_normalizer.cpp` from source and never links `libsumi`: one
  MPE decoder, two builds, zero runtime coupling. The shell's ONE MIDI
  producer fans the identical bytes into `sumi_push_midi` and
  `voxo_push_midi` (the desktop through the harness's tap, the iPad through
  the canvas's `push()`, the Tab through `shell::push_midi`); Voxo OFF is the
  1.x app. Voxo keeps its own pool of 16 performance voices keyed by
  (channel, note); the core's voice mapper is not shared.
* **The callback-thread contract** (#3), written at the top of `voxo.h`
  and tested by a counting global allocator: on the audio thread Voxo
  allocates nothing, frees nothing, takes no lock, logs nothing, makes no
  blocking call; it drains the normalizer's wait-free ring ONCE at block
  start and applies every voice transition there in event order; the shell's
  settings (gain, dialect, interpolation, Local Control, the instrument, the
  CC routes) arrive through atomics read at block start; the instrument is
  published by a pending → current → retired swap, the shell freeing the
  retired one. `voxo_render` is the callback's whole body and runs without a
  device for tests and bounces.
* **The backend is miniaudio 0.11.25** (#5, #7, #8), one implementation TU,
  Objective-C++ on Apple (no dlopen there), runtime-linked ALSA / Pulse /
  WASAPI / AAudio elsewhere; the SHELL owns the iOS session (playback,
  48 kHz, a 128-frame IO buffer) and Android's audio focus; the AAudio
  buffer is two bursts grown one per underrun by a tuner the stats query
  paces (#7, #28). **Block sizes** (#4, #9, #33, #39): macOS 128 as asked;
  iOS 128 (2.667 ms IO buffer, 9.7 ms session output latency); Android
  AAudio's burst (192 on the Tab, 4 ms; touch-to-DAC ~32 ms); Windows 256
  asked, WASAPI shared mode's 480 engine period granted with the callback
  kept at 256; Linux 256 through PulseAudio on PipeWire's shim. The mobile
  escape hatches (direct AAudio, AVAudioEngine) were not needed.
* **Voice model** (#10, #16, #17, #25): 1:1 MPE dispatch — a performance
  voice per (channel, note) stacking up to eight LAYERS: the preset's zones
  matching the note and velocity, velocity layers crossfading at equal power
  over overlaps, round robins by sequence position, random, release samples
  on note-off; each layer an ADSR (linear attack, one-pole decay and
  release), the read at 2^((note − root + bend)/12) × rates recomputed per
  block and ramped linearly per sample, **4-point Hermite** (linear kept as
  the lab's comparison; the glide check's reason: content at 0.22 of a
  sample's Nyquist reads 12.8–14.8 dB cleaner, at 0.44 only 7 dB), the loop
  with an equal-power crossfade, a 2-pole state-variable low-pass. Under MPE
  the master channel's pedal, bend, pressure and CC 74 reach every member
  (#25). Expression: pressure and CC 74 smoothed per voice by the preset's
  rising/falling times; the defaults pressure → 0.35 + 0.65 p, CC 74 → the
  cutoff × 2^((t − 0.5) × 6); the preset's `<mpePressure>` / `<mpeTimbre>`
  bindings override; swirl (0xA0) a source with no default target and no
  preset syntax. Sustain in the release logic; CC 120/123 the panic; Local
  Control (CC 122) tracked by Voxo and APPLIED by the shell, which stops
  fanning its own play surface's bytes (#12); internal sound and outbound
  MIDI are independent switches.
* **The format, honestly bounded** (#13–#15): pugixml over `.dspreset`,
  miniz over `.dslibrary`, dr_wav / dr_flac and an AIFF/AIFF-C PCM reader of
  our own (the stock Basic Piano ships AIFF — the `[ITERATE]` answered);
  parsed: groups (the cascade `<groups>` → `<group>` → `<sample>`), zones,
  ADSR, ampVelTrack, loops, seqMode/seqLength/seqPosition, trigger, tags, the
  low-pass, reverb and delay parameters, `<midi>` cc / note / velocity
  bindings, `<modulators>` mpePressure / mpeTimbre, the UI controls'
  starting values (through their bindings, `TAG_VOLUME` included).
  **The compat report** — the summary line, then one canonical sentence per
  note (memory, missing samples, streaming, chorus, convolution, EQ and other
  filters, an unknown effect, modulators, note sequences, an unknown
  binding, the custom UI) in a documented order; `voxo/COMPAT_REPORT.md` is
  the copy and a test asserts it verbatim; refusals only for what is not a
  preset (the reason given). An hour of mutation fuzzing (25 M loads) found
  no crash. Every number clamped or defaulted.
* **Memory: preload-first, an advisory gate, foreground only** (#20, #23,
  #28): everything decodes to float in memory; before decoding, the size is
  read off the sample headers and compared with the shell's advice (the
  desktop's free memory × 0.6, iOS `os_proc_available_memory` × 0.6,
  Android the activity manager's `availMem` × 0.6); over it, the memory
  note leads the report and the load proceeds. The desktop loads on a worker
  thread; the tablets on a background queue with a progress row. Foreground
  only: iOS stops the device on backgrounding and on an interruption,
  restarts on return / resume / a route change — the plist keeps the `audio`
  background mode CoreMIDI needs (#23, the author's call in transcription);
  Android holds audio focus while resumed and yields to a call.
* **The bus** (#19, #27): Freeverb and a feedback delay after the sum,
  buffers allocated once per rate, the preset's parameters and knob
  defaults, `<cc>` bindings on `FX_REVERB_*` / `FX_DELAY_*`; and the shell's
  one CC map carries six bus targets numbered from 1000 (`presets/SCHEMA.md`)
  through `voxo_map_cc`, switching the effect on for a preset without one
  (#31: every loader lets them through).
* **The shells** (#21–#24, #26–#30): the desktop's Sound section (switch,
  volume, the WAV sample row, the instrument row with its report and the
  memory advice, the demo button, the status line); the iPad's Sound page
  (the instrument list from Documents/Instruments, Files import of a
  `.dslibrary` or a preset's folder, the declared Decent Sampler types so
  AirDrop and "Open in" hand libraries over, Local Control); the Tab's Sound
  page (the Storage Access Framework's document and tree pickers, "Open
  with", the demo in the assets, the instrument list in the app's
  Instruments folder); the play surface hides the cells outside the loaded
  instrument's reach while the sound is on (`voxo_covered_notes`); the
  desktops follow the default output through a device switch without a stop
  (#33, #41 — output-device SELECTION was not built). **The demo instrument
  is the VCSL Dan Tranh, CC0** (#22): sixteen samples of the f layer at
  32 kHz, 2.9 MB, stretched across the keyboard, bundled by every shell so a
  first launch makes a sound; nothing else is bundled — libraries are the
  user's and travel with their own terms (the licensing page's premise:
  formats are not copyrightable).
* **The acceptance suite** (#32, #34, #39): `midi-sink --dev --voxo-preset
  <heavy library> --voxo-storm <s>` — the fifteen-channel MPE storm with a
  heavy library loaded on the real device; pass = 0 XRuns (Voxo's proxy; the
  platform's own count the truth where it has one — AAudio's, PipeWire's),
  0 dropped, the visual loop at or above 58 fps; the period reported, not
  demanded. Green on the three desktops.
* **Deferred, designed for:** Voxo Dorean (SOUND §7 as written) — background
  execution and disk streaming in a sister app around the same library; the
  web (SOUND §4).

## Pointers and open points for the author
* `specs/spark.py` was the author's matplotlib reference for the glow (three strokes: a wide faint cyan, a neon mid, a white-hot core); the bloom (#69) is its engine form. Git history: `git show 76e2f5e:specs/spark.py`.
* `specs/chladni.md`: the ponderomotive derivation — for the book's Chladni page (step 63), one paragraph of physics beside the shipped operator.
* Open: the Chirikov feel (§10.3); the local spark (#72, not pursued — the author keeps the stir as it is); the field stored as a displacement (roadmap step 55b); the crossed pinch's inversion (#17); the long-exposure look (§10.4).
* `specs/SOUND_SPEC.md` shipped as §12 above (with the corrections named); it is the author's to transcribe and remove, as `MEDIUM_SPEC.md` was at the Phase-6 close. Open in it: SOUND §4's "no `UIBackgroundModes`" against the plist (DECISIONS_6 #23); output-device selection (not built — the default follows); the demo recording over the Dan Tranh.

# Part 5 — v2.0 (steps 35–66: Medium, Sound, Instruments, Publish — Phases 6–9)
**DRAFT**
**Companions: `specs/CONTEXT.md` (the map), `specs/MEDIUM_SPEC.md` (`MEDIUM §n`), `specs/SOUND_SPEC.md` (`SOUND §n`), `specs/INSTRUMENT_SPEC.md` (`INSTRUMENT §n`), `specs/QUALITY_OF_LIFE_SPEC.md` (`QOL §n`); the standing engine rules are `docs/PROJECT_SPEC.md` (`SPEC §n`: §3 MIDI, §4 engine and §4.6 orientation, §5 hosts, §8 play surface); decision logs `_work/DECISIONS_5.md` … `_work/DECISIONS_8.md`, one per phase, referenced as `DECISIONS_5 #n` …; history `docs/CHANGELOG.md` (v2.0.0).**
**Scope: four phases, one public release. Phase 6 Medium (35–46): the Anod operators, the single ABI break, the Anod composite, palettes, presets and prints, then the shells. Phase 7 Sound (47–55): Voxo, the sibling library. Phase 8 Instruments (56–62): the stateful probe in use, five layouts, the strip widgets, session replay. Phase 9 Publish (63–66): the documentation for everything, the beta wave, the feedback loop, the 2.0 release. Hard ordering: the ABI break (41) precedes everything that reads the medium, a custom palette or the probe state — and it carries INSTRUMENT §1's state argument two phases early so the break stays ONE; Voxo before replay so replay re-sounds (QOL §5) in a single pass; documentation for new features last, because the live `/marble/` is the newest stable tag (DECISIONS_4 #82) and the operator book's demos need the release wasm. Quality-of-life items ride the phase they belong to (QOL §7): palettes, substrate, presets and prints with the medium; panic, quick-switch, mirroring, per-device presets and replay with the instruments; the copy fixes wherever the shell steps are. Each phase ends with a pre-release tag on the test tracks; no external beta before Phase 9. Thirty-two steps: one per session, as always.**

---

## Working Rules (apply to every step)

* All prior working rules hold. The core is reopened for FEATURE work by this roadmap in Phases 6 and 8 only; Phase 7 never touches `libsumi` (the engine stays audio-free — SOUND §1); Phase 9 reopens it for fixes under bug → regression test → fix. Core changes prove out on the desktop harness FIRST, every time.
* **The phase invariant:** `tests/fixtures/field_512_metal.bin` stays BITWISE on Metal from step 35 to step 66. New operators add passes, media change the composite, layouts change the probe — none touches an existing pass. A step that believes it must change the fixture stops and records the decision first.
* **Every operator declares its class** (MEDIUM §2 table) in its header comment, its test and its operator-book page: *exact* (det J = 1 at any magnitude; proven by a ±k inversion golden) or *sub-stepped displacement field* (soaked under the wake's ≤ a/4 rule and the four-part conservation gate of step 35). Membership is declared, never discovered in a failing soak.
* The delta rule (continuous controllers drive deltas per pass) and the one-consumer rule (`bend_mode`, `slide_mode`, `press_mode`) are unchanged; media add *defaults* for them, never a second consumer.
* **One platform per step.** Core and shared UI are authored on the desktop harness (the Mac); iOS on the Mac; Android and Linux on the Linux box; Windows on its box. A step never touches a second platform's build or store; the other shells consume in their own steps. **Sanctioned exception — verification fan-out:** a step may have OTHER boxes re-run an already-green suite unchanged (step 55's pattern); authoring stays single-platform.
* **Composed gestures inherit the strictest class of their members:** a composition containing a sub-stepped pass (the spark's burst component) gates under the sub-stepped family's numbers, even when its other members are exact.
* **The ABI event is ONE step (41).** Before it, growth is additive only (new enum values, new `sumi_add_*`/ctl dims, appended params fields — the Step-33 minor-bump pattern, `sumi_version` 0.10, 0.11 …); after it, additive only again. `libsumi` becomes **1.0.0** at the break. The prebuilt SDK stays deferred until Phase 9 asks the question.
* Evidence per step under `docs/evidence/<step>/`; at each phase end the fold: that phase's `_work/DECISIONS_<n>.md` merges into `docs/DECISIONS.md` as the next Part, evidence condenses into `CHANGELOG.md` and leaves the tree (git keeps it), scripts worth keeping move to `tools/`. `site/scripts/build-notes.mjs` renders `_work/DECISIONS_{5,6,7}.md` while in flight — Phase 9 extends the loop to 8.
* **Documentation timing:** guide fixes ship to `main` at any time (`pages.yml`). Pages for NEW operators, layouts and Voxo are drafted in the step's evidence folder (the burst page in the author's voice) and move into `site/` in step 63 — the live demos would otherwise point at scenes the released wasm does not know.
* **Pre-release tags** end Phases 6, 7 and 8 (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a pre-release, and the lanes stay proven); the author installs the build on every device and plays it. Phase 9 uses `v2.0.0-rc.N`. Nothing reaches a stable channel before step 66.
* Credentials: Phases 6–8 need none beyond the machines; Phase 9 reuses the Phase-5 set (Developer ID, ASC, Play, tap token, winget token, apt key). Author inputs (recordings, taste sign-offs, the demo instrument) are listed per step so they can be staged before the session.

---

# Phase 6 — Medium (steps 35–46)
**Order inside the phase:** the conservation gate (35) before any sub-stepped operator; torsion (36) first because it is exact and cheap and proves the pass plumbing; Chladni (37), burst (38), spark (39), Chirikov (40) as MEDIUM §5 sketches; the ABI break (41) once all five operators exist additively; the Anod composite and binding tables (42); palettes/presets/prints authored once (43); then the shells (44 iOS, 45 Android, 46 web) consume.

## Step 35 — Phase opening & the conservation gate (desktop machine)
**Spec:** MEDIUM §2 (class table); SPEC §4.1, §4.3(5); DECISIONS_3 #33 (the four-part gate). **Author inputs:** the medium's name (Anod — decided 2026-09-21), the release number (2.0.0), the slide-CC resolution (see Phase 8 head).

* Open `_work/DECISIONS_5.md`; its first entries record the decisions above and this roadmap's provisional resolutions the author confirms (the list at the end of this file).
* Generalise the step-19 pinch soak (`t19_pinch_soak` in `desktop/src/dev_tools.cpp`, reached through `dev_run_scripted`) into a **per-operator four-part gate**: (a) det J = 1 stated symbolically per operator (a header comment with the derivation, or the class declaration for sub-stepped fields); (b) ±k pass pairs invert — ink mass (Σ phase) held to ±0.5% over 500 strong pairs; (c) zero fabrication — mass never grows > 0.5% under the gesture-rate stream; (d) per-pass erosion ≤ 2× the glide-tine baseline under the identical stream. Parameterised by operator and driven through the REAL ctl/gesture path on the fixed 512² scripted clock; one `--dev` flag per operator soak; `tools/soak_report.py` turns the printed lines into the evidence table.
* **Baselines** recorded for every v1.0 pass (glide tine, pinch saddle and crossed, wake doublet and Stokeslet, ripple bake, Lamb–Oseen swirl): these are the numbers every new operator is compared against.
* **Proven red before trusted green:** run the pinch soak with the ingress mask disabled (the #33 fabrication, +9.5% over 12 000 passes) and watch (c) fail.

**DONE when:** one command soaks any operator and prints the four verdicts; the baseline table is in the evidence; the red run is in the evidence; `DECISIONS_5.md` exists with the opening entries; the fixture is bitwise (nothing in the core changed).

---

## Step 36 — Wave torsion, the proof brick (desktop machine)
**Spec:** MEDIUM §2.1.

* θ′ = θ + A·sin(k·r − φ)·e^{−γr}, r′ = r as a **third vortex profile** (`SUMI_VORTEX_TORSION`) — the vortex passes already rotate by a profile of r, so this is one enum value and one profile function; it is reachable today through every vortex route (mouse right-drag, the vortex ctl trio with the profile setting, `sumi_add_vortex`). The `[ITERATE: profile vs standalone]` resolves as: profile in the core, its OWN page in the operator book (the swirl's precedent).
* k and φ as additive ctl dims (per-note bend → k under the Anod table later).
* **The engine's first "episode":** the note-on outward phase sweep (φ = ωt with decay) as a per-voice, time-driven emitter of per-frame deltas within the pass budget — the pattern the burst's age envelope (38) and the spark's decay (39) reuse. Enabled by an additive params switch until the binding tables (42) own it.
* `--dev` key; web scene `torsion` (A, k, γ sliders, the sweep as `pace`); page draft in the evidence.

**DONE when:** class exact declared; the ±A inversion golden holds to noise (headless); the episode emits deltas, never absolutes; the scene runs; the fixture is bitwise; `sumi_version` bumped additively.

---

## Step 37 — Chladni lattice (desktop machine)
**Spec:** MEDIUM §2.2; SPEC §4.3 (the ripple's two insertion points).

* The quadrature kick-drift pair x₁ = x + (A/k_y)cos(k_y·y)cos(ωt); y₁ = y + (B/k_x)cos(k_x·x₁)sin(ωt) as a pass at BOTH insertion points: **live** (composite-side, breathing, nothing accumulates) and **bake** (delta-driven into the field). Inverse solved y first, then x — the ordering that makes det J = 1 exact. The "simultaneous" variant is written as a NEGATIVE test that must fail the inversion golden, so it can never be "simplified" into being.
* **Harmony as geometry:** k_x : k_y from the two lowest sounding voices, recomputed on voice begin/end and smoothed (the §2.2 proposal), with a fixed-ratio override for scenes and the desktop bench; A, B from poly pressure through additive ctl dims.

**DONE when:** exact declared; ±A inversion golden; the bake path passes the four-part gate (the #33 ingress rule applies to it as to the ripple bake); the live path leaves the dip un-shimmered (the ripple's own test, reused); scene `chladni` with 3:2, 4:3, 5:4 presets; fixture bitwise.

---

## Step 38 — Viscous multipole burst (desktop machine)
**Spec:** MEDIUM §2.3. **Author input:** the serendipity note's wording (the page draft is written in the author's voice and signed by the author).

* **Order fixed:** (1) `tools/multipole_verify.py`, the sibling of `stokeslet_verify.py`: χ(S), Φ_m(S), the divergence-free check of **d**, the elementary form for m ≥ 2 (and the E₁ kernel that excludes m = 1), the r → 0 hyperbolic-strain limit (the pinch) and the cos(mθ)/r far field — all numerically; the **literature check** recorded in the evidence (viscous multipole vortex decay, impulsive Stokes flows) BEFORE any wording that could imply priority. (2) The pass in the wake's family (`core/src/displacement.cpp`): ≤ a/4 sub-steps, the dual-time Gaussian core, D₂ normalised from the peak lobe displacement at r = a, χ via expm1 (the Lamb–Oseen small-r lesson), θ₀ from pen azimuth or glide direction, m from pitch class (quadrupole default; the table is a params-side lookup resolved by ear in 42). (3) The **age envelope**: ℓ² = a² + 4νt grows over the release, per-frame increments through the step-36 episode pattern.
* `sumi_add_burst(x, y, a, D, theta0, m)` for gestures; the strike route waits for the binding tables.

**DONE when:** class sub-stepped declared; the four-part gate holds under a strike stream at gesture rate (thousands of strikes, the wake's numbers as the bar); the verify script and the literature note are in the evidence; scene `burst` with the age slider; fixture bitwise; the page draft carries the lineage line and the author's note.

---

## Step 39 — Spark shear & the composed strike (desktop machine)
**Spec:** MEDIUM §2.4.

* The tri-wave kick-drift shear pass, frequency stacking (k, 2k, 4k) with the stack depth in a params field, decaying episodes (A, B ∝ e^{−t/τ}). The exactness test runs with a triangle AND a noise profile — the printed statement "shears invert for any profile" becomes a test.
* **The spark composition:** one exact drop pass (the Joule blast — radial outflow cannot be divergence-free, the oldest operator solves it) + rotated quadrupole burst sub-passes + the shear episode, as one `sumi_add_spark` gesture and as a strike-route candidate for 42. CC74 → k prepared as a slide-mode-style default.

**DONE when:** the spark SHEAR declares exact (±A inversion golden, triangle and noise profiles); `sumi_add_spark` gates as **sub-stepped by inheritance** (its burst component — the strictest-member rule) under the wake-family numbers; the four-part gate under episode streams; scene `spark` (and the composition visible in it); fixture bitwise; page draft.

---

## Step 40 — Chirikov standard map, the boss gate (desktop machine)
**Spec:** MEDIUM §2.5. **Author input:** the erosion budget vs. depth-into-chaos call, made with the numbers in hand.

* y′ = y + K·sin(k·x); x′ = x + ε·y′ (the ε-scaled drift is mandatory); K **delta-driven** from the mod-wheel/breath ctl through a new ctl dim.
* **The erosion soak:** sweep per-pass K under the four-part gate, find where (d) breaks, clamp the ceiling in the core, and put the `[ITERATE]` (budget vs. how deep into chaos the instrument may go) to the author as a table, not a paragraph.
* Scripted scene `chirikov` with K on a slider through Greene's threshold (≈ 0.9716): smooth sheets below, filamentation and island chains above.

**DONE when:** exact declared; the soak table is in the evidence and the ceiling is clamped and recorded in `DECISIONS_5`; the KAM transition is visible in the scene; fixture bitwise; page draft.

---

## Step 41 — The ABI event: libsumi 1.0.0 (desktop machine; the Mac compiles iOS the same session)
**Spec:** MEDIUM §1; INSTRUMENT §1; QOL §1. **The one break of the arc.**

* `sumi_params_t` gains `uint32_t medium` (`SUMI_MEDIUM_SUMI` = 0, `SUMI_MEDIUM_ANOD` = 1). `sumi_layout_probe` gains `const sumi_layout_state_t* state` (NULL or zeros = stateless; the struct exactly INSTRUMENT §1's, reserved words included). `sumi_cell_info_t` gains `uint32_t flags` (bit 0 = continuous, reserved for the theremin — the `[ITERATE: sentinel vs flags]` resolves as flags). `sumi_set_palette(inst, const sumi_palette_t*)` with the POD of QOL §1 and `SUMI_PALETTE_CUSTOM`. `sumi_layout_t` values 8–12 named and RESERVED (`sumi_set_params` clamps them until Phase 8). `sumi_version` → **1.0.0**; the header carries the migration note.
* Every call site updated mechanically: desktop, `hostmpe`, the iOS shell (compiled on the Mac in-step), the Android JNI (compiled by the Linux box as the first line of step 45), the web shim `sumi_web_probe`, the C11 ABI compile tests and their version pin.
* **Behaviour unchanged by construction:** medium 0 renders bitwise as 0.9.0 — the field gate proves the field, and a composite screenshot compare (new `--dev` tooling on the scripted clock) proves the pixels.

**DONE when:** `sumi_version()` reads 1.0.0; **every CI build job green, the Android compile job included** (the step-45 first line verifies ON DEVICE — main is never red between 41 and 45); all desktop suites green; the iOS project compiles; the web builds and `tools/web_gate.mjs` passes; the fixture and the composite screenshot are bitwise on Metal; `DECISIONS_5` records the break and why the probe state ships before any stateful layout.

---

## Step 42 — The Anod medium (desktop machine)
**Spec:** MEDIUM §1 (switching), §3 (composite), §4 (binding tables). **Author input:** an hour on the ROLI Piano + Airwave in Anod, then the binding table signed by eye (there is no sound until Phase 7 — the medium is judged visually); the burst's m-by-pitch-class table by eye, revisited by ear once Voxo lands (a 55-adjacent check).

* The composite branches per medium. **Strain-glow:** finite-difference the stored source coordinates → ‖J‖_F; charge phase → filament banding; aux → per-event hue; the **ingress mask** excludes fresh water (a scroll seam is a discontinuity, not strain); near-black substrate with screen-locked grain (SPEC §4.5 invariant, composite side).
* The three Anod palettes (electric blue/violet, plasma orange, phosphor green) continuing the palette ids.
* **The default binding tables** (MEDIUM §4) as medium-scoped defaults for `bend_mode`/`slide_mode`/`press_mode` and the strike route — the CC map and the mode params override exactly as today. The spark composition becomes Anod's strike; torsion's sweep its press feed (or not — the `[ITERATE]` is answered by playing).
* **Live switching prototyped and decided** (feature vs. forced dip); the same recorded session re-read in both media is the evidence either way.
* Prints styled per medium (an Anod dip is "the photograph"); the drop-edge question — every ring boundary and seam WILL glow in the first prototype — answered in `DECISIONS_5` by taste, not on paper.

**DONE when:** the Sumi composite is bitwise vs. step 41; a recorded session re-reads in Anod; the binding table is signed; every `[ITERATE]` in MEDIUM §1/§3/§4 is resolved in `DECISIONS_5` or explicitly carried to the Phase-9 beta with its question.

---

## Step 43 — Palettes, substrate, presets & prints — authored once (desktop machine)
**Spec:** QOL §1, §2, §3, §4.

* **Palette model:** 2–8 linear-RGB stops + the medium's ink-depth/glow curve (the identity guardrail; the `[ITERATE: curve fixed vs advanced fold]` resolves as fixed in 2.0, the fold deferred); the built-ins re-expressed as presets THROUGH THE SAME PATH (one code path, bitwise-checked); colour-blind-considerate presets; **the morph ring with a custom palette** (missing from QOL §1 — decide: the ring includes the custom slot, or morph is disabled while one is active); aux hue drift as a palette field or global (decide). The editor in the shared settings window with live preview on the running canvas.
* **Substrate:** paper tint / roughness / fiber presets (Sumi); darkness / grain (Anod). Composite side by construction.
* **Presets:** params + CC map + strip assignments + custom palette + layout-state defaults, JSON, **one serializer** — host-side, pure C, beside `hostmpe` — so four shells share one code path and cannot drift; export/import; the last session restores on launch; the schema rule (stamped with `sumi_version`; unknown fields ignored, missing fields defaulted).
* **Prints:** target-size export (the composite at target resolution from the same field — the UI copy states the bound honestly: detail below a field texel is interpolation; the true re-dip is replay, Phase 8), PNG plus TIFF-16 if demand-checked, Anod over alpha; **the print ledger** (session dips, thumbnails, re-export while the session lives).

**DONE when:** the built-in palettes are bitwise through the new path; a preset round-trips in ctest and the serializer has its own headless suite; the ledger re-exports a dip at 4k; the schema is documented for the shells (44–46).

---

## Step 44 — iOS shell (macOS machine, iOS agent)
**Spec:** MEDIUM §1; QOL §1–§4, §6 (copy).

* Medium switch, Anod palettes, the palette editor, substrate, presets (Files export/import), the print ledger, the clear-vs-dip copy; About shows `libsumi 1.0.0`.

**DONE when:** every new setting is reachable on the iPad; a preset made on the desktop imports and renders the same (screenshot compare); the byte path is untouched (the Phase-4 tests still pass).

## Step 45 — Android shell (Linux box, Android agent)
**Spec:** as 44. First line of the step: the step-41 JNI update compiles and the app runs.

**DONE when:** as 44 on the Galaxy Tab, plus the 16 KB page-size build still passes Play's check.

## Step 46 — Web marble (any machine)
**Spec:** MEDIUM §1; QOL §1, §3; SPEC §9.5.

* Medium switch + Anod palettes + the palette editor (lil-gui) + presets in `localStorage` with export; the five new scenes verified by `tools/web_gate.mjs --scenes`; the §4.6 web tier still passes; `site/scripts/check.mjs` learns the new scene names (the pages come in 63).

**DONE when:** the web gate and the scene sweep are green; the marble page plays Anod from a ROLI over Web MIDI in Chrome. **Phase end:** tag `v2.0.0-alpha.1`, the author installs everywhere; fold `DECISIONS_5`.

---

# Phase 7 — Sound (steps 47–55)
**The sibling library.** `voxo/` beside `core/`: `voxo/include/voxo.h` pure C (the `sumi_core.h` rules verbatim — three-state export macro, `voxo_version`, no STL or exceptions across it), `voxo/src/` C++20, compiling `core/src/midi_normalizer.cpp` and the voice-table logic FROM SOURCE (one MPE decoder, two builds, zero runtime coupling). `libsumi` is not touched in this phase: the fixture stays bitwise trivially. The shell's single producer fans the same bytes into a second SPSC; Voxo OFF = today's app, nothing regresses. Third-party: miniaudio, pugixml, miniz, dr_wav/dr_flac (permissive; listed on the citations/licences page in 63). Web is deferred (SOUND §4). **Order:** skeleton and desktop backend (47) → the mobile latency spike (48, the go/no-go on miniaudio for Android) → dispatch and pitch (49) → the format (50) → the voice's interior (51) → the bus and the gate (52) → iOS (53) → Android (54) → desktop shells and the combined stress (55).

## Step 47 — Voxo skeleton & the desktop backend (macOS machine)
**Spec:** SOUND §1.

* The library, its C ABI, the second SPSC fed by the shell's producer, **the callback-thread contract** (lock-free rings only; zero allocations, locks and logging in the callback; voice state transitions through the event queue drained at block start), miniaudio on CoreAudio with a sine per voice, block-size defaults (`[ITERATE: per platform]` → a table in `DECISIONS_6`), `voxo_version`. `build.yml` gains the Voxo headless suite (C11 ABI compile, ring tests); Windows and Linux compile here and are exercised in 55.

**DONE when:** fifteen sines follow the ROLI over MPE on the Mac, glitch-free at 128 frames; a test asserts zero allocations inside the callback (counting allocator); the contract is written at the top of `voxo.h`.

## Step 48 — The mobile latency spike (Linux box for Android; macOS machine for iOS) — TIMEBOXED
**Spec:** SOUND §1 `[ITERATE: confirm after an Android latency spike]`, §4.

* One session per platform. Android: miniaudio's AAudio path with `PerformanceMode::LowLatency`, touch-to-sound measured on the Galaxy Tab (microphone against the screen tap, the Phase-4 latency method); iOS: AVAudioSession preferred IO buffer through miniaudio, the same measurement on the iPad. Verdict per platform: miniaudio confirmed, or the escape hatch named — direct AAudio, AVAudioEngine; never RtAudio on mobile.

**DONE when:** the numbers are in the evidence and the verdicts in `DECISIONS_6`; the AAudio low-latency bar is either met or the escape hatch is scheduled into 54.

## Step 49 — Voice dispatch & pitch — "it glides" (macOS machine)
**Spec:** SOUND §2.

* Normalizer-fed 1:1 MPE dispatch (one member channel = one performance voice), per-block ratio 2^((note − root + bend·range)/12) with per-sample ramping, **4-point Hermite interpolation**, ±48-semitone glides from a single sample; sustain (CC 64) in the release logic; local control (CC 122); strip volume → master gain; internal sound and outbound MIDI as independent switches.

**DONE when:** a ±48 glide bounced to WAV shows no aliasing in an offline spectral check (`tools/`, Hermite vs. linear side by side — the reason recorded); the ROLI glides a piano sample across four octaves on the Mac.

## Step 50 — Decent Sampler subset & the compat report (macOS machine, headless)
**Spec:** SOUND §3. **Author input:** one public-domain / CC0 library (Pianobook-class) as the real-world fixture.

* pugixml over `.dspreset`: zones (lo/hi note and velocity, rootNote, tuning), groups, ADSR, loops, round-robin/sequence, the per-voice LP filter, DS MPE bindings, bus effect parameters; `.dslibrary` through miniz; WAV/FLAC through dr_wav/dr_flac (the AIFF `[ITERATE]` answered by the fixture library); **the compat report** per preset (chorus, convolution, the modulation matrix, UI, streaming: "will play without it"), calm and once.
* Fixtures: hand-written minimal presets for every feature, the real library, and malformed inputs.

**DONE when:** every fixture loads with the expected report; an hour of fuzzing the parser never crashes; the report's copy is the docs' copy.

## Step 51 — Layers, round robins, release samples, loops, filter, smoothing & bindings (macOS machine)
**Spec:** SOUND §2, §3 (the load-bearing paragraph).

* Velocity layers with crossfades, round robins, release samples, loops with crossfade (a 3-second sample sustains 30 seconds), the LP filter with CC74 → cutoff as DS's default, per-voice 1-pole smoothers from the preset's rising/falling times, the preset's `<bindings>` overriding, swirl (0xA0) offered as an extra source (`[ITERATE: default target]` → none by default).

**DONE when:** the fixture library plays with audible layer crossfades and round robins; a pad holds thirty seconds; the Osmose's slide moves the cutoff; the callback contract still holds under fifteen voices of stacked samples.

## Step 52 — Bus reverb & delay, preload gate (macOS machine)
**Spec:** SOUND §3.

* Freeverb-class reverb and a feedback delay, post-sum on the bus, real-time safe, reading the preset's DS effect parameters; **preload-first** with the advisory size gate (a warning, not a wall; desktop = free-memory check); the demo-instrument slot.

**DONE when:** zero XRuns at 128 frames with both effects on; the gate warns on an oversized library and loads it anyway.

## Step 53 — iOS (macOS machine, iOS agent)
**Spec:** SOUND §4. **Author input:** the demo instrument, recorded on brand (music box or kalimba, public domain, tiny).

* AVAudioSession low-latency playback (per 48), interruption handling (calls, Siri), **foreground only**: audio pauses with the visuals and resumes on return, no `UIBackgroundModes`; `os_proc_available_memory()` behind the advisory gate; library import from Files (`.dspreset`, `.dslibrary`); the demo instrument bundled so first launch makes a sound.

**DONE when:** on the iPad the ROLI plays the demo instrument; a call interrupts and the sound returns; a large library warns and loads; the Play surface plays it with the pen (the "controller without a sound" complaint dissolves in the author's hands).

## Step 54 — Android (Linux box, Android agent)
**Spec:** SOUND §4; the step-48 verdict.

* AAudio low latency (miniaudio or direct, per 48), audio focus, foreground only, the SAF picker for libraries, the gate, the demo instrument.

**DONE when:** as 53 on the Galaxy Tab; touch-to-sound within the step-48 bar; the Travel Sax plays it over USB.

## Step 55 — Desktop shells & the combined stress (macOS machine; Windows and Linux boxes verify)
**Spec:** SOUND §4, §5.

* Output-device selection and hotplug in the settings window; the **acceptance suite**: the Osmose storm plus a heavy preset holding 60 fps AND zero XRuns (`[ITERATE: XRun budget]` → a number in `DECISIONS_6`), scripted on the desktop harness; the licensing-posture page drafted (formats are not copyrightable; the app bundles no libraries; each library's terms travel with it).

**DONE when:** the suite is green on all three desktops (each box runs it in this step); WASAPI and ALSA device changes survive. **Phase end:** tag `v2.0.0-alpha.2`; fold `DECISIONS_6`.

---

# Phase 8 — Instruments (steps 56–62)
**INSTRUMENT §1's design has been in the ABI since step 41; this phase fills it.** Fingering as MIDI, provisionally: valves **CC 110 / 111 / 112** (≥ 64 = pressed) on the **master channel** (global state — a DAW records it where it records the mod wheel); the slide as **7-bit CC 113 with normalizer smoothing** — the 14-bit pair would put its MSB in CC 0–31, the Airwave's block (DECISIONS_4 #50), and 128 steps over six semitones is under five cents per step. The author confirms or overrides in `DECISIONS_7 #1`. **Order:** the stateful cores (56) and stateless layouts (57) headless on the desktop → the strip and surface machinery in `hostmpe` (58) → iOS (59) → Android (60) → replay (61, needs fingering to be complete and Voxo to re-sound) → web (62).

## Step 56 — Stateful layout cores (desktop machine, headless)
**Spec:** INSTRUMENT §1, §2, §3, §5.

* The normalizer decodes the fingering CCs into the engine's `sumi_layout_state_t` (one source of truth, the byte stream; the shells mirror the same bytes into their snapshot). **Trumpet:** eight partial cells × the eight valve states, idealised offsets (1 = −2, 2 = −1, 3 = −3, sums), the arrangement (column vs. arc) as a params flag decided by eye in 59; **trombone:** seven partials, `slider` 0..1 → 0..6 semitones CONTINUOUS (detents are UI ticks only). The visual-echo `[ITERATE]` resolves as: the strip shows fingering, the canvas stays ink.
* Goldens: every valve combination × every partial; the slide at detents and midpoints; a recorded fingering stream replays into identical probe answers.

**DONE when:** the goldens pass; the desktop draws both layouts as visualizer overlays; the probe stays pure (no instance, no state inside the core beyond the engine's own decoded copy); fixture bitwise.

## Step 57 — Stateless layouts (desktop machine, headless)
**Spec:** INSTRUMENT §4, §5.

* **Wicki–Hayden** hex (the cell math beside Jankó's, one echo — kept: it is not a string layout, it is the concertina button-field, and it is the cheapest item in the step); **`SUMI_LAYOUT_STRINGS`** — the fretboard generalised: string-rows × chromatic frets with a **fixed tuning-preset enum** (a params field of fixed arrays, NOT the deferred user-editable table): `STANDARD_GUITAR` (6 strings, EADGBE), `WHOLE_TONE_TAP` (whole-tone string spacing — the tapping-grid isomorphism, arguably the layout most native to glass; the docs may say "inspired by tapping instruments such as the Harpejji" — **the word never enters the enum or a product name: it is Marcodi's live trademark**), `ALL_FOURTHS` (Chapman-Stick/bass world). Per-string glide, echoes and the row-axis machinery are identical across presets; **theremin** (flags = continuous, no cells; the probe returns the pitch axis vector and the bipolar-Y convention). Layout names in every settings list (thirteen entries; the shells' `% 8` clamps become `% 13`; STRINGS presets are a sub-picker, not extra entries).

**DONE when:** goldens for the three layouts × the three string presets; the desktop overlays draw; `SUMI_LAYOUT_*` 8–12 are unreserved in the header; fixture bitwise.

## Step 58 — hostmpe: widgets, fingering on the wire, the small UX items (platform-neutral; ctest)
**Spec:** INSTRUMENT §2, §3, §5; SPEC §8; QOL §6.

* Valve buttons (the momentary widget, new CC ids); **the positional latch-slider** (a new variant: positional, not accumulating — the hand IS the slide; detent ticks; CC 113 on change under the transport budgets); the theremin surface (pitch from X through the legato re-anchor machinery, Y the bipolar press axis); the valve-change retune ramp (the piano-grid 20–40 ms machinery reused verbatim); fingering on BOTH pipes and in the re-announce; **panic** (all-notes-off + voice flush) as an action; layout quick-switch cycling a user-chosen subset; left-handed mirroring of surface and strip; per-device default presets — *offered*, not auto-applied (recommendation).
* `hostmpe_tests` goldens extended: a trumpet phrase's byte trace, a trombone glissando's, the theremin's continuous stream.

**DONE when:** the goldens pass on all three desktops' CI; the byte traces are the chart's new rows (63).

## Step 59 — iOS play surface (macOS machine, iOS agent)
**Spec:** INSTRUMENT §2–§5; QOL §6. **Author input:** the trumpet arrangement chosen by eye and recorded.

**DONE when:** a trumpet phrase with valve legato recorded into GarageBand replays with its fingering (the byte log shows CC 110–112 on the master channel); the trombone glissando is continuous and in tune with itself; panic, quick-switch, mirroring and the per-device offer work; Voxo re-sounds the trumpet.

## Step 60 — Android play surface (Linux box, Android agent)
**Spec:** as 59.

**DONE when:** as 59 on the Galaxy Tab; touch latency unchanged from Phase 4 (the probe stayed pure — measured, not assumed).

## Step 61 — Session replay (desktop machine authored; the iPad records)
**Spec:** QOL §5; SOUND §5.

* The file: timestamped bytes + params/state changes + **gesture calls** (recorded — the `[ITERATE]` resolves yes, so pen performances replay complete) **+ frame boundaries** — the per-frame drain points, as a frame index per event or tick markers. This field is what makes cross-device determinism POSSIBLE: the engine coalesces continuous dimensions per frame, and re-bucketing by wall time on a device with different frame cadence (120 Hz iPad → 60 Hz desktop) yields a different pass sequence and a field that diverges through no fault of the operators. **Playback drives the scripted clock through the recorded frame boundaries** — the evidence tooling's own pattern, productised. Version-stamped, source device named; recording on the tablets extends the Evidence byte log; replay through the loopback on ANY shell with the banner (source device, app version); re-dip at a new resolution or palette; **replay re-sounds** through Voxo.

**DONE when:** a session recorded on the iPad replays on the desktop within the §4.6 tier tolerance (measured — cross-device is a requirement, and achievable BECAUSE playback runs the recorded frame boundaries on the scripted clock, never wall-time re-bucketing); the same file re-sounds; a Metal-recorded session replays on the Linux box within its tier; a deliberate wall-time-re-bucketed replay is the negative test — it must diverge, proving the frame field is load-bearing.

## Step 62 — Web marble (any machine)
**Spec:** INSTRUMENT §4 (overlays only — Play stays web-deferred); QOL §5.

* Layouts 8–12 as visual overlays (the web probe shim carries the state); replay PLAYBACK in the browser for the gallery's "watch it again" links (bytes → `sumi_push_midi` on the scripted clock) if it fits the session — otherwise deferred with a note.

**DONE when:** the web gate is green; the overlays draw. **Phase end:** tag `v2.0.0-alpha.3`; fold `DECISIONS_7`.

---

# Phase 9 — Publish (steps 63–66)
**The Phase-5 machinery, run again.** Open `_work/DECISIONS_8.md` (and extend `build-notes.mjs`'s loop to it).

## Step 63 — Documentation (any machine)
**Spec:** SPEC §9.6 (the books); every Phase 6–8 spec's `[ITERATE]` list. **Author input:** the burst page's note (from 38), new gallery performances (an Anod piece, a trumpet piece).

* **The Anod operator book:** five pages with live scenes (torsion, Chladni, burst, spark, Chirikov), the class table in the book's index, the burst page carrying the serendipity note and the Jaffer-lineage line, literature-checked; **the medium guide** (switching, palettes and the editor, substrate, prints and the ledger); **the instrument pages** (trumpet, trombone, Wicki–Hayden, strings with its three tuning presets, theremin — fingering, the CC rows); **Voxo's guide** (loading a library, what the compat report means, foreground-only stated plainly, the licensing-posture page, "Voxo Dorean, later"); replay; presets; the settings reference rewritten; **the MIDI implementation chart** re-verified against fresh byte logs (fingering CCs, CC 122, the sampler's bindings); citations (Chirikov, Greene, Moser, Meiss; Aref & Ottino; the Decent Sampler format's author) and acknowledgments (Professor Jaffer first; **Ichisuke Fujioka honoured although the medium is Anod**). Mechanics: build against the RC (`PUBLIC_MARBLE_URL=/marble/rc/` in preview); merge with the stable tag (DECISIONS_4 #82).

**DONE when:** `check.mjs` is green with sixteen scenes in its list and every one embedded; `chart_check.py` is green against the new logs; every `[ITERATE]` of the four specs is either resolved in a decision entry or documented as a limit on the page that owns it.

## Step 64 — Store beta wave (both stores; human-heavy; needs 46, 55, 62, 63)
**Spec:** SPEC §9.4 (the Phase-5 beta procedure).

* `v2.0.0-rc.N` tags; TestFlight external and Play closed on the RC; the desktop RCs through the pre-release draft (cask PR unmerged, the `rc` apt suite, winget skipped — the Phase-5 discipline); the three questions plus Voxo's (latency felt, memory warnings seen, libraries that failed to load — compat reports collected); ≥ 2 weeks; docs-class items ship immediately (`pages.yml`).

**DONE when:** one full wave completed; every report triaged with a class; ≥ 5 external testers played Anod, ≥ 5 played with Voxo on (store metrics and the questions).

## Step 65 — Feedback incorporation & release candidates (iterative; any machine per item)
**Spec:** SPEC §9.4. The only step that loops. Core reopened for FIXES only (bug → regression test → fix); UX-feel items take the smallest change with the author's sign-off; waivers recorded in `DECISIONS_8`; the final RC nominated.

**DONE when:** zero open must-fix items; every fixed report reporter-confirmed or confirmed-unreachable; all suites green on the final RC on all platforms; the final RC has sat on both tracks ≥ 3 days.

## Step 66 — 2.0 release
**Spec:** SPEC §9.1, §9.4.

* The promotion of 65's final RC — zero code changes. Tag `v2.0.0`: the spine and lanes; publish the draft (cask, winget, `pages.yml`/apt fire); stores promoted; the changelog's 2.0 section is the notes everywhere; **the libsumi SDK question decided** (publish `sumi_core.h` + binaries as a versioned artifact against the settled 1.0.0 ABI, or defer again — the Part-4 prerequisites list); the note to Professor Jaffer about the multipole extension, in the author's voice, optional; Voxo Dorean stays deferred with its charter recorded. Then the fold: `DECISIONS_8` → `docs/DECISIONS.md`, this Part → `docs/ROADMAP.md`, the four specs → `docs/PROJECT_SPEC.md`, evidence out of the tree.

**DONE when:** all five platforms install 2.0.0 through their normal channels; the site documents 2.0 with every demo live at `/marble/`; both store listings are public; the fold is committed.

---

## Deferred (explicitly out of Phases 6–9 — do not implement)
Voxo Dorean (background execution, disk streaming, its own shell — SOUND §7); offline audio bounce of a replay; Play mode on the web and Voxo on the web; woodwind key systems, microtonal/scala tunings and the fretboard tuning table, two-handed split layouts (INSTRUMENT §6); the palette curve's "advanced fold"; field undo (deliberately absent — QOL §6); localization; auto-update; telemetry (never); Microsoft Store; Flatpak beyond the Phase-5 verdict.

---

## Provisional resolutions taken while drafting before they become `DECISIONS_5 #1…` when the phase opens
1. **The medium is named Anod** (author, 2026-09-21); Ichisuke Fujioka is honoured in the acknowledgments regardless (MEDIUM §1 `[ITERATE]` closed).
2. **Four phases, one public release, version 2.0.0**; pre-release `alpha` tags close Phases 6–8, `rc` tags run Phase 9.
3. **The ABI break (41) carries the probe's state argument and the cell-info flags two phases before any stateful layout uses them**, so the arc has exactly one break; `libsumi` is 1.0.0 from that step.
4. **Torsion is a vortex profile** in the core with its own page in the book; Chladni and the spark shear are passes of the ripple's shear family at both insertion points; the burst lives in the wake's sub-stepped family.
5. **Fingering CCs:** valves 110/111/112 on the master channel; the slide 7-bit CC 113 with smoothing (not a 14-bit pair — the MSB of a pair would land in CC 0–31, the Airwave's block; and 128 steps over six semitones ≈ 4.7 cents/step, under the ~5-cent JND — the arithmetic is the justification, keep it with the decision).
6. **Presets share one host-side serializer** (pure C, beside `hostmpe`); the core stays stateless about files.
7. **Per-device default presets are offered, not auto-applied.**
8. **Gestures are recorded in replay files**, so pen performances replay complete.
9. **New-feature documentation lands in step 63** and merges with the release tag; guide fixes ship from `main` at any time.
10. **The palette curve is fixed per medium in 2.0**; the theremin is a `flags` bit, not a radius sentinel.
11. **The fretboard generalises to `SUMI_LAYOUT_STRINGS`** with three FIXED tuning presets (standard guitar, whole-tone tap grid, all-fourths) — a params enum of fixed arrays; user-editable tunings stay deferred with microtonal. Wicki–Hayden stays (not a string layout; cheapest item in its step). **"Harpejji" is Marcodi's trademark:** docs may say "inspired by tapping instruments such as the Harpejji"; the word never enters an enum, a setting label, or a product name.
12. **Replay files carry frame boundaries** and playback drives the scripted clock through them (step 61) — wall-time re-bucketing is the documented anti-pattern and the negative test.

---

## The burst page's author note (step-38 input, lands in the docs at step 63)

**Lineage line (top of the burst's operator page):**

> Method after A. Jaffer, "The Lamb–Oseen Vortex and Paint Marbling" (arXiv:1810.04646), extended here to the multipoles m ≥ 2.

**The author's note (signed, first person — trim or roughen freely; the load-bearing sentences are the claim-nothing line and the last one):**

> A note on where this formula came from. I wasn't looking for a theorem — I was looking for a spark. The strikes in the electric medium felt too round, too liquid, nothing like the discharge I see when the music peaks. So I did what this whole project does: I followed Professor Jaffer's method. His Lamb–Oseen paper integrates a viscous flow over time to get a closed-form displacement; I tried the same integration on the higher multipoles, expecting special functions — and for m ≥ 2 the integral simply closed. The logarithmic kernel that haunts the dipole vanishes, and everything becomes elementary. I searched the literature as well as I could and did not find it stated in this form, but I claim nothing: serendipity did the work, and Jaffer drew the map. I only needed the discharge to bloom sharp and die soft, the way it sounds in my head. Here is its closed form.

(Preconditions from step 38 still apply: the literature check is recorded in the evidence BEFORE this note ships, and the note ships in the author's voice, signed.)

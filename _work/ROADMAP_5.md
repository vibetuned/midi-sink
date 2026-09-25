# The open roadmap — v2.0, Phases 7–9 (steps 47–66: Sound, Instruments, Publish)
**Phase 6 (steps 35–46) is DONE and folded into `docs/ROADMAP.md` Part 5 (2026-09-26). This file keeps the rest of the arc.**
**Companions: `docs/PROJECT_SPEC.md` (`SPEC §n`; the medium's section and the shipped quality-of-life items are drafted for it in `specs/TO_PROJECT_SPEC.md` until the author transcribes them), `specs/SOUND_SPEC.md` (`SOUND §n`), `specs/INSTRUMENT_SPEC.md` (`INSTRUMENT §n`), `specs/QUALITY_OF_LIFE_SPEC.md` (`QOL §n` — the undone items only); decision logs `_work/DECISIONS_6.md` … `_work/DECISIONS_8.md`, one per phase, referenced as `DECISIONS_6 #n` …; history `docs/CHANGELOG.md` (v2.0.0); the Phase-6 record `docs/DECISIONS.md` Part V (`DECISIONS_5 #n`).**
**Scope: three phases, one public release. Phase 7 Sound (47–55): Voxo, the sibling library. Phase 8 Instruments (55b–62): the field as displacement first, then the stateful probe in use, five layouts, the strip widgets, session replay. Phase 9 Publish (63–66): the documentation for everything, the beta wave, the feedback loop, the 2.0 release. Hard ordering: Voxo before replay so replay re-sounds (QOL §5) in a single pass; the displacement field (55b) before any layout work, because it moves every shader and fixture once; documentation for new features last, because the live `/marble/` is the newest stable tag (DECISIONS_4 #82) and the operator book's demos need the release wasm. Quality-of-life items ride the phase they belong to (QOL §7): panic on the strip, quick-switch, mirroring, per-device presets and replay with the instruments; the copy fixes wherever the shell steps are. Each phase ends with a pre-release tag on the test tracks; no external beta before Phase 9. One step per session, as always.**

---

## Working Rules (apply to every step)

* All prior working rules hold. The core is reopened for FEATURE work by this roadmap in Phase 8 only; Phase 7 never touches `libsumi` (the engine stays audio-free — SOUND §1); Phase 9 reopens it for fixes under bug → regression test → fix. Core changes prove out on the desktop harness FIRST, every time.
* **The phase invariant:** `tests/fixtures/field_512_metal.bin` stays BITWISE on Metal (a Metal invariant — DECISIONS_5 #87; GL and D3D11 hold their reference tier) through Phase 7 and again after step 55b re-captures it: 55b is the ONE step allowed to change the fixture, and it records the decision first. New operators add passes, media change the composite, layouts change the probe — none touches an existing pass.
* **Every operator declares its class** (MEDIUM §2 table) in its header comment, its test and its operator-book page: *exact* (det J = 1 at any magnitude; proven by a ±k inversion golden) or *sub-stepped displacement field* (soaked under the wake's ≤ a/4 rule and the four-part conservation gate of step 35). Membership is declared, never discovered in a failing soak.
* The delta rule (continuous controllers drive deltas per pass) and the one-consumer rule (`bend_mode`, `slide_mode`, `press_mode`) are unchanged; media add *defaults* for them, never a second consumer.
* **One platform per step.** Core and shared UI are authored on the desktop harness (the Mac); iOS on the Mac; Android on the Mac too since the Phase-6 close (the Tab is plugged into it and the Gradle/NDK toolchain is installed there — DECISIONS_6 #1); Linux on the Linux box; Windows on its box. A step never touches a second platform's build or store; the other shells consume in their own steps. **Sanctioned exception — verification fan-out:** a step may have OTHER boxes re-run an already-green suite unchanged (step 55's pattern); authoring stays single-platform.
* **Composed gestures inherit the strictest class of their members:** a composition containing a sub-stepped pass (the spark's burst component) gates under the sub-stepped family's numbers, even when its other members are exact.
* **The ABI event was ONE step (41, done):** `libsumi` is 1.1.0 and grows additively from here (new enum values, new `sumi_add_*`/ctl dims, appended params fields — the Step-33 minor-bump pattern). Step 55b is the one planned exception (the field's storage changes, not the C ABI). The prebuilt SDK stays deferred until Phase 9 asks the question.
* Evidence per step under `docs/evidence/<step>/`; at each phase end the fold: that phase's `_work/DECISIONS_<n>.md` merges into `docs/DECISIONS.md` as the next Part, evidence condenses into `CHANGELOG.md` and leaves the tree (git keeps it), scripts worth keeping move to `tools/`. `site/scripts/build-notes.mjs` renders `_work/DECISIONS_{5,6,7}.md` while in flight — Phase 9 extends the loop to 8.
* **Documentation timing:** guide fixes ship to `main` at any time (`pages.yml`). Pages for NEW operators, layouts and Voxo are drafted in the step's evidence folder (the burst page in the author's voice) and move into `site/` in step 63 — the live demos would otherwise point at scenes the released wasm does not know.
* **Pre-release tags** end Phases 6, 7 and 8 (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a pre-release, and the lanes stay proven); the author installs the build on every device and plays it. Phase 9 uses `v2.0.0-rc.N`. Nothing reaches a stable channel before step 66.
* Credentials: Phases 6–8 need none beyond the machines; Phase 9 reuses the Phase-5 set (Developer ID, ASC, Play, tap token, winget token, apt key). Author inputs (recordings, taste sign-offs, the demo instrument) are listed per step so they can be staged before the session.

---

# Phase 7 — Sound (steps 47–55)
**The sibling library.** `voxo/` beside `core/`: `voxo/include/voxo.h` pure C (the `sumi_core.h` rules verbatim — three-state export macro, `voxo_version`, no STL or exceptions across it), `voxo/src/` C++20, compiling `core/src/midi_normalizer.cpp` and the voice-table logic FROM SOURCE (one MPE decoder, two builds, zero runtime coupling). `libsumi` is not touched in this phase: the fixture stays bitwise trivially. The shell's single producer fans the same bytes into a second SPSC; Voxo OFF = today's app, nothing regresses. Third-party: miniaudio, pugixml, miniz, dr_wav/dr_flac (permissive; listed on the citations/licences page in 63). Web is deferred (SOUND §4). **Order:** skeleton and desktop backend (47) → the mobile latency spike (48, the go/no-go on miniaudio for Android) → dispatch and pitch (49) → the format (50) → the voice's interior (51) → the bus and the gate (52) → iOS (53) → Android (54) → desktop shells and the combined stress (55).

## Step 47 — Voxo skeleton & the desktop backend (macOS machine)
**Spec:** SOUND §1.

* The library, its C ABI, the second SPSC fed by the shell's producer, **the callback-thread contract** (lock-free rings only; zero allocations, locks and logging in the callback; voice state transitions through the event queue drained at block start), miniaudio on CoreAudio with a sine per voice, block-size defaults (`[ITERATE: per platform]` → a table in `DECISIONS_6`), `voxo_version`. `build.yml` gains the Voxo headless suite (C11 ABI compile, ring tests); Windows and Linux compile here and are exercised in 55.

**DONE when:** fifteen sines follow the ROLI over MPE on the Mac, glitch-free at 128 frames; a test asserts zero allocations inside the callback (counting allocator); the contract is written at the top of `voxo.h`.

## Step 48 — The mobile latency spike (macOS machine: Android on the Tab, then iOS on the iPad) — TIMEBOXED
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

## Step 54 — Android (macOS machine, Android agent)
**Spec:** SOUND §4; the step-48 verdict.

* AAudio low latency (miniaudio or direct, per 48), audio focus, foreground only, the SAF picker for libraries, the gate, the demo instrument.

**DONE when:** as 53 on the Galaxy Tab; touch-to-sound within the step-48 bar; the Travel Sax plays it over USB.

## Step 55 — Desktop shells & the combined stress (macOS machine; Windows and Linux boxes verify)
**Spec:** SOUND §4, §5.

* Output-device selection and hotplug in the settings window; the **acceptance suite**: the Osmose storm plus a heavy preset holding 60 fps AND zero XRuns (`[ITERATE: XRun budget]` → a number in `DECISIONS_6`), scripted on the desktop harness; the licensing-posture page drafted (formats are not copyrightable; the app bundles no libraries; each library's terms travel with it).

**DONE when:** the suite is green on all three desktops (each box runs it in this step); WASAPI and ALSA device changes survive. **Phase end:** tag `v2.0.0-alpha.2`; fold `DECISIONS_6`.

---

# Phase 8 — Instruments (steps 55b–62)
**The displacement field first (55b), then INSTRUMENT §1's design, in the ABI since step 41, is filled.** Fingering as MIDI, provisionally: valves **CC 110 / 111 / 112** (≥ 64 = pressed) on the **master channel** (global state — a DAW records it where it records the mod wheel); the slide as **7-bit CC 113 with normalizer smoothing** — the 14-bit pair would put its MSB in CC 0–31, the Airwave's block (DECISIONS_4 #50), and 128 steps over six semitones is under five cents per step. The author confirms or overrides in `DECISIONS_7 #1`. **Order:** the stateful cores (56) and stateless layouts (57) headless on the desktop → the strip and surface machinery in `hostmpe` (58) → iOS (59) → Android (60) → replay (61, needs fingering to be complete and Voxo to re-sound) → web (62).

## Step 55b — The field as displacement (desktop machine) — before the instruments
**Spec:** SPEC §4.1–§4.2 (the field's payload); DECISIONS_5 #61, #69, #80, #88 (every half-float quantum problem of Phase 6). **Author input:** the go, with the fixture's re-capture understood.

* The field stores each texel's DISPLACEMENT (u − x, v − y) instead of its absolute source coordinate. Near zero the RGBA16F ulp is ~60× finer than near 0.5, so the quantum that set the emission floors (#61, #69), the spark's step (#80/#81) and the Adreno's drift (#88) shrinks by that factor on every GPU — the same shaders, the same passes, one convention. Every deformation pass reads `x + d(x)` and writes `d′`; the composite, the export, the seam class test (#52: "identity" becomes `d = 0` exactly, no ULP tolerance) and the field dump follow; the ingress rule writes 0.
* **The fixture moves once**, by construction: `field_512_metal.bin` re-captured with the new payload and the phase invariant restarted from it; the composite fixture stays (the print does not change); the field gate's cross-backend tiers re-measured. The emission floors are re-derived from the new quantum (and may go), with the tests that measured them (#61) re-run.
* The web tier, GL, D3D11 and the Adreno re-gated; the Tab's six-strike compare (#88) repeated — the number this step is judged by.

**DONE when:** the new fixture is bitwise on Metal and the tiers hold on the others; every soak and harness test green; the Tab keeps six charges under #88's script with the strike's charge back at the thin proportions if the author wants them; DECISIONS_7 records the re-capture.

---

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

## Step 60 — Android play surface (macOS machine, Android agent)
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

## The burst page's author note (step-38 input, lands in the docs at step 63)

**Lineage line (top of the burst's operator page):**

> Method after A. Jaffer, "The Lamb–Oseen Vortex and Paint Marbling" (arXiv:1810.04646), extended here to the multipoles m ≥ 2.

**The author's note (signed, first person — trim or roughen freely; the load-bearing sentences are the claim-nothing line and the last one):**

> A note on where this formula came from. I wasn't looking for a theorem — I was looking for a spark. The strikes in the electric medium felt too round, too liquid, nothing like the discharge I see when the music peaks. So I did what this whole project does: I followed Professor Jaffer's method. His Lamb–Oseen paper integrates a viscous flow over time to get a closed-form displacement; I tried the same integration on the higher multipoles, expecting special functions — and for m ≥ 2 the integral simply closed. The logarithmic kernel that haunts the dipole vanishes, and everything becomes elementary. I searched the literature as well as I could and did not find it stated in this form, but I claim nothing: serendipity did the work, and Jaffer drew the map. I only needed the discharge to bloom sharp and die soft, the way it sounds in my head. Here is its closed form.

(Preconditions from step 38 still apply: the literature check is recorded in the evidence BEFORE this note ships, and the note ships in the author's voice, signed.)

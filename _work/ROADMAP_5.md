# The open roadmap — v2.0, Phases 8–10 (steps 55b–70: Suzu, Instruments, Publish)
**Phases 6 (steps 35–46) and 7 (steps 47–55) are DONE and folded into `docs/ROADMAP.md` Parts 5 and 6 (2026-09-26 and 2026-09-28). This file keeps the rest of the arc.**
**Companions: `docs/PROJECT_SPEC.md` (`SPEC §n`; the medium's section, the shipped quality-of-life items and the sound's section are drafted for it in `specs/TO_PROJECT_SPEC.md` until the author transcribes them), `specs/INSTRUMENT_SPEC.md` (`INSTRUMENT §n`), `specs/SYNTH_SPEC.md` (`SYNTH §n`), `specs/QUALITY_OF_LIFE_SPEC.md` (`QOL §n` — the undone items only); decision logs `_work/DECISIONS_7.md` (Suzu), `_work/DECISIONS_8.md` (Instruments) and `_work/DECISIONS_9.md` (Publish), one per phase, referenced as `DECISIONS_7 #n` …; history `docs/CHANGELOG.md` (v2.0.0); the Phase-6 and Phase-7 records `docs/DECISIONS.md` Parts V and VI (`DECISIONS_5 #n`, `DECISIONS_6 #n`).**
**Scope: three phases, one public release. Phase 8 Suzu (55b, 56–59 with 58b–58c): the field as displacement first, then the symplectic synth inside Voxo — cells, the modal voice and the bow, strings and chaos, then the winds (the bore as the chain in acoustic variables: the flute's jet, the sax's reed, the trumpet's lips — BEFORE the instruments phase, so the valve CCs of Phase 9 drive a real bore, not a metaphor), the orbit trace. Phase 9 Instruments (60–66): the stateful probe in use, five layouts, the strip widgets, session replay — AFTER the synth, deliberately: the trumpet and trombone are wind-family layouts and they are proven against a breath bow that holds, not a sampler piano; the pluck-position profile the layouts feed is the modal voice's own closed form. Phase 10 Publish (67–70): the documentation for everything, the beta wave, the feedback loop, the 2.0 release. Hard ordering: the displacement field (55b) before any layout or synth-trace work, because it moves every shader and fixture once; documentation for new features last, because the live `/marble/` is the newest stable tag (DECISIONS_4 #82) and the operator book's demos need the release wasm; replay re-sounds through Voxo (QOL §1), which shipped in Phase 7. Quality-of-life items ride the phase they belong to (QOL §2): panic on the strip, quick-switch, mirroring, per-device presets and replay with the instruments; the copy fixes wherever the shell steps are. Each phase ends with a pre-release tag on the test tracks; no external beta before Phase 10. One step per session, as always.**
---

## Working Rules (apply to every step)

* All prior working rules hold. The core is reopened for FEATURE work in 55b and the Instruments phase only — **the Suzu phase never touches `libsumi`** (the synth lives in Voxo; the orbit trace reaches the canvas through the EXISTING gesture ABI, host-bridged); Publish reopens the core for fixes under bug → regression test → fix. Core changes prove out on the desktop harness FIRST, every time.
* **The phase invariant:** `tests/fixtures/field_512_metal.bin` stays BITWISE on Metal (a Metal invariant — DECISIONS_5 #87; GL and D3D11 hold their reference tier) and again after step 55b re-captures it: 55b is the ONE step allowed to change the fixture, and it records the decision first. New operators add passes, media change the composite, layouts change the probe — none touches an existing pass.
* **Every operator declares its class** (MEDIUM §2 table; SYNTH §1's DSP table is its audio twin — every Suzu element declares symplectic / conformal-dissipative / drive / self-excited / lossless-transport the same way) in its header comment, its test and its operator-book page: *exact* (det J = 1 at any magnitude; proven by a ±k inversion golden) or *sub-stepped displacement field* (soaked under the wake's ≤ a/4 rule and the four-part conservation gate of step 35). Membership is declared, never discovered in a failing soak.
* The delta rule (continuous controllers drive deltas per pass) and the one-consumer rule (`bend_mode`, `slide_mode`, `press_mode`) are unchanged; media add *defaults* for them, never a second consumer.
* **One platform per step.** Core and shared UI are authored on the desktop harness (the Mac); iOS on the Mac; Android on the Mac too since the Phase-6 close (the Tab is plugged into it and the Gradle/NDK toolchain is installed there — DECISIONS_6 #1); Linux on the Linux box; Windows on its box. A step never touches a second platform's build or store; the other shells consume in their own steps. **Sanctioned exception — verification fan-out:** a step may have OTHER boxes re-run an already-green suite unchanged (step 55's pattern); authoring stays single-platform.
* **Composed gestures inherit the strictest class of their members:** a composition containing a sub-stepped pass (the spark's burst component) gates under the sub-stepped family's numbers, even when its other members are exact.
* **The ABI event was ONE step (41, done):** `libsumi` is 1.1.0 and grows additively from here (new enum values, new `sumi_add_*`/ctl dims, appended params fields — the Step-33 minor-bump pattern). Step 55b is the one planned exception (the field's storage changes, not the C ABI). The prebuilt SDK stays deferred until Phase 10 asks the question.
* Evidence per step under `docs/evidence/<step>/`; at each phase end the fold: that phase's `_work/DECISIONS_<n>.md` merges into `docs/DECISIONS.md` as the next Part, evidence condenses into `CHANGELOG.md` and leaves the tree (git keeps it), scripts worth keeping move to `tools/`. `site/scripts/build-notes.mjs` renders `_work/DECISIONS_{5,6,7,8}.md` while in flight — Publish extends the loop to 9 (Parts V and VI are in `docs/DECISIONS.md` now).
* **Documentation timing:** guide fixes ship to `main` at any time (`pages.yml`). Pages for NEW operators, layouts and Voxo are drafted in the step's evidence folder (the burst page in the author's voice) and move into `site/` in step 63 — the live demos would otherwise point at scenes the released wasm does not know.
* **Pre-release tags** end Phases 6, 7, 8 and 9 (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a pre-release, and the lanes stay proven); the author installs the build on every device and plays it. Phase 9 uses `v2.0.0-rc.N`. Nothing reaches a stable channel before step 66.
* Credentials: Phases 6–9 need none beyond the machines; Phase 10 reuses the Phase-5 set (Developer ID, ASC, Play, tap token, winget token, apt key). Author inputs (recordings, taste sign-offs, the demo instrument) are listed per step so they can be staged before the session.

---

# Phase 8 — Suzu, the synth (steps 55b, 56–58, 58b–58c, 59)
**The displacement field first (55b), then SYNTH_SPEC filled — inside Voxo, `libsumi` untouched.** The synth precedes the instruments so the wind-family layouts are proven against a voice that breathes. All Suzu work is desktop-machine (Voxo's home); the tablets consume the source picker in their Phase-9 shell steps. Open `_work/DECISIONS_7.md`; the working name **Suzu** is confirmed or overridden in `DECISIONS_7 #1`.

## Step 55b — The field as displacement (desktop machine) — before the synth's trace and the instruments alike
**Spec:** SPEC §4.1–§4.2 (the field's payload); DECISIONS_5 #61, #69, #80, #88 (every half-float quantum problem of Phase 6). **Author input:** the go, with the fixture's re-capture understood.

* The field stores each texel's DISPLACEMENT (u − x, v − y) instead of its absolute source coordinate. Near zero the RGBA16F ulp is ~60× finer than near 0.5, so the quantum that set the emission floors (#61, #69), the spark's step (#80/#81) and the Adreno's drift (#88) shrinks by that factor on every GPU — the same shaders, the same passes, one convention. Every deformation pass reads `x + d(x)` and writes `d′`; the composite, the export, the seam class test (#52: "identity" becomes `d = 0` exactly, no ULP tolerance) and the field dump follow; the ingress rule writes 0.
* **The fixture moves once**, by construction: `field_512_metal.bin` re-captured with the new payload and the phase invariant restarted from it; the composite fixture stays (the print does not change); the field gate's cross-backend tiers re-measured. The emission floors are re-derived from the new quantum (and may go), with the tests that measured them (#61) re-run.
* The web tier, GL, D3D11 and the Adreno re-gated; the Tab's six-strike compare (#88) repeated — the number this step is judged by.

**DONE when:** the new fixture is bitwise on Metal and the tiers hold on the others; every soak and harness test green; the Tab keeps six charges under #88's script with the strike's charge back at the thin proportions if the author wants them; DECISIONS_7 records the re-capture.

## Step 56 — Suzu cells (desktop machine; Voxo only)
**Spec:** SYNTH §1 (the DSP class table), §2.1–§2.4, §4. **Author input:** the name (`DECISIONS_7 #1`).

* The magic circle with exact tuning ε = 2·sin(πf/fs) in the **Gordon–Smith form** (the plain form's glide ripple is the printed justification, not the shipped sound); the Chamberlin SVF in the 2×-oversampled section with exact f = 2·sin(πfc/fs) (the trapezoidal escape hatch decided by the tuning test, not ideology); phase-space shears (waveshaping) inside the oversampled section; **FTZ/DAZ at audio-thread init**; the class table in headers and tests. A source type beside the sampler; the desktop Sound section gains the source picker row (shared UI authored here; the tablets consume in 63/64).

**DONE when:** the drift test holds (undriven undamped cell, 10 min: < 0.1 dB, < 0.5 cent — the naive simultaneous update proven red beside it); a scripted ±48-semitone glide ripples < 0.5 dB with the plain form's ripple archived; oscillator and SVF within 2 cents across MIDI 21–108 at 44.1 k and 48 k; the 64-voice decay-tail stress shows no subnormal cliff; callback headroom measured and recorded.

## Step 57 — The modal voice & the breath bow (desktop machine)
**Spec:** SYNTH §2.5–§2.6, §3. **Author input:** the binding table signed **by ear** — for the first time in this project there is sound to sign; patch names.

* The modal lattice with the **shared-potential coupling algorithm** (SYNTH §2.5: pre-update kick → cell updates → conformal damping; the spectral-radius load gate with its red control; mode splitting charted as a feature); ratio presets (harmonic string, stiff bar, bell, glass, and the **plucked-string preset** — γ_k = α + βk², √(1+Bk²), pluck position → sin(kπp)/k² kick weights); the **breath bow** (energy-servo van der Pol; breath → E_target through the INK_FLOW dimension; μ = onset character); swirl (0xA0) → coupling strength; the full MPE map through the normalizer; patches ride the QoL preset files.

**DONE when:** the energy ledger holds (zero-decay bell sustains ≥ 10 min within the drift bound; restored T60s within 5% of declared); the bow's limit cycle converges from silence and from 2× alike, zero breath decays to silence, the clamped-extraction control proven red, steady-state injection/extraction balances within 1%; **the Brisa holds a singing tone** (the author, by ear, on the actual hardware); the binding table signed in `DECISIONS_7`.

## Step 58 — Strings & chaos (desktop machine)
**Spec:** SYNTH §2.3, §2.8–§2.10, §5.

* The **Verlet chain** (CFL gate k_spring·dt² ≤ 1 enforced at patch-load; the 5%-over control blows up in the harness, archived; the tuning sweep re-derives M/k_spring, 2 cents where representable); the **hybrid string** (lossless-transport delay + bridge cells via the §2.9 junction algorithm — read, kick, update, reflect `−s + c_back·v_bridge`; passivity gated TWICE: the load-time zero-damping probe no patch can dodge, and the long soak with the 1.05×-coupling control proven red); the **Duffing cell** (rotation + cubic shear; the clang-and-settle pitch chart archived); the **kicked rotor** (K from the mod wheel, delta-smoothed; the K-sweep spectrogram archived beside the visual Chirikov's — one theorem, two senses); control-rate chaotic modulators (leapfrog double pendulum, bounded, routed to smoothed params).

**DONE when:** every gate above green with its red control archived; the combined stress — the Osmose storm + a heavy sampler preset + 10 synth voices — holds 60 fps and 0 XRuns on the Mac, with Windows and Linux re-running the suite unchanged (the sanctioned fan-out).

## Step 58b — The bore & the jet: the flute (desktop machine)
**Spec:** SYNTH §2.11, §2.13, §5. The flute comes FIRST among the winds: the jet exciter has no moving mass (delay + gain + tanh, all owned parts), so it validates the bore with the least new mechanism — and its acceptance is the spectacle.

* The staggered (p, u) bore — the §2.8 chain in acoustic variables, S(x) weights, CFL gate + red control, per-voice pitch by bore length, radiation-loss boundary as a declared conformal port; the jet: Hermite fractional delay τ = d/(αU₀), e^{μd} gain, tanh labium partition as the boundary flow port; stochastic labium vector for chiff; breath → P_mouth through INK_FLOW.

**DONE when:** the three-geometry series test passes (odd / all / all, within cents); the closed lossless bore holds the drift bound; **the overblow bifurcation is on a spectrogram** — a breath ramp jumps the octave with no fingering change — and soft blowing measurably flattens (the τ lag, free); the mouth-power ledger holds within 1% on a scripted phrase; the Brisa plays a flute that breathes (the author, by ear).

## Step 58c — The reed & the lips: sax and trumpet (desktop machine)
**Spec:** SYNTH §2.12, §2.11 (cone and Bessel profiles), §5. **Author input:** embouchure binding (`DECISIONS_7`), by ear.

* The 1-DOF valve (inward reed / outward lips) + Bernoulli aperture flow; **the junction treated like the bridge's** — load-time probe with an archived unstable-variant red, plus the mouth-power ledger; cone bore (sax: the full series from a closed cone, gated) and cylinder+Bessel-flare bore (trumpet); **v1 brassiness = a declared §2.4 shear on the outgoing wave**, amplitude-driven — shock propagation stays deferred with its (γ+1)/2 note.

**DONE when:** the ledger and probe gates green with reds archived; the sax bore's impedance peaks at all integers (the conical result, measured); lip tension bends pitch within a partial and jumps registers on the trumpet bore (by ear + byte log); the combined stress re-passes with two wind voices added; the phase's spec folds queued in `DECISIONS_7`.

## Step 59 — The orbit trace & the phase close (desktop machine)
**Spec:** SYNTH §2.7. **Author input:** trace-scale taste sign-off; whether the rotor's trace defaults ON in Anod.

* **Scope route first:** the shell overlay drawing voice orbits screen-space, dip-excluded like the live ripple. **Gesture route second:** curvature-weighted per-frame polylines emitted as tine/wake segments through the EXISTING gesture ABI (the shell bridges Voxo → libsumi at frame rate — no core change; class by inheritance under the strictest-member rule; budget-counted like any feed); per-voice toggle in the bindings.

**DONE when:** the trace budget test passes (10 voices, segments merged per the echo rules, no feed starved; trace OFF bit-identical to no-trace); the rotor scribbling its chaos into Anod is on video in the evidence — the synth drawing its own phase portrait in ink. **Phase end:** tag `v2.0.0-alpha.3`; the author plays it on every device (the synth on the desktops; the tablets regression-checked); fold `DECISIONS_7`.

---

# Phase 9 — Instruments (steps 60–66)
**INSTRUMENT §1's design, in the ABI since step 41, is filled — with a bow to prove the wind layouts against.** Fingering as MIDI, provisionally: valves **CC 110 / 111 / 112** (≥ 64 = pressed) on the **master channel** (global state — a DAW records it where it records the mod wheel); the slide as **7-bit CC 113 with normalizer smoothing** — the 14-bit pair would put its MSB in CC 0–31, the Airwave's block (DECISIONS_4 #50), and 128 steps over six semitones is under five cents per step. The author confirms or overrides in `DECISIONS_8 #1`. Open `_work/DECISIONS_8.md`. **Order:** the stateful cores (60) and stateless layouts (61) headless on the desktop → the strip and surface machinery in `hostmpe` (62) → iOS (63) → Android (64) → replay (65, needs fingering to be complete and Voxo to re-sound) → web (66).

## Step 60 — Stateful layout cores (desktop machine, headless)
**Spec:** INSTRUMENT §1, §2, §3, §5.

* The normalizer decodes the fingering CCs into the engine's `sumi_layout_state_t` (one source of truth, the byte stream; the shells mirror the same bytes into their snapshot). **Trumpet:** eight partial cells × the eight valve states, idealised offsets (1 = −2, 2 = −1, 3 = −3, sums), the arrangement (column vs. arc) as a params flag decided by eye in 59; **trombone:** seven partials, `slider` 0..1 → 0..6 semitones CONTINUOUS (detents are UI ticks only). The visual-echo `[ITERATE]` resolves as: the strip shows fingering, the canvas stays ink.
* Goldens: every valve combination × every partial; the slide at detents and midpoints; a recorded fingering stream replays into identical probe answers.

**DONE when:** the goldens pass; the desktop draws both layouts as visualizer overlays; the probe stays pure (no instance, no state inside the core beyond the engine's own decoded copy); fixture bitwise.

## Step 61 — Stateless layouts (desktop machine, headless)
**Spec:** INSTRUMENT §4, §5.

* **Wicki–Hayden** hex (the cell math beside Jankó's, one echo — kept: it is not a string layout, it is the concertina button-field, and it is the cheapest item in the step); **`SUMI_LAYOUT_STRINGS`** — the fretboard generalised: string-rows × chromatic frets with a **fixed tuning-preset enum** (a params field of fixed arrays, NOT the deferred user-editable table): `STANDARD_GUITAR` (6 strings, EADGBE), `WHOLE_TONE_TAP` (whole-tone string spacing — the tapping-grid isomorphism, arguably the layout most native to glass; the docs may say "inspired by tapping instruments such as the Harpejji" — **the word never enters the enum or a product name: it is Marcodi's live trademark**), `ALL_FOURTHS` (Chapman-Stick/bass world). Per-string glide, echoes and the row-axis machinery are identical across presets; **theremin** (flags = continuous, no cells; the probe returns the pitch axis vector and the bipolar-Y convention). Layout names in every settings list (thirteen entries; the shells' `% 8` clamps become `% 13`; STRINGS presets are a sub-picker, not extra entries).

**DONE when:** goldens for the three layouts × the three string presets; the desktop overlays draw; `SUMI_LAYOUT_*` 8–12 are unreserved in the header; fixture bitwise.

## Step 62 — hostmpe: widgets, fingering on the wire, the small UX items (platform-neutral; ctest)
**Spec:** INSTRUMENT §2, §3, §5; SPEC §8; QOL §6.

* Valve buttons (the momentary widget, new CC ids); **the positional latch-slider** (a new variant: positional, not accumulating — the hand IS the slide; detent ticks; CC 113 on change under the transport budgets); the theremin surface (pitch from X through the legato re-anchor machinery, Y the bipolar press axis); the valve-change retune ramp (the piano-grid 20–40 ms machinery reused verbatim); fingering on BOTH pipes and in the re-announce; **panic** (all-notes-off + voice flush) as an action; layout quick-switch cycling a user-chosen subset; left-handed mirroring of surface and strip; per-device default presets — *offered*, not auto-applied (recommendation).
* `hostmpe_tests` goldens extended: a trumpet phrase's byte trace, a trombone glissando's, the theremin's continuous stream.

**DONE when:** the goldens pass on all three desktops' CI; the byte traces are the chart's new rows (67).

## Step 63 — iOS play surface (macOS machine, iOS agent)
**Spec:** INSTRUMENT §2–§5; QOL §6. **Author input:** the trumpet arrangement chosen by eye and recorded.

**DONE when:** a trumpet phrase with valve legato recorded into GarageBand replays with its fingering (the byte log shows CC 110–112 on the master channel); the trombone glissando is continuous and in tune with itself; panic, quick-switch, mirroring and the per-device offer work; **the Sound section's source picker carries Suzu and a bowed patch sings under the trumpet's valves** (consume-and-verify of step 56's shared UI); Voxo re-sounds the trumpet.

## Step 64 — Android play surface (macOS machine, Android agent)
**Spec:** as 63.

**DONE when:** as 63 on the Galaxy Tab; touch latency unchanged from Phase 4 (the probe stayed pure — measured, not assumed).

## Step 65 — Session replay (desktop machine authored; the iPad records)
**Spec:** QOL §5; SOUND §5.

* The file: timestamped bytes + params/state changes + **gesture calls** (recorded — the `[ITERATE]` resolves yes, so pen performances replay complete) **+ frame boundaries** — the per-frame drain points, as a frame index per event or tick markers. This field is what makes cross-device determinism POSSIBLE: the engine coalesces continuous dimensions per frame, and re-bucketing by wall time on a device with different frame cadence (120 Hz iPad → 60 Hz desktop) yields a different pass sequence and a field that diverges through no fault of the operators. **Playback drives the scripted clock through the recorded frame boundaries** — the evidence tooling's own pattern, productised. Version-stamped, source device named; recording on the tablets extends the Evidence byte log; replay through the loopback on ANY shell with the banner (source device, app version); re-dip at a new resolution or palette; **replay re-sounds** through Voxo.

**DONE when:** a session recorded on the iPad replays on the desktop within the §4.6 tier tolerance (measured — cross-device is a requirement, and achievable BECAUSE playback runs the recorded frame boundaries on the scripted clock, never wall-time re-bucketing); the same file re-sounds; a Metal-recorded session replays on the Linux box within its tier; a deliberate wall-time-re-bucketed replay is the negative test — it must diverge, proving the frame field is load-bearing.

## Step 66 — Web marble (any machine)
**Spec:** INSTRUMENT §4 (overlays only — Play stays web-deferred); QOL §5.

* Layouts 8–12 as visual overlays (the web probe shim carries the state); replay PLAYBACK in the browser for the gallery's "watch it again" links (bytes → `sumi_push_midi` on the scripted clock) if it fits the session — otherwise deferred with a note.

**DONE when:** the web gate is green; the overlays draw. **Phase end:** tag `v2.0.0-alpha.4`; fold `DECISIONS_8`.

---

# Phase 10 — Publish (steps 67–70)
**The Phase-5 machinery, run again.** Open `_work/DECISIONS_9.md` (and extend `build-notes.mjs`'s loop to it).

## Step 67 — Documentation (any machine)
**Spec:** SPEC §9.6 (the books); every Phase 6–9 spec's `[ITERATE]` list. **Author input:** the burst page's note (from 38), new gallery performances (an Anod piece, a trumpet piece, a bowed Suzu piece with the orbit trace on).

* **The Anod operator book:** five pages with live scenes (torsion, Chladni, burst, spark, Chirikov), the class table in the book's index, the burst page carrying the serendipity note and the Jaffer-lineage line, literature-checked; **the medium guide** (switching, palettes and the editor, substrate, prints and the ledger); **the instrument pages** (trumpet, trombone, Wicki–Hayden, strings with its three tuning presets, theremin — fingering, the CC rows); **Voxo's guide** (loading a library, what the compat report means, foreground-only stated plainly, the licensing-posture page, "Voxo Dorean, later"); replay; presets; **Suzu's phase-space chapter** (SYNTH §6 — the DSP class table as the operator book's audio twin, the Gordon–Smith and CFL justifications with their archived charts as FIGURES, no new wasm scenes: the rotor's spectrogram sits beside the visual Chirikov page, cross-linked — one theorem, two senses; the patch guide; the bow's energy story told plainly); the settings reference rewritten; **the MIDI implementation chart** re-verified against fresh byte logs (fingering CCs, CC 122, the sampler's bindings); citations (Chirikov, Greene, Moser, Meiss; Aref & Ottino; the Decent Sampler format's author) and acknowledgments (Professor Jaffer first; **Ichisuke Fujioka honoured although the medium is Anod**). Mechanics: build against the RC (`PUBLIC_MARBLE_URL=/marble/rc/` in preview); merge with the stable tag (DECISIONS_4 #82).

**DONE when:** `check.mjs` is green with sixteen scenes in its list and every one embedded; `chart_check.py` is green against the new logs; every `[ITERATE]` of the four specs is either resolved in a decision entry or documented as a limit on the page that owns it.

## Step 68 — Store beta wave (both stores; human-heavy; needs 46, 55, 59, 66, 67)
**Spec:** SPEC §9.4 (the Phase-5 beta procedure).

* `v2.0.0-rc.N` tags; TestFlight external and Play closed on the RC; the desktop RCs through the pre-release draft (cask PR unmerged, the `rc` apt suite, winget skipped — the Phase-5 discipline); the three questions plus Voxo's (latency felt, memory warnings seen, libraries that failed to load — compat reports collected) plus Suzu's (does the glide sing, does the bow hold, which patch did you keep); ≥ 2 weeks; docs-class items ship immediately (`pages.yml`).

**DONE when:** one full wave completed; every report triaged with a class; ≥ 5 external testers played Anod, ≥ 5 played with Voxo on, ≥ 5 played a Suzu patch (store metrics and the questions).

## Step 69 — Feedback incorporation & release candidates (iterative; any machine per item)
**Spec:** SPEC §9.4. The only step that loops. Core reopened for FIXES only (bug → regression test → fix); UX-feel items take the smallest change with the author's sign-off; waivers recorded in `DECISIONS_9`; the final RC nominated.

* **Carried from Phase 7, a nice-to-have (DECISIONS_6 #42):** output-device selection in the desktop settings window's Sound section. Today every desktop follows the default output through a device switch without a stop (DECISIONS_6 #33, #41), which is what the sound spec asked; a device row (miniaudio's enumeration, the chosen name in the INI, "System default" the first entry and the fallback when the name is gone) is the smallest change, taken here if the beta asks for it or the author wants it — a shell item, no core or Voxo ABI change.

**DONE when:** zero open must-fix items; every fixed report reporter-confirmed or confirmed-unreachable; all suites green on the final RC on all platforms; the final RC has sat on both tracks ≥ 3 days.

## Step 70 — 2.0 release
**Spec:** SPEC §9.1, §9.4.

* The promotion of 65's final RC — zero code changes. Tag `v2.0.0`: the spine and lanes; publish the draft (cask, winget, `pages.yml`/apt fire); stores promoted; the changelog's 2.0 section is the notes everywhere; **the libsumi SDK question decided** (publish `sumi_core.h` + binaries as a versioned artifact against the settled 1.0.0 ABI, or defer again — the Part-4 prerequisites list); the note to Professor Jaffer about the multipole extension, in the author's voice, optional; Voxo Dorean stays deferred with its charter recorded. Then the fold: `DECISIONS_9` → `docs/DECISIONS.md`, this Part → `docs/ROADMAP.md`, the five specs (SYNTH included) → `docs/PROJECT_SPEC.md`, evidence out of the tree.

**DONE when:** all five platforms install 2.0.0 through their normal channels; the site documents 2.0 with every demo live at `/marble/`; both store listings are public; the fold is committed.

---

## Deferred (explicitly out of Phases 6–10 — do not implement)
Physical stick-slip (Helmholtz) bowing, 2D FDTD membranes/gongs (the budget arithmetic in SYNTH §7), user-editable modal ratio tables, port-Hamiltonian full models, Suzu on the web; Voxo Dorean (background execution, disk streaming, its own shell — SOUND §7); offline audio bounce of a replay; Play mode on the web and Voxo on the web; woodwind key systems, microtonal/scala tunings and the fretboard tuning table, two-handed split layouts (INSTRUMENT §6); the palette curve's "advanced fold"; field undo (deliberately absent — QOL §6); localization; auto-update; telemetry (never); Microsoft Store; Flatpak beyond the Phase-5 verdict.

---

## The burst page's author note (step-38 input, lands in the docs at step 63)

**Lineage line (top of the burst's operator page):**

> Method after A. Jaffer, "The Lamb–Oseen Vortex and Paint Marbling" (arXiv:1810.04646), extended here to the multipoles m ≥ 2.

**The author's note (signed, first person — trim or roughen freely; the load-bearing sentences are the claim-nothing line and the last one):**

> A note on where this formula came from. I wasn't looking for a theorem — I was looking for a spark. The strikes in the electric medium felt too round, too liquid, nothing like the discharge I see when the music peaks. So I did what this whole project does: I followed Professor Jaffer's method. His Lamb–Oseen paper integrates a viscous flow over time to get a closed-form displacement; I tried the same integration on the higher multipoles, expecting special functions — and for m ≥ 2 the integral simply closed. The logarithmic kernel that haunts the dipole vanishes, and everything becomes elementary. I searched the literature as well as I could and did not find it stated in this form, but I claim nothing: serendipity did the work, and Jaffer drew the map. I only needed the discharge to bloom sharp and die soft, the way it sounds in my head. Here is its closed form.

(Preconditions from step 38 still apply: the literature check is recorded in the evidence BEFORE this note ships, and the note ships in the author's voice, signed.)

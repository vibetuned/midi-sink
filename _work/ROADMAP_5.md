# The open roadmap — v2.0, Phase 10 (steps 67–70: Publish)
**Phases 6 (steps 35–46), 7 (steps 47–55), 8 (steps 55b, 56–59, 59b–59c) and 9 (steps 60–66) are DONE and folded into `docs/ROADMAP.md` Parts 5, 6, 7 and 8 (2026-09-26, 2026-09-28, 2026-10-04 and 2026-10-06). This file keeps the rest of the arc.**
**Companions: `docs/PROJECT_SPEC.md` (`SPEC §n`; the medium's section, the shipped quality-of-life items, the sound's and the synth's sections are drafted for it in `specs/TO_PROJECT_SPEC.md` until the author transcribes them — `SYNTH §n` means the synth spec as it stood, `git show 5111748:specs/SYNTH_SPEC.md`), `specs/INSTRUMENT_SPEC.md` (`INSTRUMENT §n`), `specs/QUALITY_OF_LIFE_SPEC.md` (`QOL §n` — the undone items only); decision logs `_work/DECISIONS_8.md` (Instruments) and `_work/DECISIONS_9.md` (Publish), one per phase, referenced as `DECISIONS_8 #n` …; history `docs/CHANGELOG.md` (v2.0.0); the Phase-6, -7 and -8 records `docs/DECISIONS.md` Parts V, VI and VII (`DECISIONS_5 #n`, `DECISIONS_6 #n`, `DECISIONS_7 #n`).**
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
* Evidence per step under `docs/evidence/<step>/`; at each phase end the fold: that phase's `_work/DECISIONS_<n>.md` merges into `docs/DECISIONS.md` as the next Part, evidence condenses into `CHANGELOG.md` and leaves the tree (git keeps it), scripts worth keeping move to `tools/`. `site/scripts/build-notes.mjs` renders `_work/DECISIONS_{5,6,7,8}.md` while in flight — Publish extends the loop to 9 (Parts V–VII are in `docs/DECISIONS.md` now).
* **Documentation timing:** guide fixes ship to `main` at any time (`pages.yml`). Pages for NEW operators, layouts and Voxo are drafted in the step's evidence folder (the burst page in the author's voice) and move into `site/` in step 63 — the live demos would otherwise point at scenes the released wasm does not know.
* **Pre-release tags** end Phases 6, 7, 8 and 9 (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a pre-release, and the lanes stay proven); the author installs the build on every device and plays it. Phase 9 uses `v2.0.0-rc.N`. Nothing reaches a stable channel before step 66.
* Credentials: Phases 6–9 need none beyond the machines; Phase 10 reuses the Phase-5 set (Developer ID, ASC, Play, tap token, winget token, apt key). Author inputs (recordings, taste sign-offs, the demo instrument) are listed per step so they can be staged before the session.

---

# Phase 10 — Publish (steps 67–70)
**The Phase-5 machinery, run again.** Open `_work/DECISIONS_9.md` (and extend `build-notes.mjs`'s loop to it).

## Step 67 — Documentation (any machine)
**Spec:** SPEC §9.6 (the books); every Phase 6–9 spec's `[ITERATE]` list. **Author input:** the burst page's note (from 38), new gallery performances (an Anod piece, a trumpet piece, a bowed Suzu piece with the orbit trace on).

* **The Anod operator book:** five pages with live scenes (torsion, Chladni, burst, spark, Chirikov), the class table in the book's index, the burst page carrying the serendipity note and the Jaffer-lineage line, literature-checked; **the medium guide** (switching, palettes and the editor, substrate, prints and the ledger); **the instrument pages** (trumpet, trombone, Wicki–Hayden, strings with its three tuning presets, theremin — fingering, the CC rows); **Voxo's guide** (loading a library, what the compat report means, foreground-only stated plainly, the licensing-posture page, "Voxo Dorean, later"); replay; presets; **Suzu's phase-space chapter** (SYNTH §6 — the DSP class table as the operator book's audio twin, the Gordon–Smith and CFL justifications with their archived charts as FIGURES, the Suzu lab's panels (59b–59c) as its live scenes — the real engine in the browser, built from the tag like `/marble/`; the rotor's spectrogram sits beside the visual Chirikov page, cross-linked — one theorem, two senses; the patch guide; the bow's energy story told plainly); the settings reference rewritten; **the MIDI implementation chart** re-verified against fresh byte logs (fingering CCs, CC 122, the sampler's bindings); citations (Chirikov, Greene, Moser, Meiss; Aref & Ottino; the Decent Sampler format's author) and acknowledgments (Professor Jaffer first;). Mechanics: build against the RC (`PUBLIC_MARBLE_URL=/marble/rc/` in preview); merge with the stable tag (DECISIONS_4 #82).

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
Physical stick-slip (Helmholtz) bowing, 2D FDTD membranes/gongs (the budget arithmetic in SYNTH §7), user-editable modal ratio tables, port-Hamiltonian full models; Voxo Dorean (background execution, disk streaming, its own shell — SOUND §7); offline audio bounce of a replay; Play mode on the web and Voxo's sampler on the web (Suzu runs there since 59b, deviceless, in the lab); woodwind key systems, microtonal/scala tunings and the fretboard tuning table, two-handed split layouts (INSTRUMENT §6); the palette curve's "advanced fold"; field undo (deliberately absent — QOL §6); localization; auto-update; telemetry (never); Microsoft Store; Flatpak beyond the Phase-5 verdict.

---

## The burst page's author note (step-38 input, lands in the docs at step 63)

**Lineage line (top of the burst's operator page):**

> Method after A. Jaffer, "The Lamb–Oseen Vortex and Paint Marbling" (arXiv:1810.04646), extended here to the multipoles m ≥ 2.

**The author's note (signed, first person — trim or roughen freely; the load-bearing sentences are the claim-nothing line and the last one):**

> A note on where this formula came from. I wasn't looking for a theorem — I was looking for a spark. The strikes in the electric medium felt too round, too liquid, nothing like the discharge I see when the music peaks. So I did what this whole project does: I followed Professor Jaffer's method. His Lamb–Oseen paper integrates a viscous flow over time to get a closed-form displacement; I tried the same integration on the higher multipoles, expecting special functions — and for m ≥ 2 the integral simply closed. The logarithmic kernel that haunts the dipole vanishes, and everything becomes elementary. I searched the literature as well as I could and did not find it stated in this form, but I claim nothing: serendipity did the work, and Jaffer drew the map. I only needed the discharge to bloom sharp and die soft, the way it sounds in my head. Here is its closed form.

(Preconditions from step 38 still apply: the literature check is recorded in the evidence BEFORE this note ships, and the note ships in the author's voice, signed.)

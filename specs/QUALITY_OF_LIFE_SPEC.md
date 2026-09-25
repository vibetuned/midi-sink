# QUALITY OF LIFE SPECIFICATION: User-Requested Polish — the undone items
**Phases 7–9, everyone's side. Companions: `PROJECT_SPEC.md`, `INSTRUMENT_SPEC.md`, `SOUND_SPEC.md`. Reduced at the Phase-6 close (2026-09-26): palettes, substrate, presets, prints and the dip/clear copy shipped in step 43 and the shells — their shipped form is drafted for `PROJECT_SPEC.md` §11 in `specs/TO_PROJECT_SPEC.md` (decisions `DECISIONS_5 #63–#68, #73–#74, #79, #86`). What remains below is what has not shipped. Open points are marked `[ITERATE]`. Items here must never compromise the two standing product rules: no telemetry, and the canvas stays an instrument (no feature that turns playing into menu-diving).**

---

## 1. Session replay (the byte stream is the recording) — Phase 8, step 61

* Everything musical is already MIDI — including fingering (INSTRUMENT_SPEC §1) — so **record the timestamped byte stream + params/state changes + gesture calls, replay it through the loopback** and the performance reconstructs deterministically. The pen tracer proved the pattern; this productizes it.
* **Cross-device is a requirement, not a bonus** (recurrent user ask): the replay file is platform-neutral and version-stamped — record on the iPad, replay on the desktop (or any shell). Determinism holds because the field math is identical across backends within the documented §4.6 tiers (Metal bitwise; GL, D3D11 and GLES at their tiers — `DECISIONS_5 #87`); the replay banner states the source device and app version.
* With SOUND_SPEC's sampler, a replay **re-sounds**: the same bytes feed the audio engine, so an iPad session replays on desktop with a bigger instrument loaded. Offline audio bounce of a replay is the natural deferred extension.
* Gestures are recorded beside the bytes (resolved: `DECISIONS_5 #8`), so pen performances replay complete; replay files carry frame boundaries and playback drives the scripted clock through them (`#12`) — wall-time re-bucketing is the documented anti-pattern and the negative test.
* Replay enables: re-dipping a past performance at a new resolution or palette, and the gallery's "watch it again" links.

## 2. Small UX items still open — Phase 8, step 58 (hostmpe and the shells)

* **Panic** — the settings action exists on the tablets ("Stop all notes"); the desktop's settings window has none yet, and the strip button `[ITERATE: a strip button?]` is unbuilt.
* Layout quick-switch on the strip `[ITERATE: cycles a user-chosen subset, not all]`.
* A **preset-next** button on the strip `[ITERATE: a performance feature — which presets cycle?]`.
* Per-device default presets: connecting a known controller (ROLI/Osmose/Brisa profile detected per §2.5 heuristics) **offers** its preset (resolved as offer, never auto-apply — `DECISIONS_5 #7`).
* Left-handed mirroring for the play surface and strip.
* **Undo: deliberately absent** (author's decision, recorded): field undo means snapshot ring-buffers and a state model that every later feature must respect — the cost compounds forever. The dip is the instrument's undo: fresh paper.

## 3. Small items carried from the shipped sections

* Substrate: `[ITERATE: expose the fibre angle-drift amount?]` (the fibre scale shipped; the angle drift is a constant).
* Prints: TIFF-16 for print workflows `[ITERATE: demand-check first]` (PNG shipped, over alpha in Anod).
* Palettes: the ink-depth / glow curve is fixed per medium in 2.0; the "advanced fold" that would expose it stays deferred.

---

## 4. Sequencing note
Replay depends on INSTRUMENT_SPEC's fingering-as-MIDI to be complete and on Voxo (Phase 7) to re-sound, so it lands after both (step 61); the strip items land with the hostmpe step of Phase 8 (58).

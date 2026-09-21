# QUALITY OF LIFE SPECIFICATION: User-Requested Polish
**Phase 6, everyone's side. Companions: `MEDIUM_SPEC.md`, `INSTRUMENT_SPEC.md`. Driven by beta/user feedback; iteration expected — open points are marked `[ITERATE]`. Items here must never compromise the two standing product rules: no telemetry, and the canvas stays an instrument (no feature that turns playing into menu-diving).**

---

## 1. User palettes (the most-requested item)

* **Model: a palette is an N-stop gradient (2–8 stops) plus an ink-depth curve**, applied by the composite exactly like the built-ins (built-ins become presets expressed in the same model — one code path).
* ABI: `sumi_set_palette(inst, const sumi_palette_t*)` — a POD of stops (linear-space RGB + position) and curve params; `active_palette_id` gains a `SUMI_PALETTE_CUSTOM` value. Part of the Phase-6 ABI event.
* Shells own the editor UI (stop picker, live preview on the running canvas) and persistence (§3). Both media consume the same palette model — an Anod glow palette and a sumi ink palette differ in how the composite applies it, not in the data.
* **Color-blind-considerate presets** ship alongside (deuteranopia/protanopia-safe pairs) `[ITERATE: which sets]`.
* **Identity guardrail:** custom palettes flow through the same ink-depth/glow curve as the built-ins — users choose the hues, the medium keeps its rendering character (pooled near-black sumi, strain-lit anod). The curated preset library is the identity statement; the editor is freedom within it. `[ITERATE: curve fully fixed per medium, or exposed as an "advanced" fold?]`
* `[ITERATE: per-drop hue drift (aux) under custom palettes — expose the drift range as a palette field or keep it global?]`

## 2. Paper & substrate customization

* Sumi: paper tint (cream ↔ white ↔ toned), roughness presets surfacing the existing `paper_roughness` plus fiber scale `[ITERATE: expose fiber angle-drift amount?]`. Anod: substrate darkness and grain per MEDIUM_SPEC §3.
* All substrate options respect the screen-locked invariant by construction (they are composite-side).

## 3. Presets & persistence (un-deferring Phase 4's item)

* **A preset = params + CC map + strip assignments + custom palette + layout state defaults**, serialized host-side (JSON, shells own storage; the core stays stateless about files).
* Named presets, switchable from settings and `[ITERATE: from the strip? a preset-next button is a performance feature]`; the last session restores on launch.
* Presets are shareable files — export/import in the shells `[ITERATE: schema versioning rule — presets carry the sumi_version they were made with; unknown fields ignored, missing fields defaulted]`.

## 4. Prints & export

* Export resolution independent of screen (re-render the field at target resolution — it is resolution-independent by design; the dip machinery gains a target-size argument) `[ITERATE: cap? 8k?]`.
* Formats: PNG (current), optional TIFF-16 for print workflows `[ITERATE: demand-check first]`; Anod prints optionally on transparent background (glow over alpha).
* **A print ledger** in the shells: dips of the current session with thumbnails, re-export at any size while the session lives.

## 5. Session replay (the byte stream is the recording)

* Everything musical is already MIDI — including fingering (INSTRUMENT_SPEC §1) — so **record the timestamped byte stream + params/state changes + gesture calls, replay it through the loopback** and the performance reconstructs deterministically. The pen tracer proved the pattern; this productizes it.
* **Cross-device is a requirement, not a bonus** (recurrent user ask): the replay file is platform-neutral and version-stamped — record on the iPad, replay on the desktop (or any shell). Determinism holds because the field math is identical across backends within the documented §4.6 tiers; the replay banner states the source device and app version.
* With SOUND_SPEC's sampler, a replay **re-sounds**: the same bytes feed the audio engine, so an iPad session replays on desktop with a bigger instrument loaded. Offline audio bounce of a replay is the natural deferred extension.
* Honest boundary stated in the UI: stylus wakes are gesture-only (the documented invariant) — replays of pen performances omit wakes unless the gesture stream is also recorded `[ITERATE: record gestures too? cheap — timestamped gesture calls beside the bytes — and it makes replay complete; proposal: yes]`.
* Replay enables: re-dipping a past performance at a new resolution or palette, and the gallery's "watch it again" links.

## 6. Small UX items from the feedback pile

* **Panic** (all-notes-off + voice flush) as a settings action and `[ITERATE: a strip button?]` — every instrument needs one.
* **Canvas-clear vs paper-dip distinction** surfaced in UI copy (clear discards; dip prints then clears) — a repeated beta confusion.
* Layout quick-switch on the strip `[ITERATE: cycles a user-chosen subset, not all]`.
* Per-device default presets: connecting a known controller (ROLI/Osmose/Brisa profile detected per §2.5 heuristics) offers its preset `[ITERATE: auto-apply vs offer]`.
* Left-handed mirroring for the play surface and strip.
* **Undo: deliberately absent** (author's decision, recorded): field undo means snapshot ring-buffers and a state model that every later feature must respect — the cost compounds forever. The dip is the instrument's undo: fresh paper.

---

## 7. Sequencing note
Palettes and presets are pure-win and independent of the media/instrument ABI event's *design* but share its release; replay depends on INSTRUMENT_SPEC's fingering-as-MIDI to be complete. Proposal: QoL items ship interleaved through Phase 6's roadmap rather than as a trailing step — palettes early (most-requested), replay after fingering lands.

# SOUND SPECIFICATION: The Internal MPE Sampler (Decent Sampler-Compatible)
**Phase 7, the missing half. Companions: `PROJECT_SPEC.md` (the medium's section drafted in `TO_PROJECT_SPEC.md` until transcribed), `INSTRUMENT_SPEC.md`, `QUALITY_OF_LIFE_SPEC.md`. Iteration expected — open points are marked `[ITERATE]`.**

**Why this exists (the recurrent complaint, stated):** the app is a controller without a sound. On iOS most plugins die in the background; Android lacks a synth ecosystem. Users want to *play it everywhere*. The fix is an internal sampler speaking a format with a real free-instrument ecosystem: **Decent Sampler presets** (`.dspreset` XML + samples, `.dslibrary` zip bundles; Pianobook and friends).

---

## 1. Architecture: a sibling core, not a libsumi feature

* **A second library beside libsumi** (working name: **Voxo** — the author's water-box/vox play — `[ITERATE: confirm]`): C++ behind a **pure C ABI** (the sumi_core.h rules verbatim: no STL/exceptions across it, three-state export macro, version function). The rendering engine stays audio-free; the shells wire both.
* **Feed: the same bytes.** The shell's single-producer merge point fans the identical raw MIDI stream into two SPSC queues — one for libsumi, one for the sampler. Same producer thread contract; two independent consumers.
* **Front half is shared source:** the sampler compiles `midi_normalizer.cpp` (and the voice-table logic) from the same repo sources — one MPE decoder, two builds, zero runtime coupling. MCM zones, RPN 0 ranges (±48 member / ±2 master), mode detection, sustain, the stuck-voice safeguard: all inherited, all already soak-tested. The sampler consumes the normalized vocabulary (VoiceBegin/Glide/Press/Slide/Swirl/End + GlobalCtl), not raw bytes, past its ingest queue.
* **Audio backend: miniaudio** (CoreAudio / WASAPI / ALSA / AAudio+OpenSL / WebAudio) — chosen over sokol_audio for Android's low-latency path. `[ITERATE: confirm after an Android latency spike — AAudio PerformanceMode::LowLatency is the acceptance bar.]` **Fallback ladder, stated correctly:** RtAudio is DESKTOP-ONLY (no iOS/Android backends exist) — it is the desktop escape hatch; the mobile escape hatches are direct AAudio (Android) and AVAudioEngine (iOS). Nobody plans a mobile rescue with RtAudio.
* **The callback thread contract (the hardest real-time contract in the app):** lock-free rings only, zero allocations, zero locks, zero logging in the callback; all voice state transitions via the event queue drained at block start. Block size 128–256 frames target `[ITERATE: per-platform defaults]`.

## 2. Voice model

* **MPE dispatch is the easy part** (one member channel = one performance voice — no stealing heuristics), but a *performance voice* activates a **sample stack**: velocity layers (with crossfades), round robins, release samples, per-group envelopes. The dispatcher is 1:1; the voice is small-orchestral inside.
* **Pitch (X):** ratio = 2^((note − root + bend·range)/12), recomputed per block with per-sample ramping; **Hermite (4-point cubic) interpolation** — linear interpolation audibly dulls and aliases under ±48-semitone glides, and wide glides are this instrument's signature gesture.
* **Expression (Y/Z):** per-voice 1-pole smoothers on pressure and CC74, α from DS's rising/falling smoothing times; default MPE map = DS's own (pressure → gain/expression, CC74 → cutoff/timbre) with the preset's `<bindings>` overriding. Swirl (0xA0) is offered to bindings as an extra source `[ITERATE: default target — none? tremolo depth?]`.
* Sustain (CC 64) honored in the release logic; strip volume → master gain; **local control** toggle (honoring CC 122 for the classic touch): internal sound and outbound MIDI are independent switches.

## 3. Decent Sampler format: the v1 subset, honestly bounded

* **v1 parses:** sample zones (lo/hi note & velocity, rootNote, tuning), groups, ADSR, loops (with crossfade), round-robin/sequence modes, one per-voice LP filter, DS MPE bindings, **bus reverb + delay** (author's call — the two effects even a beginning musician actually uses; simple real-time-safe algorithmic implementations — Freeverb-class + feedback delay — reading the preset's DS effect parameters, post-sum on the bus, never per-voice), `.dslibrary` zip loading (miniz), WAV/FLAC decode (dr_wav/dr_flac; `[ITERATE: AIFF? some Pianobook libraries use it]`), pugixml for the XML.
  * **Why loops and the filter are load-bearing, recorded so they never get "simplified" out:** loop points are what let a 3-second sample sustain a 30-second pad — without them every held note dies at file end, killing exactly the instruments beginners reach for (pads, organs, strings); and CC74 → cutoff is DS's default MPE timbre map — without the filter, the pen's Y axis and every slide gesture is audibly dead.
* **v1 does not parse:** chorus, convolution, the full modulation matrix, UI/skin definitions, sample streaming. **Every preset load produces a compat report** — "uses convolution reverb: will play without it" — shown once, calmly; compatibility is a dialog, not a crash or a silent wrong sound.
* **Memory: preload-first, advisory gate, FOREGROUND-ONLY in v1.** v1 **preloads to RAM** — the author's instinct is right that streaming is v1 overkill and that modern Android handles large native allocations (native heap sits outside the Java limit; only low-RAM devices and the LMK bite). The size gate is an **advisory warning, not a wall**, and iOS budgets it via `os_proc_available_memory()` (Jetsam limits are real, but the dangerous combination — large preload while BACKGROUNDED — is out of v1 scope entirely, see §4). Disk streaming is deferred — and now has a designated home: the Voxo Dorean sister app (§7).
* **Licensing posture (docs page):** the format is implementable (formats are not copyrightable); the app **bundles zero libraries**; users load their own, and each library's own terms travel with it. One tiny public-domain demo instrument ships so first launch makes a sound `[ITERATE: record it ourselves — a music-box or kalimba, on brand]`.

## 4. Platform notes

* **iOS:** **foreground audio only in v1** — the re-scoped truth: internal sound dissolves the background complaint at the root (users needed background plugins because they needed a SECOND app for sound; with sound inside, midi-sink IS the foreground app while being played). AVAudioSession configured for low-latency playback; audio pauses with the visuals on backgrounding, resumes on return; interruption handling (calls, Siri) per session callbacks. No `UIBackgroundModes`, no background Jetsam exposure.
* **Android:** AAudio low-latency stream, audio focus handling; foreground-only in v1 (no foreground-service machinery — that question moves to Voxo Dorean).
* **Desktop:** trivial by comparison; default-device hotplug handling.
* **Web:** deferred (wasm + WebAudio is plausible; memory and fetch-size make it a later conversation).

## 5. Ties into the rest of the system

* **Replay re-sounds** (QUALITY_OF_LIFE_SPEC §5): replays feed the sampler like live bytes — record on iPad, replay on desktop with a grander instrument loaded. Deferred: offline bounce of a replay to WAV.
* **Presets** (QoL §3) gain an instrument slot: a performance preset may reference a loaded library by name/hash (never by embedding samples).
* The sampler is optional at runtime: OFF = today's controller, nothing regresses. The visual budget is untouched — the audio thread never blocks the render thread, and the acceptance suite includes a combined stress (Osmose storm + heavy preset) holding both 60 fps and glitch-free audio `[ITERATE: XRun budget]`.

## 6. Roadmap sketch (to be formalized)
Backend spike (miniaudio latency on all four native platforms, sine per voice) → normalizer-fed voice dispatch + Hermite pitch (the "it glides" milestone) → DS subset parser + compat report → layers/round-robin/release/loops → smoothing & bindings → bus reverb + delay → foreground session handling (iOS interruptions, Android focus) → advisory gate + demo instrument → combined stress + beta. Desktop first, per house rule.

## 7. Voxo Dorean (the sister app — deferred, designed for)
The background use case — the tablet as a standalone sound module while OTHER apps hold the screen — is a different product with different constraints, and it gets one: **Voxo Dorean**, a small standalone app around the same Voxo library (the sibling-library architecture's third payoff: one more tiny host shell, zero engine changes). Its charter, when its time comes: background execution (`UIBackgroundModes: audio` / Android foreground service), **disk streaming to RAM** as its core competency (the deferred item lives here, where background Jetsam budgets make it mandatory rather than optional), a minimal UI (library picker, MIDI input, meters), and the strict memory discipline the background demands. Nothing in midi-sink v1 blocks on it; everything in Voxo v1 (the library, the DS subset, the compat report) is inherited by it for free.

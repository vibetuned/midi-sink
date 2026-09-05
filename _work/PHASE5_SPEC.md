# PHASE 5 SPECIFICATION: Packaging, Release Engineering, Web & Documentation
**Companions: `PROJECT_SPEC.md`, `PHASE4_SPEC.md`, `DECISIONS*.md` (design notes), `CHANGELOG.md` (condensed DONE evidence), `ROADMAP_4.md`.**
**Standing assumption: the author's existing publishing infrastructure is an INPUT, not a deliverable** — Apple Developer ID + App Store Connect, Google Play account, an existing Homebrew tap, an apt repository with its GPG signing key, prior winget manifests. Pipelines are parameterized by these (CI secrets, repo/tap names); no step teaches account setup.

---

## 1. Scope & principles

Phase 5 turns a five-platform engine with harnesses into a released product family:

* **Desktop (macOS, Windows, Linux)** — the harness becomes a product, distributed through the author's existing channels (tap cask, winget, apt) plus GitHub Releases.
* **iOS / Android** — store-published, **beta first**: TestFlight and a Play closed track with real users before any production listing (author's standing practice; the phase has an explicit feedback gate).
* **Web** — a WebGPU/wasm **marble-mode** build: the sixth host shell, and the live-example engine for the docs.
* **Documentation** — a public site: user guide, the operator book with live demos, a performance-video gallery (incl. the Jaffer tribute), the MIDI implementation chart, published design notes (the DECISIONS files, lightly edited) and changelog. **Hard ordering:** the docs need the web build (live examples); the RELEASE LANES need the docs (a cask requires a homepage URL, both stores require a privacy-policy URL, and support/marketing URLs are wanted everywhere — all of which live on the docs site); and the beta needs the docs as its user guide. The chain is therefore: web → docs → lanes → beta → release.

Principles carried over: the core stays frozen except the WebGPU seam (one new swapchain TU, the §4.6 discipline already proven across four backends); everything a tag can build, a tag builds — humans touch only store uploads and review responses; every artifact traces to a commit and a `sumi_version()`.

Versioning: Phase 5 ships **1.0.0**. `sumi_version()` and every platform's marketing version derive from the same git tag (single source: the tag; CI injects it).

---

## 2. Desktop productization

The harness graduates. Known debt from the changelog and DECISIONS:

* **macOS becomes a real `.app` bundle** (currently a bare executable with a runtime Dock-tile icon): Info.plist, icon asset from the existing `tools/gen_icons.py` output, hardened runtime, Developer ID signing, notarization + stapling. The cask installs the bundle.
* **Settings UI** replacing debug keys: the iOS settings sheet is the contents spec (layout picker incl. Piano grid, palettes, bend/slide/press/pinch/vortex/ripple rows, sim_scale, CC map editor, "Paper dip (fresh sheet)" button, MIDI port list with the 1 Hz rescan status). Native-toolkit minimalism is fine (one window, no theming project).
* **`--dev` flag** gates every debug binding (keys 1–9, C/M/K, `--field-dump`, stress feeders, pen tracer hooks). Release builds keep the flag (support asks "run with --dev") but ship with it off.
* **About/version** wired to `sumi_version()` + git describe; **first-run hint** (one dismissible line: where MIDI devices appear, where settings live).
* **Product naming decision** (bundle id, executable name, cask/winget/apt package names) is made once here and recorded in DECISIONS — every later manifest references it.

Windows/Linux inherit the same settings UI and flag; Linux keeps the existing .desktop/hicolor install component (already shipped in Step 14's icon work).

---

## 3. Release engineering (tag → artifacts, all five platforms)

One tag-triggered GitHub Actions release workflow:

* **Matrix:** macos (universal arm64+x86_64 bundle → signed, notarized, stapled DMG), windows (MSVC build → signed-if-cert-present installer + portable zip; unsigned builds are still released, SmartScreen consequence documented in the README, not fought), ubuntu (deb + tarball), and the web lane — the CI-released platforms. **iOS and Android are MANUAL by the author's choice:** Xcode archive → TestFlight and Android Studio signed bundle → Play internal, each walked from a documented `RELEASING.md` checklist against a tagged checkout with the CI-injected version; PR CI keeps mobile build-only compile checks so tags never surprise the archive. Store promotion past the test tracks is human either way (§4).
* **Per-OS pinned `sokol-shdc` fetch** (existing CMake machinery) and the FetchContent pins make builds reproducible; the workflow asserts the ABI/C11 tests, the hostmpe suite, and the §4.6 field regression per backend before packaging anything.
* **Channel automation on release publish:** cask bump PR to the author's tap (version + sha256), winget manifest bump (wingetcreate PR), deb pushed to the existing apt repo with its GPG key from secrets (Release/InRelease re-signed). **Flatpak is a one-session spike, not a commitment:** the open question is ALSA MIDI through the sandbox; if the spike is clean it becomes a channel, otherwise the deb + tarball stand.
* **Release notes generate from CHANGELOG.md** (the condensed-evidence practice continues: each phase's DONE evidence condenses into the changelog, which is the release-notes source — no third format).

---

## 4. Store beta gate (iOS + Android)

Standing practice made explicit: **no production listing before a beta round — and no beta round before the documentation site is live** (testers get a real user guide, and their confusion reports then measure the guide, not its absence). Sequencing is therefore: WebGPU/web → docs → release lanes → beta → release.

* **TestFlight** (external group) and **Play closed testing** track, populated from the same tagged build the workflow produced.
* **Feedback instrument:** a pinned GitHub Discussion (or issue template) per beta wave asking three things — what did you play it with (device/controller), what confused you in the first five minutes, what did you expect Play mode to do that it didn't. First-five-minutes confusion feeds the docs' user guide directly.
* **Store metadata** (screenshots of marbling under real playing, the two modes named plainly, MPE-controller support called out) prepared once, localized later if ever.
* **Exit criteria to production:** one full beta wave (≥ 2 weeks) whose feedback is triaged into classes, then an **incorporation loop** — the one phase-5 step allowed to iterate: bugfix-scoped changes (core included, each fix landing with the regression test that would have caught it), release-candidate tags through the full pipeline, reporter confirmation on the test tracks, author sign-off on any feel change. **1.0 is the promotion of the final confirmed RC, never a build the testers haven't held.** Production promotion is a human click, deliberately.

---

## 5. WebGPU backend & marble web

The sixth host shell, exactly as the architecture intended:

* **Core:** `swapchain_webgpu.cpp` hosts SOKOL_IMPL for the WebGPU backend; `sokol-shdc` gains the WGSL dialect output; the §4.6 cross-backend field regression gains a **web tier** tolerance (like the documented GLES3 mobile tier). RGBA16F filtering is core WebGPU — the field format survives unchanged. The C-ABI is the wasm export surface (Emscripten; exceptions stay off, matching the ABI rules).
* **Host shell:** a page of JS — canvas + WebGPU device/swapchain handed to the core, pointer/touch events → marble gestures (tap drop, drag tine, two-finger twist vortex — pen pointer events additionally drive the wake where the browser reports pen type/pressure), `requestAnimationFrame` loop, resize → `sumi_resize`.
* **Scope: Marble mode only.** Play mode is web-deferred: WebMIDI *input* is wired where available (Chrome/Edge — the ROLI drives the web build there; Safari has never shipped WebMIDI and the page must degrade to gestures-only without a scare banner), but the play surface, allocator, and outbound transports do not exist in the browser in Phase 5.
* **Scene/embed API — the docs contract:** query parameters select an operator demo scene and expose its parameters (`?scene=lamb_oseen&gamma=…&rc=…`, `?scene=pinch&variant=saddle…`), with a minimal-chrome embed mode. **The docs' live examples ARE this artifact** — one wasm binary, no reimplementations to drift.
* Hosted on GitHub Pages beside the docs; built by the same release workflow.

---

## 6. Documentation site

Static site (KaTeX for formulae), deployed to Pages by CI. Four books and a chart:

1. **User guide** — Marble and Play modes (Play flagged iOS/Android-only), per-device setup pages (ROLI Piano, Airwave routing defaults, Osmose, Brisa/Travel Sax wind mode, classic keyboards), layouts (all six incl. Piano grid), the control strip, the stylus (legato feel, per-cell retriggers, barrel dials, wake), paper dips and prints. Beta-wave confusion reports are this book's backlog.
2. **The Operators** — one page per deformation (drop, tine, vortex ×2 profiles, wake, pinch ×2 variants, ripple, Lamb–Oseen swirl, scroll): the formula, its invariants (area-preservation argument, exactness, sub-stepping rule or exemption, ownership/consumer rule), and a **live embedded demo** (the §5 wasm with scene parameters — sliders map to the formula's symbols).
3. **Architecture** — the Option-2 pattern distilled from PROJECT_SPEC for host-shell builders: the C-ABI, threading contract, §4.6 orientation discipline, per-backend swapchain ownership.
4. **Performance gallery** — videos of real performances (recorded by the author; embedded, self-hosted or unlisted-video embeds — author's call on hosting), each captioned with device + layout + modes used, so the gallery doubles as a "what it can do" index into the user guide. Includes the **Jaffer tribute performance**: the piece from his marbling videos, performed by the author on this instrument (identify the exact piece from his video credits at production time; performed-by-author means only the composition's status matters — verify it is public domain, which classical/folk dance pieces are). Optionally paired with a scripted **Turkish-moire scene** — a pattern Jaffer analyzes in his pigment-transport work — as a bridge between the gallery and the operator book.
5. **Design notes & changelog** — the DECISIONS files published (lightly edited: internal paths trimmed, entries kept verbatim otherwise — they are the honest engineering record) and CHANGELOG.md rendered.

**Store-required pages** — the site carries, at stable URLs published before any lane or manifest references them: the **privacy policy** (short and honest: the app collects nothing — no telemetry, no accounts, no network calls beyond the user's own MIDI transports; store-specific clauses as each console requires), a **support/contact page**, and the **homepage** itself (the cask's `homepage` field, the stores' marketing URL). URL stability is a contract: lanes and store listings hardcode these.

**The MIDI implementation chart** — one page, the instrument's contract: message support in/out per mode, MCM behavior and zone, bend ranges (±48 member / ±2 master), the CC table (1, 2/7/11 aliases, 64, 74, 102, 103, map-routable dims), 0xA0 poly pressure, the consumer/ownership rules (bend_mode / slide_mode / press_mode), per-transport rate policies, and what a DAW recording does and does not capture (the wake invariant, stated for users).

**Citations & acknowledgments:** a dedicated page, and the project's happiest discovery belongs on it: the wake and the Lamb–Oseen swirl are not merely in Jaffer's spirit — they are HIS published extensions, independently converged on. Cite at minimum: Lu, Jaffer, Jin, Zhao & Mao, "Mathematical Marbling," *IEEE Computer Graphics and Applications* vol. 32 no. 6 (2012) pp. 26–35; Jaffer, "Oseen Flow in Paint Marbling," arXiv:1702.02106 (the closed-form short-stylus-stroke field — the wake's lineage); Jaffer, "The Lamb–Oseen Vortex and Paint Marbling," arXiv:1810.04646 (the swirl's own paper); his marbling pages at MIT CSAIL. Then Lamb's *Hydrodynamics* (the doublet and the vortex), Rankine (1858), and the MMA/AMEI MPE specification. Final citation details verified against the publications during writing, never from memory. The operator pages for the wake and the swirl each carry a one-line lineage note pointing at the corresponding Jaffer paper. On release, send Professor Jaffer a link with a short note — the Lamb–Oseen extension of his closed-form family is the kind of thing an author enjoys seeing.

---

## 7. Explicitly out of Phase 5 scope
Play mode on the web (WebMIDI output, wasm hostmpe), localization, Flatpak beyond the spike (if it fails), Windows Store / Microsoft Store listing, auto-update mechanisms, telemetry of any kind, Phase 6 instrument layouts (trumpet valves / trombone slide / Wicki–Hayden / fretboard / theremin — parked with the noted architectural question: stateful layouts need a probe-state design).

# IMPLEMENTATION ROADMAP 4: Packaging, Release, Web & Documentation
**Companions: `PHASE5_SPEC.md`, `PROJECT_SPEC.md`, `DECISIONS*.md`, `CHANGELOG.md`. Start `DECISIONS_4.md`.**
**Scope: Steps 23–34. ONE PLATFORM PER STEP except 33 — each platform's agent runs on its machine. Hard ordering: WebGPU/web (25) → docs (26) → release lanes (27–31, order-flexible among themselves) → beta (32) → feedback incorporation (33, the only step that loops) → release (34). The docs precede every lane because the lanes and store listings hardcode the site's homepage, privacy-policy, and support URLs; 1.0 is the promotion of 33's final release candidate, never a fresh build.**

---

## Working Rules (apply to every step)

* All prior working rules hold. The core stays frozen except Step 30's WebGPU seam (one swapchain TU + shdc dialect — the Step 11/12/14 pattern, fourth verse).
* **One platform per step.** A step never touches a second platform's build, signing, or store. Shared code (settings UI, workflow spine) is authored once in its designated step; other platforms' steps consume and verify it, never rewrite it.
* **The author's existing infrastructure is an input:** Developer ID, ASC app records, Play account, the tap, the apt repo + GPG key, prior winget manifests. Each step lists the credentials it needs at the top so the author stages secrets before the session; no step creates accounts.
* Version strings come from the git tag via CI injection — no hand-edited version numbers anywhere.
* Evidence condenses into `CHANGELOG.md`; release notes generate from it. Store submissions and beta promotions are HUMAN actions by design; CI stops at "uploaded."

---

## Step 23 — Desktop productization (macOS machine)
**Spec:** PHASE5 §2. **Credentials:** none (ad-hoc signing only).

* Product naming decision (app name, bundle id, cask/winget/deb package names) recorded in DECISIONS_4 first.
* The shared settings window (one implementation, toolkit-minimal): the iOS sheet's rows, CC-map editor, MIDI port list + rescan status, dip button. `--dev` gates every debug binding/tool; About = version + commit; first-run hint.
* macOS `.app` bundle (Info.plist, icons from `tools/gen_icons.py`, hardened-runtime entitlements); the runtime Dock-tile hack retires.
* Windows/Linux compile of the shared UI is CI-verified here but NOT exercised — their lane steps own that.

**DONE when:** on macOS the app launches to a playable instrument with zero debug keys active, every setting mouse-reachable, `--dev` restores the lab bench; the bundle passes `codesign --verify` ad-hoc; a volunteer who has never seen the repo connects the ROLI and marbles within two minutes using only the first-run hint.

---

## Step 24 — Release orchestration spine (platform-neutral, any machine)
**Spec:** PHASE5 §3. **Credentials:** none (dry-run only).

* The tag-triggered workflow's spine: version injection from the tag, the pre-package gates (ABI/C11 + hostmpe suites, per-backend §4.6 field regressions), CHANGELOG→release-notes generation, artifact upload scaffolding, and a `--dry-run` dispatch mode. **Contains zero platform lanes** — the CI lane steps (25 web, 27 macOS, 29 Windows, 30 Linux) each add exactly one job to this spine; iOS and Android (28, 31) are deliberately MANUAL procedures with no lane job, only PR compile checks.
* A deliberately broken field-regression fixture must block packaging (the gate proven red before it is trusted green).

**DONE when:** dry-run on a test tag runs gates on all runners, produces a versioned notes draft, uploads nothing; the broken-fixture test blocks the pipeline; the lane interface (job template + artifact naming contract) is documented in the workflow file for the CI lanes (25, 27, 29, 30).

---

## Step 25 — WebGPU backend & marble web (desktop machine)
**Spec:** PHASE5 §5 (whole section); PROJECT_SPEC §4.6 (web tier joins the field regression).

* Core: WGSL dialect in the shdc pipeline; `swapchain_webgpu.cpp` (SOKOL_IMPL host); Emscripten target with the C-ABI as wasm exports. Shell: JS host page (device/swapchain, RAF loop, resize, pointer/touch → marble gestures, pen pointer-events → wake with pressure where reported), WebMIDI input on Chrome/Edge with silent gestures-only degradation elsewhere.
* Scene/embed API per spec (`?scene=…&param=…`, minimal-chrome embed) covering every operator with slider-mapped formula symbols; Pages deploy joins the Step-24 spine as the web lane (the first lane — it must exist before the docs step publishes).

**DONE when:** the web tier passes §4.6 within its documented tolerance; touch-playable on iPad Safari, mouse-playable on desktop Chrome, the ROLI drives it over WebMIDI in Chrome; every operator scene works via query params; first marble < 5 s on a mid-range phone (Lighthouse evidence); Safari shows gestures-only with no error banner.

---

## Step 26 — Documentation site (any machine; needs Step 25)
**Spec:** PHASE5 §6 (five books + the chart + citations).

* Static site + KaTeX, CI-deployed. User guide; **The Operators** (formula, invariants, ownership rule, live Step-25 embed per page — the wake and swirl pages each carry their one-line Jaffer-lineage note); Architecture; **Performance gallery** (author-recorded videos, captioned device/layout/modes; the **Jaffer tribute performance** — exact piece identified from his video credits, public-domain composition verified, performed by the author on the instrument; optional Turkish-moire scripted scene as a gallery↔operators bridge); Design notes (DECISIONS 1–4, lightly edited) + rendered CHANGELOG.
* **Store-required pages at stable URLs** (PHASE5 §6): privacy policy (the app collects nothing — plus each console's required clauses), support/contact page, and the homepage itself — these URLs are hardcoded by every later lane and store listing, so they are frozen here and recorded in DECISIONS_4.
* The MIDI implementation chart, verified against real byte logs (one session per mode), including what a DAW recording does not capture (the wake invariant, for users).
* Citations page per spec: Lu, Jaffer, Jin, Zhao & Mao (IEEE CG&A 2012); Jaffer, "Oseen Flow in Paint Marbling" (arXiv:1702.02106); Jaffer, "The Lamb–Oseen Vortex and Paint Marbling" (arXiv:1810.04646); his CSAIL marbling pages; Lamb's *Hydrodynamics*; Rankine (1858); the MMA/AMEI MPE specification — details verified against the publications during writing, never from memory.

**DONE when:** every operator demo is the release wasm via the embed API (grep the site: no second implementation); the chart matches the byte logs; the gallery plays with correct captions and the tribute video is in place with its piece identified and its public-domain status verified; all citations resolve; the privacy/support/homepage URLs are live and recorded (lane prerequisite); the site deploys from the same tag as the artifacts. Gallery videos are an author input — the step ships with whatever performances are recorded by then and the gallery accepts additions without a site rebuild.

---

## Step 27 — macOS release lane (macOS machine)
**Spec:** PHASE5 §3. **Credentials:** Developer ID cert, notary API key, tap repo token.

* Lane job: universal (arm64+x86_64) bundle → sign → notarize → staple → DMG → release asset; cask bump PR to the author's tap (version/sha256/URL per the tap's existing conventions; `homepage` = the Step-26 URL).

**DONE when:** a test tag yields a Gatekeeper-clean DMG on a fresh macOS user account and an auto-opened cask PR that `brew install --cask` succeeds from; About shows the tag version.

---

## Step 28 — iOS release procedure (macOS machine, iOS agent) — MANUAL BY DESIGN
**Spec:** PHASE5 §3–4. **Credentials:** the author's existing Xcode signing setup (no CI secrets).

* **No CI lane** — the author archives and uploads by hand. This step's deliverable is the verified procedure: the Xcode project archives cleanly from a tagged checkout with the CI-injected version (Archive → Distribute → TestFlight), documented as an `ios/RELEASING.md` checklist (checkout tag, version sanity, archive, upload, what to click in ASC); store metadata (screenshots from real sessions, both modes described, MPE controllers named; privacy-policy, support and marketing URLs = the Step-26 pages) staged in the repo for the beta wave. PR CI keeps a build-only compile check for iOS so tags never surprise the archive.

**DONE when:** the author has walked the checklist end-to-end once from a test tag: a processing-complete build in TestFlight at the tag version, installed on the author's iPad; the checklist contains every step actually taken (audited against reality, not intention).

---

## Step 29 — Windows release lane (Windows machine)
**Spec:** PHASE5 §2–3. **Credentials:** code-signing cert if available; winget-pkgs fork token.

* Verify + polish the shared settings UI and `--dev` on Windows (this step owns any Windows-only fixes); installer + portable zip; signing-if-cert-present (unsigned still releases; SmartScreen consequence documented in the README, not fought); winget manifest bump via wingetcreate PR per the author's existing manifest conventions.

**DONE when:** a test tag yields an installer that runs on a clean Windows VM, `winget install` succeeds from the bumped manifest, the settings window and WinMM MIDI work, About shows the tag version.

---

## Step 30 — Linux release lane (Linux box)
**Spec:** PHASE5 §2–3. **Credentials:** apt repo GPG key.

* Verify + polish the shared settings UI and `--dev` on Linux (X11 + Wayland); deb + tarball; publish to the author's apt repo (Release/InRelease re-signed) from the lane job. **Flatpak spike, timeboxed to half the session:** manifest + ALSA MIDI through the sandbox on one distro — clean → its own publish hook; not clean → verdict in DECISIONS_4, closed.

**DONE when:** a test tag yields `apt install <name>` working on clean Ubuntu with the ROLI playable over ALSA, the .desktop/icon integration intact (Step-14 work preserved), and the flatpak verdict recorded either way.

---

## Step 31 — Android release procedure (Linux box, Android agent) — MANUAL BY DESIGN
**Spec:** PHASE5 §3–4. **Credentials:** the author's existing keystore in Android Studio (no CI secrets).

* **No CI lane** — the author builds and uploads by hand. Deliverable: the verified procedure — Android Studio builds a signed .aab from a tagged checkout with the CI-injected version (Build → Generate Signed Bundle), documented as an `android/RELEASING.md` checklist (checkout tag, version sanity, signed bundle, Play Console internal-track upload); store metadata staged like iOS's (privacy-policy and support URLs = the Step-26 pages). PR CI keeps a build-only compile check for Android. The closed-testing wave is Step 32.

**DONE when:** the author has walked the checklist end-to-end once from a test tag: a build live on the Play internal track at the tag version, installed on the author's tablet with USB-MIDI to the Linux box working; the checklist audited against what was actually done.

---

## Step 32 — Store beta wave (both stores; human-heavy; needs Steps 26–31)
**Spec:** PHASE5 §4.

* TestFlight external group + Play closed track opened on the current tagged builds; the docs site (live since Step 26) is the linked user guide and privacy/support source from day one. Feedback instrument live (the three questions); crashes triaged from the store consoles into issues. Runs ≥ 2 weeks wall-clock.

**DONE when:** one full wave completed; every piece of feedback triaged into the Step-33 backlog with a class (crash / bug / UX-feel / docs); docs-class items ship to the site immediately (the guide is measured, not backlogged); ≥ 5 external testers actually played (store metrics). Fixing app-side items is NOT this step — it is Step 33's whole job.

---

## Step 33 — Feedback incorporation & release candidates (iterative; any machine per item)
**Spec:** PHASE5 §4. The only step allowed to loop.

* **Scoped unfreeze:** bugfix-scoped changes are permitted ANYWHERE, core included — under the bug → regression-test → fix pattern (every fix lands with the test that would have caught it), and every core touch keeps the full suites green (§4.6 field regressions per backend, hostmpe, ABI). Feature work remains frozen; UX-feel items take the smallest change that addresses the report and need the author's sign-off (taste is not delegable).
* Triage classes from Step 32: crashes (must fix), bugs (fix, or waive with rationale in DECISIONS_4), UX-feel (smallest change + author sign-off, or waive), docs (already shipped in 32).
* **Each fix batch → a release-candidate tag** → the CI lanes fire and the author walks the mobile RELEASING checklists → the RC lands on TestFlight/Play test tracks → **the original reporters confirm** ("does this fix it for you?" via the feedback thread). Substantial batches may warrant a one-week mini-wave; the author bounds the iteration.
* The step ends by nominating a **final RC**: the exact build 1.0 will be.

**DONE when:** zero open must-fix items (every crash and bug fixed-with-test or waived in DECISIONS_4); every fixed report is reporter-confirmed or confirmed-unreachable; all suites green on the final RC across all platforms; the final RC has sat on both test tracks for ≥ 3 days with no new must-fix reports; DECISIONS_4 records the RC → 1.0 nomination.

---

## Step 34 — 1.0 release
**Spec:** PHASE5 §1, §4.

* **1.0 is the promotion of Step 33's final RC — zero code changes between the RC and this tag** (the tag moves; the build is the one the testers confirmed). Tag `v1.0.0`: the spine + CI lanes fire (desktop three + web); channels bump; docs + web deploy from the same tag. Human actions in order: walk the iOS and Android RELEASING checklists from the tag (or promote the RC builds already on the test tracks where the consoles allow), verify the five installables, promote TestFlight → App Store review and Play closed → production, publish the GitHub Release, send Professor Jaffer the note with the link (and the tribute video).

**DONE when:** all five platforms install 1.0.0 through their normal channels; docs + marble web are live at the release tag; both store listings are public or in review with nothing pending on the author's side; the changelog's 1.0 section is the notes everywhere; the note to Professor Jaffer is sent.

---

## Deferred (explicitly out of Phase 5 scope — do not implement)
Play mode on the web (WebMIDI output + wasm hostmpe), localization, auto-update, telemetry, Microsoft Store, Flatpak beyond the spike verdict, a **prebuilt libsumi SDK** (dylib/.dll/.so + `sumi_core.h` as a versioned release artifact — deliberately post-Phase-6: publishing binaries makes the ABI a public contract, and the probe-state redesign is a scheduled break; prerequisites when the time comes: settled ABI, an embedding license decision, semver + deprecation policy, the Architecture book as its manual; note the shaders are baked in via sokol-shdc headers, so no separate metallib/shader artifacts are ever shipped), and all Phase 6 instrument layouts (trumpet valves, trombone slide, Wicki–Hayden, fretboard, theremin) — with the standing architectural note: valve/slide layouts are STATEFUL, and the instance-free probe needs a designed state input before any of them are attempted.

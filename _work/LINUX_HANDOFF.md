# Handoff — Linux box: Steps 30 (Linux release lane) and 31 (Android release procedure)

Written on the macOS machine after Steps 23–28. You are the agent on the
author's Linux box (Ubuntu 25.10, the machine that is also the DAW end of the
tablet transports). Step 29 (Windows lane) belongs to the **Windows machine**
— do not touch it, nor the macOS lane, nor `web`. One platform per step.

## Read first, in this order

1. `CLAUDE.md` — the working rules. Two that bite: **agents never commit**
   (prepare evidence, report "tree ready"); **agents never edit specs or
   roadmaps** — new decisions go to `_work/DECISIONS_4.md` as numbered
   entries (**next number: 39**), and where a spec/roadmap line and reality
   disagree you flag it in the entry.
2. `_work/PHASE5_SPEC.md` §2–§4 and `_work/ROADMAP_4.md` Steps 30–31 (the
   DONE lines are the contract).
3. `_work/DECISIONS_4.md` #1–#38 — especially #3 (version from the tag),
   #10–#14 (the spine), #22 (frozen URLs), #31–#36 (the macOS lane, the
   cask PR flow, gate tiers, the Pages environment rule), #37–#38 (iOS
   procedure — Step 31 mirrors it).
4. `.github/workflows/release.yml` — read the **LANE INTERFACE** block at the
   top before adding the `linux` job; `publish-cask.yml` for the
   `release: published` channel-workflow shape.
5. `docs/evidence/step27/SUMMARY.md` and `step28/SUMMARY.md` — the shape
   your evidence must take; `docs/evidence/step26/` for how the site is
   built (you will touch its install page).

## State of the world

* Tags `v0.5.0-rc.1…rc.3` exist; the spine is green on all three runners:
  `version → gates(metal, d3d11, gl) → web + macos → publish` (draft
  pre-release). The `web` job deploys the docs site + `/marble/` to Pages at
  **`https://midi-sink.vibetuned.com/`** from the tag; the `github-pages`
  environment allows `main` and `v*` tags (#36). `publish` needs every lane:
  **add `linux` to its `needs`** when you add the job.
* Gate tiers (#35): the Linux runner renders on Mesa llvmpipe and is judged
  at 2.5e-2 / 1e-3 against the Apple fixture; on your box with a real GPU
  `tools/field_gate.py`'s defaults (1e-2 / 1e-4) hold bitwise. Do not
  loosen anything; if a run is red, read the gate report artifact first.
* Versions: `SUMI_APP_VERSION` from the tag (`-DSUMI_APP_VERSION=…`); About
  and `--version` print it; no number is hand-edited anywhere.
  Release-candidate tags are pre-releases; the human publishes drafts.
* `docs/CHANGELOG.md` has a `## v0.5.0` section (release notes come from it,
  `--strict` on real tags). Do not add sections; the author folds evidence.
* Organization secrets (names the sibling repos battuta / midi-stroke use;
  the author confirms they exist for this repo too): `APT_GPG_PRIVATE_KEY`
  (armored private key for the apt repo), `TAP_PUSH_TOKEN`, `APPLE_*`.
  Nothing else is provisioned; no step creates accounts.
* Tools: `gh` is authenticated on the Mac, not necessarily on the box
  (`gh auth login` if you need run logs; the public API shows job/step
  conclusions without auth).

## Step 30 — Linux release lane

**Inputs already in the tree.** `packaging/linux/midi-sink.desktop.in` +
hicolor icons; `desktop/CMakeLists.txt` install component
`desktop-integration` (Step 14: the `.desktop` `Exec`/`TryExec` are written
ABSOLUTE at install time because GIO drops entries whose binary is not in
PATH — preserve that; for a `.deb` the absolute path is `/usr/bin/midi-sink`).
The GL harness owns its context (§5.1); the settings window is a second GLFW
window with its own GL 3.2 context (#2) — on Wayland it makes its own
context current and restores the main one before `sumi_update`.

**Deliverables.**

1. **Verify + polish** the settings window and `--dev` on X11 and Wayland
   (GLFW picks Wayland when `WAYLAND_DISPLAY` is set). Any Linux-only fix is
   this step's; the core stays frozen (Step 33 is the scoped unfreeze).
2. **`linux` job in `release.yml`**, per the LANE INTERFACE: `needs:
   [version, gates]`, build with the injected version, artifact **`dist-linux`**
   containing `midi-sink_<version>_amd64.deb` and
   `midi-sink-<version>-linux-x64.tar.gz`; uploads guarded by `dry_run`.
   Consider building on `ubuntu-22.04` for glibc reach (the siblings do) —
   your call, record it. Debian versions cannot contain `-`: an RC tag
   `0.5.0-rc.2` must become `0.5.0~rc.2` in the package (tilde sorts before
   the release) while the file/asset name keeps the interface's
   `midi-sink_<version>_amd64.deb` form — decide and record. CPack DEB from
   the existing install component is the obvious route.
3. **apt repository = the docs site's `/apt/`**, signed with
   `APT_GPG_PRIVATE_KEY` — the siblings' pattern (see
   `../battuta/.github/workflows/pages.yml`, step "apt repository": `gh
   release download … '*_amd64.deb'`, `apt-ftparchive packages/release`,
   `gpg --detach-sign` → `Release.gpg`, `--clearsign` → `InRelease`, export
   the public key as `midi-sink.asc`). **Constraint specific to this repo:**
   the Pages tree is built by the release `web` job AT TAG TIME, when the
   release is still a draft with no public asset URLs, so the apt repo
   cannot be composed there. Author a **`publish-apt.yml` on
   `release: published`** that rebuilds the Pages tree and redeploys:
   checkout the tag → `site/` `npm ci && npm run build` with `SITE_VERSION`
   = the tag → download the `midi-sink-<v>-web.tar.gz` asset and unpack it
   under `pages/marble/` (no emsdk needed) → download the deb, build
   `pages/apt/` as above → `upload-pages-artifact` + `deploy-pages` (the
   environment already allows tag refs). Skip cleanly when the secret is
   absent, like the siblings. Then add the apt instructions to
   `site/src/content/docs/guide/install.md` (Windows/Linux paragraph) and
   the README: keyring under `/etc/apt/keyrings/`, `deb [signed-by=…]
   https://midi-sink.vibetuned.com/apt stable main`.
4. **Flatpak spike, timeboxed to half the session:** a manifest + ALSA MIDI
   through the sandbox on one distro. Clean → its own publish hook; not
   clean → verdict in DECISIONS_4 and close it. Either way the entry states
   what was tried.

**DONE (roadmap):** a test tag yields `apt install midi-sink` working on a
clean Ubuntu with the ROLI playable over ALSA, `.desktop`/icon integration
intact, the flatpak verdict recorded. The ROLI-over-ALSA evidence is a byte
capture (`tests/midi_capture_alsa.cpp`, `tools/midi_asserts.py capture`) plus
a screenshot; "clean Ubuntu" is a fresh VM or container the deb installs into.

## Step 31 — Android release procedure (manual by design)

Mirror Step 28 exactly (`ios/prepare_release.sh`, `ios/RELEASING.md`,
`ios/metadata/`, DECISIONS_4 #37–#38):

1. **Version from the tag.** `android/app/build.gradle.kts` still hard-codes
   `versionCode = 1` / `versionName = "0.2.0"` (lines 16–17) — replace with
   values derived from git: `versionName` = numeric `X.Y.Z` (Play, like
   ASC, wants a plain version string; the RC suffix goes into a
   `BuildConfig` field for About), `versionCode` = commit count on HEAD
   (monotonic, unique per commit — Play requires strictly increasing
   codes), and pass `-DSUMI_APP_VERSION=<describe>` into
   `externalNativeBuild.cmake.arguments` so the core carries it too.
   An `android/prepare_release.sh` that prints the three values (and warns
   on a dirty tree / not on a tag) is the author's one entry point.
2. **About** in the settings sheet: `midi-sink X.Y.Z (versionCode) ·
   describe` + `libsumi a.b.c` — the on-device DONE check.
3. **`android/RELEASING.md`**: checkout tag → prepare → Android Studio →
   Build → Generate Signed Bundle (the author's existing keystore; no CI
   secrets) → Play Console → internal testing track → install on the Galaxy
   Tab → About reads the tag → USB-MIDI to this box works (`amidi -l`, then
   `tests/midi_capture_alsa`) → record the walk.
4. **`android/metadata/`**: listing text (both modes named plainly, MPE
   controllers, S-Pen), **Data safety: no data collected / shared**,
   privacy `https://midi-sink.vibetuned.com/privacy/`, support
   `https://midi-sink.vibetuned.com/support/`, screenshots plan (author input
   from real sessions; Play wants 16:9/9:16 phone + 7"/10" tablet sets).
5. **No CI job.** The roadmap line says "PR CI keeps a build-only compile
   check for Android"; the author decided for iOS that the tablet apps are
   built locally only, and `build.yml`'s comment currently extends that to
   Android (#38). **Confirm with the author** before adding or omitting an
   Android compile job; record the answer.

**DONE (roadmap):** a build live on the Play internal track at the tag
version, installed on the author's tablet with USB-MIDI to this box working;
the checklist audited against what was actually done.

## Evidence and reporting

`docs/evidence/step30/SUMMARY.md` and `step31/SUMMARY.md` in the Step 27/28
shape: what landed, a DONE table (PASS / PARTIAL / YOURS with the file that
proves each cell), "found on the way", "not taken". Keep raw outputs beside
the summary. The author commits; end each step with "tree ready" and the
evidence path for the commit message. Test tags: ask the author to push
`v0.5.0-rc.N`; the spine's `publish` job now needs your `linux` job, so a
missing or red `linux` job blocks the draft — run the workflow with
`dry_run` first (Actions → release → Run workflow).

## Addendum — Step 33, first fix batch (written on the Mac; Android needs YOUR build)

The author opened Step 33 early. This batch landed on the Mac (DECISIONS_4
#49–#52): ABI 0.6.0 (`SUMI_DROP_FEED`, `SUMI_VORTEX_LAMB_OSEEN`), the
Marble-mode **pressure gesture** on every shell, the measured Airwave map,
print-buffer recycling in the core, iOS's two dip buttons. **Android was
written but not compiled here** — verify on the Galaxy Tab:

* `android/app/src/main/java/.../MainActivity.kt` (`SumiSurfaceView`): a
  250 ms long press (`postDelayed`) lays a drop and a `Choreographer` tick
  emits `nativeAddFeed` / `nativeAddSwirl` per frame (hold / push up = feed,
  pull back = swirl; constants in the companion object, same as the other
  shells). Check: no drop on lift after a press, tines still work, two-finger
  gestures cancel the timer, `ACTION_CANCEL` clears the press.
* `NativeBridge.kt` + `android/cpp/sumi_jni.cpp`: `nativeAddFeed(x, y, r)` →
  `sumi_add_drop(…, SUMI_DROP_FEED)`, `nativeAddSwirl(x, y, s, rc)` →
  `sumi_add_vortex(…, SUMI_VORTEX_LAMB_OSEEN)`.
* Regression: the desktop `midi-sink --dev --pressure-test` is the behavioural
  test for the two passes (feed widens one band, swirl rotates by the analytic
  angle, three unread dips all accepted); run it on the Linux box too (GL).
* The on-device suites (`--es hostmpeTests 1`) are unchanged by this batch.

* **v0.7 (#53), also from the Mac:** `sumi_params_t` grew (`wake_profile`,
  `wake_spread`) — rebuild everything native against the new header (the JNI
  lib does through the repo-root CMake). `NativeBridge.nativeSetWakeProfile(
  profile, spread)` exists in `sumi_jni.cpp`; the Android settings sheet has
  NO control for it yet (the iOS sheet has a picker + slider under "Stylus
  wake") — add the same two rows if you have the session, else the default
  (0, the doublet) is what shipped before.

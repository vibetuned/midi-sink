# Evidence — Step 31: Android release procedure (manual by design)

PHASE5 §3–4; ROADMAP_4 Step 31. Decisions: `_work/DECISIONS_4.md` #47–#48.
Machine: the author's Linux box (Android Studio Gradle 8.x / AGP, NDK r27,
the Galaxy Tab S8 Ultra over adb). No core change. No CI job — the author's
answer to the handoff's question was "no Android job, like iOS" (#47, flagged
against the roadmap's compile-check line).

## What landed

* **Version from the tag** in `android/app/build.gradle.kts`: `versionName`
  = numeric `X.Y.Z` of `git describe`, `versionCode` = commit count,
  `BuildConfig.BUILD_DESCRIBE` = the full describe,
  `-DSUMI_APP_VERSION=<describe>` into the CMake arguments (the core carries
  it), `SUMI_APP_VERSION` in the environment overrides. A `printVersion`
  Gradle task echoes the triple.
* **About** in the settings sheet (`SettingsDialog`, after SESSION):
  `midi-sink X.Y.Z (versionCode) · describe` and `libsumi a.b.c · AGPL-3.0 ·
  midi-sink.vibetuned.com` — `libsumi` through the new
  `NativeBridge.nativeCoreVersion()` (JNI → `sumi_version()`).
* **`android/prepare_release.sh`** — derives the triple from git, warns on a
  dirty tree / off-tag checkout, runs Gradle's `printVersion` and exits 1 if
  the two disagree; prints the Android Studio hand-off line.
* **`android/RELEASING.md`** — checkout tag → prepare → Generate Signed
  Bundle (author's upload keystore) → Play Console internal testing →
  verify About on the tablet → USB-MIDI to this box (`amidi -l`,
  `midi_capture_alsa`, `midi_asserts.py capture`) → record.
* **`android/metadata/`** — `listing.md` (both modes named plainly, MPE
  controllers and the S-Pen called out, **Data safety: nothing collected or
  shared**, permissions rationale, privacy `…/privacy/`, support
  `…/support/`, release-notes and tester text) and
  `screenshots/README.md` (Play's phone 16:9/9:16 + 7"/10" tablet sets, six
  slots, feature graphic).
* README Android section: the release-procedure paragraph.
* **Paper dip on the tablet** (author's request mid-step, #48): a CANVAS
  section in the settings sheet with *Paper dip — save the print* (PNG to the
  gallery, `Pictures/midi-sink`, through MediaStore) and *Paper dip — discard
  (fresh sheet, no print)*. JNI `nativeDipForPrint(keep)` drains unread
  prints, dips, and the render thread hands the readback over when it lands
  (`nativeTakePrint`); Kotlin writes the PNG off the UI thread and toasts.

## Verified here

| Check | Result | Evidence |
|---|---|---|
| Gradle derives the version from git | **PASS** — `printVersion`: `versionName=0.5.0`, `versionCode=40`, `describe=0.5.0-rc.4-2-g66a3032-dirty` (a dirty tree, as expected before the commit) | `gradle_version.txt` |
| The tree builds with the wiring (`assembleDebug`) | **PASS** — BUILD SUCCESSFUL; the only warning is a pre-existing Kotlin deprecation | `gradle_build.txt` |
| `prepare_release.sh` | **PASS** — same triple as Gradle, warns `dirty` and `not on a tag`, ends "ready: open android/ in Android Studio → …" | `prepare_release_local.txt` |
| About on the device reads the build | **PASS** — installed on the Galaxy Tab (`versionCode=40 versionName=0.5.0` per `dumpsys package`), settings sheet → ABOUT: `midi-sink 0.5.0 (40) · 0.5.0-rc.4-2-g66a3032-dirty` / `libsumi 0.5.0 · AGPL-3.0 · midi-sink.vibetuned.com` (read back from the accessibility tree; screenshot downscaled) | `android_about_text.txt`, `android_about.jpg` |
| Paper dip — save | **PASS** — after four drops: `[dip] print 1480x924 ready for the gallery` → `Print saved: Pictures/midi-sink/midi-sink-print-20260906-171924.png (1480x924)`; the file exists on the device (1.26 MB), pulled and inspected: the sheet, right way up | `android_dip_logcat.txt`, `android_print.jpg` |
| Paper dip — discard, three times in a row | **PASS** — three `fresh sheet, print 1480x924 discarded` lines, no `refused (both print buffers busy)` — the buffers are freed each time (the iPad's button does not read and stops after two dips: flagged in #48) | `android_dip_logcat.txt` |
| USB-MIDI from the tablet reaches this box | **PASS** — `amidi -l`: `hw:5,0,0 SAMSUNG_Android MIDI 1` (the Step-22 transport, unchanged) | `amidi_l.txt` |

## DONE — the author's walk

The DONE criterion is a build **on the Play internal track at the tag
version**, installed on the tablet, USB-MIDI to this box working, the
checklist audited. Nothing on this box can sign with the upload keystore or
reach Play Console; the parts that could be checked at the tag are:

| Cell | State | Proof |
|---|---|---|
| Tagged checkout carries the version | **PASS** — `v0.5.0-rc.5` (commit `ce03a17`, pushed): from a clean worktree of the tag `prepare_release.sh` prints `describe 0.5.0-rc.5`, `versionName 0.5.0`, `versionCode 41`, `exactly on a tag: yes`, and Gradle agrees | `prepare_release_rc5.txt` |
| Android Studio builds the tree | **PASS** (author) — after the Gradle JVM fix below and the author's AGP upgrade 8.10.0 → 8.13.2 (Gradle stays 8.14), "everything is running" | commit `ce03a17` |
| Signed bundle → Play internal track | **YOURS** — `android/RELEASING.md` §3–§4; record tag, versionCode 41, processing time | — |
| About on the tablet reads the tag | **YOURS** — expect `midi-sink 0.5.0 (41) · 0.5.0-rc.5` / `libsumi 0.5.0` from the Play build (the debug build read `0.5.0 (40) · 0.5.0-rc.4-2-g66a3032-dirty`, see above) | — |
| USB-MIDI to this box from the Play build | **YOURS** — `amidi -l`, `build/tests/midi_capture_alsa --match SAMSUNG`, `tools/midi_asserts.py capture` | — |

Append here what Play showed and every checklist line that had to change.

## Found on the way

* Compose needs `buildFeatures.buildConfig = true` for a custom
  `buildConfigField` (AGP 8 defaults it off) — without it `BuildConfig` has
  no `BUILD_DESCRIBE`.
* `providers.exec` (configuration-cache-safe) is the Gradle way to shell out
  to git; a raw `Runtime.exec` at configuration time breaks the cache.
* The author upgraded AGP 8.10.0 → 8.13.2 from Studio while walking the
  checklist (commit `ce03a17`); Gradle 8.14 and the JDK-21 pin stand.
* Android Studio (build 261, bundled JBR **25**) refused the project:
  "Incompatible Gradle JVM version" — Gradle 8.14 runs on Java 17–24. The CLI
  had been working only because the user-level `~/.gradle/gradle.properties`
  on this box sets `org.gradle.java.home` to a Temurin 21 — Studio does not
  read that. The tree now pins the daemon JVM with Gradle's criteria
  (`gradle/gradle-daemon-jvm.properties`, `toolchainVersion=21`; it takes
  precedence over `org.gradle.java.home`, is honoured by Studio, and resolves
  to any local JDK 21); the untracked `.gradle/config.properties` Studio reads
  for `#GRADLE_LOCAL_JAVA_HOME` was pointed at the JDK 21 as well.
* The tablet was dozing when first captured (a black screencap): the
  checklist says to read About from the sheet with the screen on — the
  accessibility dump is the text-exact check.

## Not taken

* No Android CI job (author's decision; #47).
* No Play Console API / fastlane / Gradle Play Publisher: uploads are human
  (Phase 5 §4), as for iOS.
* No keystore or signing config in the tree or in Gradle.

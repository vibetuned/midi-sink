# Releasing midi-sink for Android — the checklist (Step 31)

Android has **no CI lane by design** (DECISIONS_4 #38, #47): the author builds
the signed bundle in Android Studio and uploads it to Play Console by hand,
from a **tagged checkout** with the version the tag carries. Store promotion
past the test tracks is a human decision (Phase 5 §4). Every line below is a
step actually taken; if reality differs, fix the line.

Prerequisites (once): Android Studio with SDK 36 + NDK r27 (the
`local.properties` of this checkout) **running Gradle on a JDK 21** — Gradle
8.14 / AGP 8.13.2 accept Java 17–24 and current Studio bundles a JBR 25, so
the tree pins the daemon JVM to 21 (`gradle/gradle-daemon-jvm.properties`,
auto-detected from `~/.jdks` or `/usr/lib/jvm`); if Studio still shows
*Incompatible Gradle JVM version*, set Settings → Build, Execution,
Deployment → Build Tools → Gradle → **Gradle JDK** to a 21 (or to *Gradle
Daemon JVM criteria*) and sync again. The author's **upload keystore** (never
in the repository, never in CI), a Play Console app record for
`com.vibetuned.midisink` with an **internal testing** track and at least one
tester (the author's own account), and the Data safety form filled from
`android/metadata/listing.md`.

## 1. Checkout the tag

```sh
git fetch --tags
git checkout v0.5.0-rc.5          # the tag the spine built — never a dirty tree
git status --short                # must print nothing
```

The desktop packages, the web build and the docs for this tag came out of the
same commit; the tablet build must too.

## 2. Version sanity

```sh
android/prepare_release.sh
```

Read its output before continuing:

* `describe (About)` is the tag with **no** `-dirty`, e.g. `0.5.0-rc.5`.
* `versionName (Play)` is the numeric `X.Y.Z` (`0.5.0`); Play cannot take
  `-rc.5` — the RC lives in `BuildConfig.BUILD_DESCRIBE`, shown in About.
* `versionCode (Play)` is the commit count: it grows with every commit, so a
  later RC of the same `X.Y.Z` is a higher code (Play requires strictly
  increasing codes per app, across all tracks).
* `exactly on a tag: yes`.
* The Gradle block prints the same three values — the script exits non-zero
  if they differ. (No prebuilt-archive trap here, unlike iOS: Gradle compiles
  the core from source with `-DSUMI_APP_VERSION` at every build.)

## 3. Signed bundle

1. Android Studio → **File → Open** → the `android/` directory (let Gradle
   sync; the version values are computed at sync time from git).
2. **Build → Generate Signed App Bundle / APK…** → **Android App Bundle** → Next.
3. Key store path: the author's upload keystore; enter the store and key
   passwords; key alias as created. **Do not** create a new key here — a
   different upload key means a Play support request to reset it.
4. Build variant **release** → Create. Studio writes
   `android/app/release/app-release.aab` and shows a *locate* link.
5. Sanity: `unzip -p android/app/release/app-release.aab base/manifest/AndroidManifest.xml`
   is binary XML, so read the version from Gradle instead —
   `cd android && ./gradlew -q :app:printVersion` must still print the Step-2
   values (same checkout, nothing changed).

## 4. Play Console

1. Play Console → midi-sink → **Testing → Internal testing** → **Create new
   release**.
2. Upload `app-release.aab`. Play signs the distributed APKs with the app
   signing key it holds; the upload key is only yours. The release page shows
   `0.5.0 (41)` — versionName (versionCode) — check both against Step 2.
3. Release notes: the tag's section of `docs/CHANGELOG.md`, condensed to the
   user-visible lines (the same text the desktop draft carries).
4. **Save → Review release → Start rollout to Internal testing.** Internal
   testing needs no review; the build is available to testers within minutes.
5. Pre-launch report warnings (no crash on Play's device farm, which has no
   MIDI hardware) are informational; a crash is not.
6. The **closed testing** wave (Phase 5 §4, Step 32) promotes this same
   release from the internal track — not a new upload.

## 5. Verify on the tablet

1. On the Galaxy Tab, join the internal track through the opt-in link (Play
   Console → Internal testing → Testers → *Copy link*), then install/update
   from the Play Store listing.
2. Settings sheet (⚙) → scroll to **About**: reads `midi-sink 0.5.0 (41) ·
   0.5.0-rc.5` and `libsumi 0.5.0`. **This is the DONE check**: the installed
   build is the tag.
3. Settings → **Canvas** → *Paper dip — save the print*: the sheet renews
   and a toast names `Pictures/midi-sink/midi-sink-print-<stamp>.png`; the
   gallery shows it. *Paper dip — discard* renews without a file. Both must
   work three times in a row (the core's two print buffers are freed each
   time).
4. Play mode on the chromatic grid: a finger plays, the S-Pen legatos, the
   strip's sustain pad and the pen button both hold the pedal.
5. **USB-MIDI to the Linux box**: set *Use USB for* to **MIDI** on the tablet
   (or `adb shell svc usb setFunctions midi`), then on the box `amidi -l`
   lists `SAMSUNG_Android MIDI 1`; `build/tests/midi_capture_alsa --match
   SAMSUNG --seconds 20 --out cap.csv` while you play, then
   `python3 tools/midi_asserts.py capture cap.csv --usb SAMSUNG --policy rate`
   — the MCM handshake, centre-bend-before-Note-On and the ≤100 Hz policy
   all assert.

## 6. Record

Append the walk to `docs/evidence/step31/SUMMARY.md`: tag, versionCode, Play
processing time, tablet model and Android version, the About line as read,
the capture's assert summary, and every line above that had to change.

## Store metadata

`android/metadata/listing.md` holds the listing text (both modes named
plainly, the MPE controllers and the S-Pen called out, the frozen
privacy/support/website URLs), the **Data safety** answers (no data
collected or shared) and the content-rating answers; the screenshot plan is
`android/metadata/screenshots/README.md`. Screenshots come from real
sessions on the tablet (`adb exec-out screencap -p > shot.png`, or the
hardware buttons) at the sizes Play asks for.

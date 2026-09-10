# Store submission prep — App Store Connect + Google Play (v1.0.0)

Task: fill the technical (non-copy) fields both consoles ask for, and produce
the screenshot / graphic assets, for the `v1.0.0` submission. No core change;
tree left clean apart from the new metadata files.

## What was produced

| Path | What |
|---|---|
| `ios/metadata/submission-answers.md` | every non-copy ASC field, answered from the tree |
| `android/metadata/submission-answers.md` | every non-copy Play field, answered from the tree |
| `ios/metadata/screenshots/ipad-13/` | 2 shots, 2064×2752 (ASC iPad 13") |
| `ios/metadata/screenshots/iphone-6.9/` | 1 shot, 1320×2868 (ASC iPhone 6.9") |
| `android/metadata/screenshots/tablet/` | 4 shots, 2960×1665 (exactly 16:9) |
| `android/metadata/icon-512.png` | 512×512 RGBA, 411 KB (Play requires 32-bit PNG) |
| `android/metadata/feature-graphic-1024x500.png` | 1024×500, no alpha |
| `*/metadata/screenshots/README.md` | appended: what was captured, from which device, and why |

Scripts kept here: `simtouch.py` (Simulator touch injection),
`marble.py` (adb composition), `crop169.py` (16:10 → 16:9),
`feature.py` (feature graphic).

## How the screenshots were made

Real sessions on real builds of the `v1.0.0` tag — no mock-ups, no retouching.

* **Android** — `v1.0.0 (56)` on the author's Galaxy Tab S8 Ultra
  (SM-X906B, Android 16), driven with `adb shell input` and
  `uiautomator` (via `tools/android_automation/droid.py`) to set palette,
  layout and mode. `input motionevent DOWN`/`UP` holds a finger down so the
  Play-mode joystick ring is on screen while the frame is grabbed.
* **iOS** — `1.0.0 (56)` on the iPad Pro 13-inch (M5) and iPhone 17 Pro Max
  simulators, driven by synthesised `CGEvent` mouse events mapped onto the
  Simulator window.

## Findings worth the author's attention

1. **The app segfaults in the iOS Simulator.** `SUMI_SHDC_SLANG`
   (`cmake/CompileShaders.cmake:59`) lists `metal_macos:metal_ios:…` but not
   `metal_sim`, so the generated `*_shader_desc(sg_backend)` has no
   `SG_BACKEND_METAL_SIMULATOR` case, returns a null desc, and
   `sg_make_shader` → `_sg_shader_desc_defaults` dereferences it inside
   `sumi_renderer_create`. Device builds are unaffected and App Review runs on
   real hardware, so this does not block submission. It is a **cache
   variable**, so a simulator build needs no repo edit — see
   `ios/metadata/screenshots/README.md`. Adding `metal_sim` to the default
   list would make the Simulator work out of the box; that is a core-build
   change and therefore the author's call.

2. **`ios/Info.plist` is tracked but generated.** `xcodegen` rewrites its
   `SumiBuildDescribe` key, so running `ios/prepare_release.sh` (or plain
   `xcodegen`) dirties the tree. The committed value was itself a stale
   generated string, `0.5.0-rc.5-1-g02a04f2-dirty`. Because the tree was dirty,
   the first Android build of this task stamped About
   `midi-sink 1.0.0 (56) · 1.0.0-dirty`. Reverting the file and rebuilding gave
   the correct `midi-sink 1.0.0 (56) · 1.0.0`, which is what the shipped
   screenshot shows. Worth either gitignoring the key or committing a
   placeholder, so a release build cannot silently stamp `-dirty`.

3. **`TARGETED_DEVICE_FAMILY` is `1,2`** (`ios/project.yml`) — iPhone *and*
   iPad — while the spec, the listing copy and the screenshot plan are
   iPad-only. Confirmed with the author as intentional: ASC therefore requires
   an iPhone 6.9" screenshot set, and review will test on an iPhone.

4. **AGPL-3.0 vs the App Store — resolved.** GPL-family terms conflict with
   Apple's Licensed Application End User Agreement (the VLC precedent): store
   terms impose device limits, signing that blocks modified installs, and a
   redistribution ban, which AGPL sections 6 and 10 forbid. The author
   confirmed that all three identities in the history (`osf@qlik.com`,
   `pedrogas_g@hotmail.com`, `pfillastre@vibetuned.com`) are the same person
   and sole copyright holder, so the exception could be granted for the whole
   work. Added as `LICENSE-APPSTORE-EXCEPTION.md` — an **additional permission
   under AGPL-3.0 section 7**, in its own file because the FSF text in
   `LICENSE` may not be modified. It covers store copies only; the source and
   anything built from it stay plain AGPL-3.0, and section 7 lets any
   recipient strip the permission. Referenced from `README.md` and the site's
   `index.mdx`. Packaging was deliberately left alone: `LICENSE` is bundled
   only into the Windows installer/zip and the Linux tarball, none of which
   impose the terms the exception addresses.

   *This is a licensing document, not a technical one — worth a lawyer's read
   before the first store submission.*

5. **`libsumi 0.9.0` next to app `1.0.0` is correct, not a mismatch.**
   `core/src/engine.cpp:89` returns the core ABI version deliberately.
   `ios/RELEASING.md` §6 tells the author to check About reads
   `libsumi <app version>`, which was only true while the two happened to
   coincide at 0.5.0. That line is now misleading and wants a word.

6. **Two fields cannot be answered from the tree** and are the author's
   decision: **EU Digital Services Act trader status** (both stores; declaring
   *non-trader* removes the app from EU storefronts, declaring *trader*
   publishes name/address/phone/email) and the Play **target-audience** age
   band (`listing.md` says 18+).

7. **Play device catalog will look small.** `AndroidManifest.xml` marks
   `android.software.midi` and GLES 3.0 as `required="true"`; combined with
   `arm64-v8a`-only and `minSdk 29`, expect a supported-device count in the
   thousands, not tens of thousands. Intended, but check the number on
   *Reach and devices → Device catalog* before rollout so it is not a surprise.

## Verification

* Every asset's pixel size checked against the current published specs
  (Apple: screenshot-specifications; Google: "Add preview assets", both fetched
  2026-09-10, cited in the two answer sheets).
* Android screenshots and the feature graphic are 24-bit, no alpha
  (`samplesPerPixel: 3`); the Play icon is 32-bit RGBA as required.
* Tablet crops verified at ratio 1.7778 (16:9).
* About on the installed tablet build reads `midi-sink 1.0.0 (56) · 1.0.0` —
  clean, matching the tag.

## Not done

* iPhone Play-mode screenshot (ASC needs only one iPhone shot; the Simulator
  window's bezel padding is not a clean affine, so corner controls were not
  reliably reachable on the phone).
* App Preview videos. Play's promo video is a **YouTube URL** — an existing
  gallery performance can be pasted straight in. ASC needs an uploaded file at
  the slot's own resolution.
* No submission was made and nothing was uploaded to either console — those
  are human actions.

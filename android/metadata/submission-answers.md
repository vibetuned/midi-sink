# Google Play Console — the technical fields, answered

Companion to `listing.md` (which holds the marketing copy and the Data-safety
narrative). This file is the paste-ready answer for every *non-copy* field the
Play Console asks, for the `v1.0.0` release of `com.vibetuned.midisink`.

Facts this sheet is derived from — re-check them if the tree moves:

| Fact | Value | Source |
|---|---|---|
| Application ID | `com.vibetuned.midisink` | `android/app/build.gradle.kts` |
| `versionName` | `1.0.0` | tag `v1.0.0` |
| `versionCode` | `56` (commit count) | `git rev-list --count HEAD` |
| `minSdk` / `targetSdk` / `compileSdk` | 29 / 36 / 36 | `android/app/build.gradle.kts` |
| ABI | `arm64-v8a` only | `ndk { abiFilters }` |
| 16 KB page size | enabled | `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`, DECISIONS_4 #74 |
| Permissions | Bluetooth scan/connect/advertise only | `AndroidManifest.xml` |
| Required features | `android.software.midi`, GLES 3.0 | `AndroidManifest.xml` |
| Advertising ID | not used, not declared | no `AD_ID` permission |
| Minification | off (`isMinifyEnabled = false`) | `build.gradle.kts` |

---

## 1. App content — the declarations, one by one

| Section | Answer |
|---|---|
| **Privacy policy** | `https://midi-sink.vibetuned.com/privacy/` |
| **App access** | *All functionality is available without special access.* No login, no region lock, no promo code. |
| **Ads** | *No, my app does not contain ads.* |
| **Content rating** | IARC questionnaire — see §2. Result: **Everyone / PEGI 3**. |
| **Target audience and content** | Age groups: **18 and over** (per `listing.md`). The app is not child-directed; declaring 13+ or 18+ keeps it out of the Families programme, which it has no reason to enter. |
| **News app** | No. |
| **COVID-19 contact tracing and status apps** | No. |
| **Data safety** | *No data collected, no data shared* — see §3. |
| **Government apps** | No. |
| **Financial features** | *My app doesn't provide any financial features.* |
| **Health apps** | No health-related features. |
| **Advertising ID** | *No* — the app does not use an advertising ID. (Confirmed: no `com.google.android.gms.permission.AD_ID` in the manifest. Declaring "yes" here would fail review, since the permission is absent.) |
| **Photo and video permissions** | n/a — the app declares no `READ_MEDIA_*` or storage permission. A saved print is written through MediaStore, which needs no permission on API 29+. |
| **Device location** | Not used. `BLUETOOTH_SCAN` carries `neverForLocation`, so no location declaration is required. |
| **Foreground service** | None declared. |
| **Restricted permissions** | None requested — nothing to justify. |

## 2. Content rating — IARC questionnaire

Category: **Utility, Productivity, Communication, or Other** (a visualizer /
instrument, not a game).

| Question | Answer |
|---|---|
| Violence, blood, sexuality, nudity, crude humour, hate speech | No to all |
| Drugs, alcohol, tobacco | No |
| Gambling or simulated gambling | No |
| Fear or horror content | No |
| **Does the app allow users to interact or exchange content with each other?** | **No** — MIDI goes to the user's own DAW/synth; there is no server, no account, no other user |
| Does the app share the user's location with other users? | No |
| Does the app allow purchase of digital goods? | No |
| Does the app contain, or allow access to, unrestricted internet content? | **No** — no browser, no web view |
| Does the app collect or share personal information? | No |

Expected result: **Everyone (ESRB) / PEGI 3 / USK 0 / Everyone (IARC generic)**.

## 3. Data safety — the form's own answers

> *Does your app collect or share any of the required user data types?* → **No**

That single answer closes the form. The reasoning (paste into any follow-up):
the app has no account, no analytics, no crash reporting and no network of its
own. MIDI over USB, BLE and the on-device virtual device is local I/O. Byte
logs and the session CSV stay in app-private storage and are read only by the
user over adb. A print the user chooses to save is a PNG written to their own
gallery (`Pictures/midi-sink`, MediaStore). Nothing is transmitted to us.

Follow-ups that therefore do not apply: *encrypted in transit* (n/a), *deletion
request mechanism* (n/a), *data types* (none).

## 4. Store settings

| Field | Answer |
|---|---|
| App category | **Music & Audio** |
| Tags | MIDI · Music production (max 5; these two are the accurate ones) |
| Store listing contact — email | `info@vibetuned.com` |
| Store listing contact — website | `https://midi-sink.vibetuned.com/` |
| Store listing contact — phone | optional; author supplies or leaves blank |
| External marketing | opt in or out; no effect on review |

## 5. EU Digital Services Act — trader status

**Author decision, not derivable from the tree.** Play requires a trader /
non-trader declaration for EU distribution, and a *trader* declaration
publishes name, address, phone and email on the EU listing. Answer it the same
way as App Store Connect (`ios/metadata/submission-answers.md` §8) so the two
stores agree.

## 6. Device availability — a consequence worth knowing

`AndroidManifest.xml` marks two features **required**:

```xml
<uses-feature android:name="android.software.midi" android:required="true" />
<uses-feature android:glEsVersion="0x00030000" android:required="true" />
```

Play filters the catalog on both. `android.software.midi` is not universal —
a device only reports it if the OEM shipped the MIDI stack — so the Play
Console's device-catalog page will show a reduced supported-device count. That
is intended (the app is useless without MIDI), but check the number on the
**Reach and devices → Device catalog** page before rollout so it is not a
surprise. Combined with `arm64-v8a`-only and `minSdk 29`, expect a catalog in
the thousands of models rather than the tens of thousands.

## 7. Release technical facts

| Field | Answer |
|---|---|
| App signing | **Play App Signing** — upload with the author's upload key; Play holds the app signing key |
| Bundle | `android/app/release/app-release.aab` from `Build → Generate Signed App Bundle` |
| Release name | `1.0.0 (56)` |
| Target API level | 36 — meets the current Play requirement |
| 16 KB page size | satisfied (DECISIONS_4 #74); Play's bundle check will not warn |
| Pre-launch report | warnings from Play's device farm having no MIDI hardware are informational; a **crash** is not |

## 8. Preview assets — what is actually required

| Asset | Spec | Count |
|---|---|---|
| App icon | 512×512, **32-bit PNG with alpha**, ≤1024 KB | 1, required |
| Feature graphic | 1024×500, JPEG or **24-bit PNG, no alpha** | 1, required |
| Phone screenshots | 16:9 or 9:16; min side ≥320 px, max side ≤3840 px, max ≤2× min | **min 2**, max 8 |
| 7" tablet screenshots | 16:9 or 9:16; 1080–7680 px | min 4 recommended for large-screen quality, max 8 |
| 10" tablet screenshots | same | same |
| Promo video | **a YouTube URL** — public or unlisted, embeddable, not age-restricted, ads disabled | 1, optional |

The promo-video field is a link, not an upload: an existing YouTube
performance from the gallery can be used directly.

See `screenshots/README.md` for the capture plan and the exact crop that turns
a Galaxy Tab S8 Ultra capture into a compliant 16:9 image.

## 9. Licensing — the store exception

midi-sink is AGPL-3.0. Play's Developer Distribution Agreement is far less
troublesome than Apple's — Google does not impose the device limits and
anti-modification signing that put GPL-family licences in conflict with the
App Store — so Play alone would probably not need an exception.

The repository nevertheless carries `LICENSE-APPSTORE-EXCEPTION.md`, an
**additional permission under AGPL-3.0 section 7** granted by the sole
copyright holder. It is written for "an application store or comparable
curated distribution platform", so it covers Play as well as the App Store and
the two stores stay on identical terms. Store copies only; the published
source stays plain AGPL-3.0.

Nothing needs to be entered in the Play Console for this — there is no licence
field. See `ios/metadata/submission-answers.md` §11 for the full reasoning.

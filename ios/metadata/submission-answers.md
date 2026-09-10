# App Store Connect — the technical fields, answered

Companion to `listing.md` (which holds the marketing copy). This file is the
paste-ready answer for every *non-copy* field App Store Connect asks, for the
`v1.0.0` submission of `com.vibetuned.midi-sink`.

Facts this sheet is derived from — re-check them if the tree moves:

| Fact | Value | Source |
|---|---|---|
| Bundle ID | `com.vibetuned.midi-sink` | `ios/project.yml` |
| Team | `47D2CT68R2` | `ios/project.yml` |
| Device family | `1,2` — **iPhone and iPad** | `ios/project.yml` |
| Deployment target | iOS 16.0 | `ios/project.yml` |
| Marketing version | `1.0.0` | tag `v1.0.0` |
| Build number | `56` (commit count) | `git rev-list --count HEAD` |
| Encryption | `ITSAppUsesNonExemptEncryption = false` | `ios/Info.plist` |
| Permissions used | Bluetooth, Local Network, Photo-library *add* | `ios/Info.plist` |
| Background mode | `audio` (required by `MIDISourceCreate`) | DECISIONS_3 #24 |

---

## 1. App Information (app-level, all versions)

| Field | Answer |
|---|---|
| Name | `midi-sink` |
| Subtitle | `Ink marbling, played with MIDI` |
| Primary language | English (U.S.) |
| Bundle ID | `com.vibetuned.midi-sink` |
| SKU | `midi-sink-ios` |
| Primary category | Music |
| Secondary category | Graphics & Design |
| Content rights | **Does your app contain, show, or access third-party content?** → **No.** The app renders only what the user plays; it ships no licensed media, no web view, no catalog. The app is the author's own AGPL-3.0 work — see §11 on the store exception. |
| Privacy Policy URL | `https://midi-sink.vibetuned.com/privacy/` |
| Localizations | English (U.S.) only for 1.0.0 |

## 2. Age Rating questionnaire → **4+**

Every content question is **None** / **No**:

| Question | Answer |
|---|---|
| Cartoon or Fantasy Violence | None |
| Realistic Violence | None |
| Prolonged Realistic or Sadistic Violence | None |
| Sexual Content or Nudity | None |
| Profanity or Crude Humor | None |
| Alcohol, Tobacco, or Drug Use or References | None |
| Mature or Suggestive Themes | None |
| Horror or Fear Themes | None |
| Medical or Treatment Information | None |
| Gambling (simulated or real) | No |
| Contests | No |
| **Unrestricted web access** | **No** — the app has no web view and makes no HTTP requests |
| In-app messaging / chat between users | No |
| User-generated content shared with others | No — a print is saved to the user's own Photos, never uploaded |
| Age assurance / age verification used | No |

Result: **4+**, no advisories.

## 3. App Privacy → **Data Not Collected**

> *Do you or your third-party partners collect any data from this app?*
> → **No, we do not collect data from this app.**

Justification if review asks: the binary has no analytics SDK, no crash
reporter, no account, and no outbound network of its own. The only network
API is CoreMIDI's RTP-MIDI session (peer-to-peer on the local link) and the
only file written outside the sandbox is a print the user explicitly saves to
Photos. Nothing leaves the device to us.

Privacy Choices URL: leave blank.

## 4. Export compliance

Already answered in the binary — `ITSAppUsesNonExemptEncryption = false` in
`ios/Info.plist` — so **no prompt appears on upload**. If a form ever asks:

| Question | Answer |
|---|---|
| Is your app designed to use cryptography or does it contain or incorporate cryptography? | **No** |
| France declaration | n/a (follows from the above) |

The app implements no cryptography and opens no TLS connection of its own.

## 5. Pricing and Availability

| Field | Answer |
|---|---|
| Price | **Free** |
| Availability | All countries and regions |
| Pre-orders | No |
| Distribution on Apple Vision Pro | **No** — untick "Make this app available" (`SUPPORTS_MACCATALYST: NO`, and the shell is a touch/Pencil app with no visionOS layout) |
| Distribution on Mac (Designed for iPad) | **No** — same reason |

## 6. Version 1.0.0 page

| Field | Answer |
|---|---|
| Version | `1.0.0` |
| Build | `56` |
| Copyright | `2026 Pedro Fillastre` |
| Routing App Coverage File | n/a |
| Version release | Manually release this version (you decide when 1.0.0 goes live) |
| Phased release for automatic updates | n/a on a first release |

## 7. App Review Information

| Field | Answer |
|---|---|
| Sign-in required | **No** (leave unticked) |
| Demo account | n/a |
| Contact | Pedro Fillastre · `info@vibetuned.com` · (phone number — author supplies) |
| Attachment | optional; a short screen recording of Play mode driving a synth app shortens review |

**Notes** (paste as-is):

> The app needs no account, and no hardware to review.
>
> Marble mode is the visualizer. A connected MIDI instrument plays into the
> water — an MPE controller's strike, pressure, glide and slide become drop,
> feed, comb and hue; a wind controller draws one calligraphic line; any
> keyboard plays drops on the circle of fifths. Touch and the Apple Pencil
> drive that same water directly (tap = drop, drag = comb, two-finger twist =
> vortex, pinch = fold), so the mode can be exercised in full with nothing
> plugged in.
>
> Play mode turns the iPad into an MPE instrument and sends MIDI *out* to
> other apps and devices, so to see its output you need a MIDI destination:
> open any synth app on the same device (midi-sink publishes a virtual
> CoreMIDI source named "midi-sink"), or connect an MPE controller.
>
> Bluetooth is used only to pair Bluetooth MIDI instruments and to advertise
> the play surface as a BLE-MIDI device; local network only for the CoreMIDI
> network session (RTP-MIDI). The `audio` background mode is required by iOS:
> `MIDISourceCreate` returns `kMIDINotPermitted` (-10844) without it. The app
> plays no audio itself — it is a visualizer and a MIDI controller.

## 8. EU Digital Services Act — trader status

**Author decision, not derivable from the tree.** Apple requires every
developer distributing in the EU to declare trader status; a *non-trader*
declaration removes the app from all EU storefronts. Declaring **trader**
publishes your name, address, phone and email on the EU listing pages.
Decide before submitting, then fill Business → Trader Status in ASC.

## 9. Screenshots — what is actually required

Because device family is `1,2`, ASC requires **both** sets:

| Slot | Required | Accepted pixel sizes (landscape) | Count |
|---|---|---|---|
| iPhone 6.9" | yes | `2868×1320`, `2796×1290`, or `2736×1260` | 1–10 |
| iPad 13" | yes | `2752×2064` or `2732×2048` | 1–10 |

No alpha channel; `.png`, `.jpg`, `.jpeg`. Smaller display sizes are scaled
from these automatically — no other set needs uploading.

The bare minimum to submit is **one image in each slot**. See
`screenshots/README.md` for the capture plan and the device to capture on.

## 10. App preview (video) — optional

If a preview is uploaded it must be captured at the same display size as its
screenshot slot, 15–30 s, `.mov`/`.mp4`/`.m4v`, and must show only in-app
footage. A YouTube link is **not** accepted (that is a Play-only field) — the
file is uploaded to ASC directly.

## 11. Licensing — the App Store exception

midi-sink is AGPL-3.0. GPL-family terms and Apple's Licensed Application End
User Agreement conflict: the store imposes device limits, signing that blocks
installing a modified build, and a redistribution ban, which AGPL sections 6
and 10 forbid a licensor from accepting. Apps have been pulled over exactly
this (VLC, 2011).

Resolved by `LICENSE-APPSTORE-EXCEPTION.md` in the repository root — an
**additional permission under AGPL-3.0 section 7**, granted by the sole
copyright holder, permitting conveyance through an application store under
that store's terms. It is a separate file because the FSF licence text in
`LICENSE` may not be modified. It applies only to store copies; the published
source stays plain AGPL-3.0.

Nothing needs to be entered in App Store Connect for this — there is no
licence field. It matters only if review questions the licence, in which case
point at that file. Have a lawyer read it before the first submission.

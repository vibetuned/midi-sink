# Releasing midi-sink for iPadOS — the checklist (Step 28)

iOS has **no CI lane by design**: the author archives in Xcode and uploads to
App Store Connect by hand, from a **tagged checkout** with the version the tag
carries. Store promotion past TestFlight is a human decision (Phase 5 §4).
Every line below is a step actually taken; if reality differs, fix the line.

Prerequisites (once): Xcode with the team `47D2CT68R2` signed in
(Settings → Accounts), the App Store Connect record `com.vibetuned.midi-sink`,
`brew install ninja xcodegen`, a TestFlight **external** group created in ASC.

## 1. Checkout the tag

```sh
git fetch --tags
git checkout v0.5.0-rc.2          # the tag the spine built — never a dirty tree
git status --short                # must print nothing
```

The desktop DMG and the web/docs for this tag came out of the same commit;
the iPad build must too.

## 2. Version sanity + core rebuild + project generation

```sh
ios/prepare_release.sh
```

Read its output before continuing:

* `describe (About)` is the tag with **no** `-dirty`, e.g. `0.5.0-rc.2`.
* `MARKETING_VERSION` is the numeric `X.Y.Z` (`0.5.0`); App Store Connect
  cannot take `-rc.2` — the RC lives in the build number and in About.
* `CURRENT_PROJECT_VERSION` is the commit count: it grows with every commit,
  so a later RC of the same `X.Y.Z` is a higher build number (ASC requires
  strictly increasing build numbers per version).
* `libsumi.a` / `libhostmpe.a` timestamps are **now** — the app links the
  prebuilt archives; a stale core has shipped to the iPad before.
* The three `Info.plist` lines echo the values back.

## 3. Archive

1. `open ios/midi-sink-ios.xcodeproj`
2. Scheme **midi-sink**, destination **Any iOS Device (arm64)**.
3. Signing & Capabilities: team `47D2CT68R2`, *Automatically manage signing*
   ticked, bundle id `com.vibetuned.midi-sink`, Background Modes → Audio
   present (the virtual MIDI source needs it, DECISIONS_3 #24).
4. **Product → Archive**. The Organizer opens on the new archive; its
   Version/Build must read `0.5.0 (NNN)`.

## 4. Upload

1. Organizer → **Distribute App** → **App Store Connect** → **Upload**.
2. Leave *Upload your app's symbols* and *Manage version and build number*
   at Xcode's defaults **except**: untick *Manage version and build number*
   (the tag owns them; Xcode must not renumber).
3. Automatically manage signing → Upload. Wait for "Upload Successful".
4. No export-compliance question appears: `ITSAppUsesNonExemptEncryption`
   is `false` in the plist.

## 5. App Store Connect

1. ASC → My Apps → midi-sink → **TestFlight** → iOS Builds. The build
   appears within minutes as *Processing*, then *Ready to Submit* / *Ready
   to Test* (10–30 min; an email confirms).
2. Build → **Test Information**: what to test (paste the tag's section from
   `docs/CHANGELOG.md`, or the TestFlight notes in `ios/metadata/`).
3. **Internal testing**: add the build to the internal group → installable at
   once on the author's iPad through the TestFlight app.
4. **External testing** (the beta wave, Step 32): add the build to the
   external group → *Submit for Review* — the first external build of a
   version goes through Beta App Review (typically a day). Test Information
   must carry the beta description, feedback e-mail (`info@vibetuned.com`),
   privacy URL `https://midi-sink.vibetuned.com/privacy/`.

## 6. Verify on the iPad

1. TestFlight app → midi-sink → Install (or Update).
2. Settings sheet → **About** reads `midi-sink 0.5.0 (NNN) · 0.5.0-rc.2` and
   `libsumi 0.5.0`. This is the DONE check: the installed build is the tag.
3. Marble mode paints; Play mode → the strip appears, a finger plays, the
   Pencil legatos; Settings → Outbound → virtual source visible to another
   app (e.g. a synth on the iPad or a Mac over IDAM).

## 7. Record

Append the walk to `docs/evidence/step28/SUMMARY.md`: tag, build number,
ASC processing time, TestFlight install on which iPad, any line above that
had to change.

## Store metadata

`ios/metadata/` holds the listing text (both modes named plainly, the MPE
controllers called out, the frozen privacy/support/marketing URLs) and the
screenshot plan. Screenshots come from real sessions — capture them on the
iPad (Settings → Evidence for full-screen bursts, or the hardware buttons)
at the sizes ASC asks for (see `ios/metadata/screenshots/README.md`).

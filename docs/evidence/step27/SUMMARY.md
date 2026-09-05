# Evidence — Step 27: macOS release lane

PHASE5 §3; ROADMAP_4 Step 27. Decisions: `_work/DECISIONS_4.md` #31–#34.
Machine: the author's Mac (Apple silicon, Xcode CLT; no Developer ID on
this machine — the credentials are organization secrets, exercised only by
the runner). No core change.

## What landed

* **`packaging/macos/release.sh`** — the whole release path as one
  environment-driven script, identical locally and in CI: verify universal →
  `codesign` (hardened runtime, secure timestamp when a real identity) →
  notarize the app → **staple the app** → DMG around the stapled app
  (`hdiutil` UDZO + `/Applications` symlink) → sign the DMG → notarize the DMG
  → staple the DMG → `spctl` assess both → sha256. Without credentials it
  yields an ad-hoc DMG and says so; `REQUIRE_NOTARIZATION=1` makes the
  absence of a notary an error. Apple-ID and App-Store-Connect-key notary
  credentials both accepted (#32).
* **`macos` job in `release.yml`** — `needs: [version, gates]`, throwaway
  keychain import of the Developer ID (.p12 from `APPLE_CERTIFICATE`),
  universal configure (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, deployment
  12.0, `SUMI_CODESIGN_IDENTITY` = the Developer ID), build, `--version`
  check against the tag, the script, `dist-macos` with
  `midi-sink-<version>-macos-universal.dmg` (+ `.sha256`). `publish` now
  needs `macos`. Dry runs sign but skip the notary round trip; real tags with
  a certificate require notarization (#31).
* **`publish-cask.yml`** — on `release: published`: hash the universal DMG,
  write `Casks/midi-sink.rb` in the tap's conventions (`#{version}` URL,
  `livecheck :github_latest`, `depends_on macos: ">= :monterey"`, `zap
  trash`, `homepage` = the frozen Step-26 URL), push branch
  `midi-sink-<version>` and **open a PR** on `vibetuned/homebrew-tap`
  (`[pre-release]` in the title for RC tags). Direct-push, the siblings'
  practice, was deliberately not copied: an RC cask must never land on main
  (#33).
* README release section and the site's install page: the cask command and
  the lane description.

## Verified here (no credentials needed)

| Check | Result | Evidence |
|---|---|---|
| The tree builds universal | **PASS** — `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, deployment target 12.0, every FetchContent dependency fat; `lipo`: `x86_64 arm64` | `lipo.txt`, `version_universal.txt` |
| The packaging script produces a valid DMG ad-hoc | **PASS** — ad-hoc signature (`flags=0x10002(adhoc,runtime)`), `codesign --verify --deep --strict` on app and DMG, 3.9 MB UDZO, sha256 written; "notarized: NO" stated | `release_sh_adhoc.txt` |
| The DMG is what a user gets | **PASS** — mounted: `midi-sink.app` + `Applications` symlink; the app inside is universal, runs `--version`, verifies, `LSMinimumSystemVersion 12.0` | `dmg_contents.txt` |
| The script refuses to ship unnotarized when told to | **PASS** — `REQUIRE_NOTARIZATION=1` with no credentials exits 1 (the lane sets this on every real tag that has a certificate) | `release_sh_negative_control.txt` |
| Workflows parse | **PASS** — `release.yml` jobs `version, gates, web, macos, publish`; `publish-cask.yml` on `published` | (js-yaml load) |

## DONE criteria — the parts that need the runner and a human (#34)

Nothing on this machine can sign with the Developer ID or reach Apple's
notary: the DONE gates run on a **release-candidate tag published as a
pre-release**:

1. `git tag v0.5.0-rc.1 && git push origin v0.5.0-rc.1` → the spine runs;
   the `macos` job signs, notarizes twice, staples twice, attaches
   `midi-sink-0.5.0-rc.1-macos-universal.dmg` to a **pre-release draft**
   (the `version` job marks any `-` version pre-release). Check the job log:
   both notarytool submissions `Accepted`, both `stapler validate` OK,
   `spctl --assess` accepted for the DMG and for the mounted app.
2. Publish the draft (keep the pre-release flag) → `publish-cask` opens the
   `[pre-release] midi-sink 0.5.0-rc.1` PR on the tap.
3. **Gatekeeper-clean on a fresh macOS user account:** download the DMG from
   the pre-release, open, drag, launch — no warning; About reads
   `0.5.0-rc.1`.
4. **Cask:** `git clone` the tap, `git checkout midi-sink-0.5.0-rc.1`,
   `brew install --cask ./Casks/midi-sink.rb` → installs and launches;
   `brew uninstall --cask midi-sink` cleans up. Close the PR unmerged.
5. A real tag later (`v1.0.0`) repeats the path; that PR is merged.

Prerequisites the author stages once: the organization secrets
(`APPLE_CERTIFICATE`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_SIGNING_IDENTITY`,
`APPLE_ID`, `APPLE_PASSWORD`, `APPLE_TEAM_ID`, `TAP_PUSH_TOKEN`) visible to
this repository, and `TAP_PUSH_TOKEN` carrying `pull-requests: write` on the
tap (with `contents: write` only, the branch is pushed and the job prints the
compare URL to open the PR by hand).

## Flagged

ROADMAP_4 Step 27 names a "notary API key"; the author's existing secrets are
an Apple ID + app-specific password (what battuta and Midi Stroke use). The
lane consumes the existing secrets (working rule: infrastructure is an
input); the script also accepts an App Store Connect key (#32).

# Evidence — Step 28: iOS release procedure (manual by design)

PHASE5 §3–4; ROADMAP_4 Step 28. Decisions: `_work/DECISIONS_4.md` #37–#38.
Machine: the author's Mac (Xcode 26.6, xcodegen). No core change. No CI job:
iOS is built and released locally only (author's decision, #38 — flagged
against the roadmap's compile-check line).

## What landed

* **Version from the tag** in the Xcode project: `project.yml` takes
  `MARKETING_VERSION` / `CURRENT_PROJECT_VERSION` / `SumiBuildDescribe` from
  the environment; **`ios/prepare_release.sh`** derives them from
  `git describe` (numeric `X.Y.Z` for ASC, commit count as the build number,
  the full describe for About), rebuilds `libsumi.a` / `libhostmpe.a` with
  `SUMI_APP_VERSION` injected (the stale-core trap), regenerates the project,
  and echoes the values for a human to read. `ITSAppUsesNonExemptEncryption
  = false` answers the export-compliance prompt once.
* **About** in the settings sheet: `midi-sink X.Y.Z (build) · describe` and
  `libsumi a.b.c` — the on-device DONE check.
* **`ios/RELEASING.md`** — the checklist: checkout tag → prepare → Archive →
  Distribute/Upload (Xcode's build-number management OFF) → ASC TestFlight
  (internal, then external for the beta wave) → verify About on the iPad →
  record.
* **`ios/metadata/`** — listing text (both modes named plainly, MPE
  controllers called out, privacy/support/marketing = the frozen Step-26
  URLs, App Privacy "Data Not Collected", review notes, TestFlight test
  information) and the screenshot plan (six slots, ASC sizes; author input
  from real sessions).

## Verified here

| Check | Result | Evidence |
|---|---|---|
| The script derives, rebuilds and regenerates | **PASS** on a dirty working tree: describe `0.5.0-rc.2-1-g…-dirty`, marketing `0.5.0`, build `36`, both archives rebuilt (fresh timestamps), project regenerated | `prepare_release_local.txt` |
| An unsigned device build of the regenerated project succeeds and carries the version | **PASS** — `xcodebuild … generic/platform=iOS Release CODE_SIGNING_ALLOWED=NO`: BUILD SUCCEEDED; built `Info.plist`: `CFBundleShortVersionString 0.5.0`, `CFBundleVersion 36`, `SumiBuildDescribe 0.5.0-rc.2-1-g2e006d4-dirty`, `ITSAppUsesNonExemptEncryption false` | `built_app_infoplist_local.txt` |

## DONE — the author's walk (to be appended)

The DONE criterion is a processing-complete TestFlight build at the tag
version installed on the author's iPad, with the checklist audited against
what was actually clicked. From a **clean tagged checkout** (the current
tree is dirty — the Step 28 files themselves need a commit and a tag, e.g.
`v0.5.0-rc.3`, before the walk): `git checkout v0.5.0-rc.3 &&
ios/prepare_release.sh`, then `ios/RELEASING.md` §3–§7. Record here: tag,
build number, ASC processing time, iPad model, and every checklist line that
had to change.

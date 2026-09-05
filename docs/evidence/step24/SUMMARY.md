# Evidence — Step 24: Release orchestration spine (platform-neutral)

PHASE5 §3; ROADMAP_4 Step 24. Decisions: `_work/DECISIONS_4.md` #10–#14.
Authored on the macOS machine; the spine itself is platform-neutral. Zero
platform lanes (by design — the CI lane steps 25, 27, 29, 30 each add one
job; iOS/Android are manual procedures with no lane).

## What landed

* **`.github/workflows/release.yml`** — tag push (`v*`) or manual dispatch
  with `dry_run` (default true) + optional `version`:
  * `version` job: the one source of the version string (tag → `X.Y.Z`,
    dispatch → input or `0.0.0-dry.<run>`), validated as `X.Y.Z[-pre]`;
    release notes generated from `docs/CHANGELOG.md` (`--strict` on real
    tags, draft-marked fallback on dry runs) and uploaded as the
    `release-notes` artifact.
  * `gates` job (matrix macOS/Metal, Windows/D3D11, Ubuntu/GL): configure
    with `-DSUMI_APP_VERSION=<version>`, build, headless suites, **assert
    the binary's `--version` carries the injected version**, then the
    **§4.6 field regression through the real renderer** via
    `tools/field_gate.py` (Xvfb + Mesa llvmpipe on Linux), dump + report
    uploaded as `field-<backend>`.
  * `publish` job (skipped when `dry_run`): merges every lane's `dist-*`
    artifact and creates a **draft** GitHub release with the notes as body
    (prerelease when the version carries `-`). Humans publish; channel bumps
    are later `release: published` workflows, as in battuta / midi-stroke.
  * **Lane interface** documented at the top of the file (#13) for the four
    CI lanes (web, macOS, Windows, Linux): one job, `needs: [version, gates]`,
    uploads guarded by `dry_run`, one `dist-<lane>` artifact,
    `midi-sink-<version>-<platform>[-<variant>].<ext>` naming. iOS and
    Android are manual procedures with no lane job (roadmap revision).
* **`tools/field_gate.py`** — the gate with its own negative control: dump →
  compare vs fixture (must pass) → compare vs a corrupted copy of the fixture
  (must FAIL). Exit codes name the failure class: 1 regression, 3 gate cannot
  go red, 4 no renderer on this machine.
* **`tools/release_notes.py`** — `## vX.Y.Z` section → notes; missing
  section → marked draft (dry run) or hard error (`--strict`, real tag).

## DONE verification

| DONE criterion | Result | Evidence |
|---|---|---|
| Dry run on a test tag runs gates on all runners, produces a versioned notes draft, uploads nothing | **Locally proven, GitHub run pending your push.** The workflow parses (jobs `version → gates(metal,d3d11,gl) → publish`, `publish.if = dry_run != 'true'`); the gate ran green on this Mac's Metal renderer with the negative control rejecting the corrupted fixture; the version job's shell logic was exercised for every trigger case (tag, prerelease tag, bare dispatch → `0.0.0-dry.42`, dispatch with version, malformed tag rejected); a `-DSUMI_APP_VERSION=1.2.3-rc1` build shows `1.2.3-rc1` in `--version` and `1.2.3` in Info.plist; the notes generator drafted `v9.9.9` from the latest section with the DRAFT banner. The three-runner execution needs the workflow on GitHub: push, then *Actions → release → Run workflow (dry_run)* | `version_derivation.txt`, `version_injection.txt`, `field_gate_metal_green.txt`, `notes_v9.9.9_draft.md`, `notes_v0.4.0.md` |
| A deliberately broken field-regression fixture blocks the pipeline | **PASS** — a fixture with +0.3 on a 40×40 block: the gate reports `FAIL (max 3.0e-01 <= 1.0e-02: NO)` and exits 1; `publish` and every lane `needs: gates`, so packaging cannot proceed. And the gate proves it can go red on every green run too (negative control, exit 3 if the comparator ever stopped failing a corrupted fixture) | `field_gate_broken_fixture.txt`, `field_gate_metal_green.txt` |
| Lane interface documented in the workflow file for the CI lanes (25, 27, 29, 30) | **PASS** — the LANE INTERFACE block at the top of `release.yml` (job shape, `needs`, dry-run guard, artifact name + file naming, who creates the release; iOS/Android explicitly excluded as manual) | `.github/workflows/release.yml` |
| Headless suites still green | **PASS** — ctest 4/4 | build log |

## Known risk, flagged (DECISIONS_4 #11)

The D3D11 swapchain creates a hardware device only; a GPU-less Windows
runner may make the D3D11 gate exit 4 ("no renderer on this machine") on the
first dry run. If it does, the WARP fallback belongs to the Windows lane step
(29) as a `swapchain_*` seam change — until then the Windows gate is honestly
red, never quietly skipped. The Linux GL gate runs under Xvfb with Mesa
llvmpipe and is expected green; both are what the dry run exists to find out.

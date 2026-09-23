# Handoff — Windows machine: Step 46 (Phase 6 on the Windows desktop)

Written on the macOS machine, 2026-09-23. You are the agent on the author's
Windows machine (MSVC + Ninja, the D3D11 backend, the ROLI and the Airwave
over WinMM). Do not touch the macOS, iOS, Linux, Android or web lanes.

**Your step:** everything Phase 6 put into the core and the desktop shell has
been built, gated and looked at only on Metal and WebGPU (and, by the time
you read this, on GL through step 45a on the Linux box — read its
`docs/evidence/step45a/SUMMARY.md` first; it will have found any shared
desktop problem before you). Windows runs the same C++ through **D3D11**
(`hlsl5` from sokol-shdc) — none of the Phase-6 shaders, render targets or
the export readback have ever executed on it. Build, gate, look, fix
(bug → regression test → fix), record.

> Step numbering: `_work/ROADMAP_5.md` has no Windows step and names 46 the
> web marble; the author's numbering is **44a iOS, 44b web, 45a Linux, 45b
> Android, 46 Windows**. Evidence in `docs/evidence/step46/`. ROADMAP_5
> puts the phase end on its step 46 (tag `v2.0.0-alpha.1`, fold
> DECISIONS_5 into docs/DECISIONS.md as Part V); in the author's numbering
> that follows this step — **the tag is the author's, and do not fold the
> decision log unless the author asks.**

## Read first, in this order

1. `CLAUDE.md` — **agents never commit, stage or push**; **agents never edit
   specs or roadmaps** (flag conflicts in a decision entry); temporary
   verification hooks are removed from the tree.
2. `_work/DECISIONS_5.md` — the Phase-6 record; new entries continue after
   the last one written (the Linux and Android agents write before you; at
   the time of writing the next free number was **#76**). Most relevant:
   #50–#58 (the Anod composite), #60–#62 (the Chladni cells: an RGBA16F
   index map), #63–#69 (palettes, substrate, presets, prints and the
   ledger, the bloom), #70–#72 (the binding table, the stir's ownership),
   #75 (the medium-aware gestures).
3. `docs/evidence/step43/`, `step43_chladni/`, `step44a/`, `step44b/`,
   `step45a/` — what landed and how it measured.
4. `docs/BUILD.md` — every harness test and gate with its command line.

## What changed since your last verification (Step 33, core 0.9)

* **Core ABI → 1.1.0** (`sumi_version()`): 1.0.0 was the one break
  (`sumi_layout_probe` gained a `state` argument); since then additive — the
  medium, the palette model / library, substrate and glow params, the
  export ABI (`sumi_read_field`, `sumi_export_begin/poll`), the
  medium-aware gestures (`sumi_gesture_*`). Rebuild from scratch.
* **New GPU work, never run on D3D11:** `bloom.glsl` (RGBA16F half-res
  chain), the composite's Anod branch with its `linear_out` / `bloom_in`
  variants and the palette path, the Chladni `cells_fs` pass with its
  RGBA16F index map, the export target read back through
  `swapchain_d3d11.cpp`'s readback slot (the paper-dip print's). sokol-shdc
  binding names are global across generated headers — a collision made
  sokol refuse the composite pass on the Mac (#69); check the D3D11 debug
  layer / validation log first if a pass draws nothing.
* **Desktop shell:** settings sections Medium, Substrate, Palette (library
  + custom editor), Presets, Prints (the ledger, thumbnails as GL textures
  in the ImGui window — the settings window is GL even on Windows), Chladni
  / Burst / Spark / Chirikov; presets and `last_session.json` beside
  `settings.ini` in `%APPDATA%\midi-sink`; ledger PNGs written on a
  background thread (paths with non-ASCII user names are worth one try).

## Checklist

1. **Build + ctest** (MSVC, `cmake -B build -G Ninja && cmake --build build
   && ctest --test-dir build`): five suites including `preset_tests` (the
   C11 serializer under MSVC's `/W4` for the first time).
2. **§4.6 field gate on D3D11:** `tools/field_gate.py … --backend d3d11` —
   on a real GPU D3D11 was **bitwise** with the Metal fixture (Steps 11,
   DECISIONS_4 #35's note); the phase invariant is bitwise on Metal — record
   yours.
3. **Composite gate on D3D11:** `tools/composite_gate.py --app … --fixture
   tests/fixtures/composite_512_metal.rgba --out <dir>` (default
   `--max-diff 0`). Measure; if it differs, explain it and propose a D3D11
   tier with evidence in a decision entry — do not loosen Metal's. Compare
   with what 45a found on GL.
4. **Harness tests** in a scratch folder (they write PNGs into the cwd):
   `--anod-test` 9/9, `--chladni-test` 7/7, `--palette-test` 5/5 (its hashes
   are Metal's prints — if D3D11's differ, report them and whether the prints
   look identical; don't edit the Metal table), `--print-test` 5/5 (4k/8k
   exports and Anod over alpha: the D3D11 readback's first real workout),
   `--gesture-test` 6/6, `--burst-test`, `--spark-test`, `--torsion-test`,
   `--chirikov-test`, soaks `chladni`, `chladni-field`, `spark`, `burst`.
5. **Look at it:** Anod's black glass, the water grid, the bloom halo, the
   Anod palettes and the custom editor, the Sumi paper tints and fibers;
   `--anod-strike-render <dir>` against `docs/evidence/step43/
   anod_strike_after.png`, `anod_strikes_bend.png`, `anod_stir_alone.png`.
6. **Prints:** dip, thumbnails, re-export at 4K and an Anod sheet over
   alpha, open the PNGs.
7. **Presets across machines:** `docs/evidence/step44a/desktop-anod.json`
   into `%APPDATA%\midi-sink\presets\`, load it, type `desktop-anod` in the
   name box, export, and compare the two files — **byte-identical** (the
   iPad, the web page and Linux are). Watch line endings: the serializer
   writes `\n`; the file must not come back with `\r\n`.
8. **Gestures (#75)** in both media: click (Sumi drop / Anod strike),
   Shift-drag (fold / bursts), right-drag (vortex / torsion vortex), Shift +
   right-drag press (feed–swirl / torsion feed–Chladni stir).
9. **The ROLI in Anod:** strike = the spark, bend = the Chladni stir both
   ways, poly pressure = the torsion / spark wavenumbers, mod wheel = the
   Chirikov throw; the Airwave map unchanged.
10. **The Windows channels unaffected:** the release lane's Windows job
    (`.github/workflows/release.yml`) and the winget manifest are untouched
    by Phase 6 — one local build of the release artifact (zip / installer as
    the lane makes it), install, `--version`, launch.

## Hand-back

`docs/evidence/step46/SUMMARY.md` (gate numbers, test counts, any D3D11
hashes / composite difference, screenshots), decision entries for anything
resolved or fixed (a D3D11 fix must leave Metal, GL and the web where they
were — say what you could not re-run), "tree ready" with a commit
reference. The author commits, tags and publishes.

## The author's open items (don't act on them)

The Chirikov parameters (being tuned); MEDIUM §4's table rows vs what
shipped (#70, #71 — the author owns the spec); the "local spark" proposal
in #72; the phase-end tag and the DECISIONS_5 fold.

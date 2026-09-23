# Handoff — Linux box: Step 45a (Phase 6 on the Linux desktop)

Written on the macOS machine, 2026-09-23, after steps 42–44b. You are the
agent on the author's Linux box (Ubuntu, GNOME on Wayland, NVIDIA, the GL
backend; Android Studio + NDK and the Galaxy Tab are also here, but Android
is **step 45b — a separate handoff, `_work/ANDROID_HANDOFF.md`, done after
this one**). Do not touch the macOS, iOS, Windows or web lanes.

**Your step:** everything Phase 6 put into the core and the desktop shell has
been built, gated and looked at **only on Metal (macOS) and WebGPU (the
wasm)**. The Linux desktop runs the same C++ through the **GL backend**
(`glsl410` from sokol-shdc) — none of the Phase-6 shaders or render targets
have ever executed on it. Build it, run every gate and harness test, look at
the Anod medium and the new settings with your own eyes, fix what breaks
(bug → regression test → fix), and record it.

> Step numbering: `_work/ROADMAP_5.md` still names the steps 44 iOS / 45
> Android / 46 web. The author works to **44a iOS, 44b web marble, 45a
> Linux, 45b Android, 46 Windows** (flagged in DECISIONS_5 #73/#74). Use the
> author's numbers: evidence in `docs/evidence/step45a/`.

## Read first, in this order

1. `CLAUDE.md` — the working rules. **Agents never commit, stage or push**
   (leave the work as unstaged changes; prepare evidence; report "tree
   ready" with a commit reference). **Agents never edit specs or roadmaps**
   (`specs/*.md`, `_work/ROADMAP_5.md`) — flag disagreements in a decision
   entry. Temporary verification hooks are always removed from the tree.
2. `_work/DECISIONS_5.md` — the Phase-6 record. New entries are numbered
   from **#76** (the Android and Windows agents append after you; if an
   entry number is taken when you write, take the next one). The entries
   that concern you most: #50–#58 (the Anod composite, the seam mask, the
   water grid, `anod_pitch`), #60–#62 (the Chladni cells: an RGBA16F index
   map, the disc / field modes), #63–#69 (palettes, substrate, presets,
   prints and the ledger, the bloom), #70–#72 (the binding table and the
   stir's ownership), #75 (the medium-aware gestures).
3. `docs/evidence/step43/SUMMARY.md`, `step43_chladni/`, `step44a/`,
   `step44b/` — what each landed and how it was measured on the Mac.
4. `docs/BUILD.md` — every harness test and gate, with its command line.

## What changed since your Step 33 verification (all on the Mac)

* **Core ABI 0.9 → 1.1.0.** `sumi_version()` reads 1.1.0. 1.0.0 was the one
  break (`sumi_layout_probe` gained a `state` argument — pass `nullptr`);
  everything after is additive: the medium (`params.medium`, Sumi / Anod),
  the palette model and library (`sumi_set/get_palette`,
  `sumi_palette_preset*`), the substrate and glow params, the export ABI
  (`sumi_read_field`, `sumi_export_begin/poll`), the medium-aware gestures
  (`sumi_gesture_tap/_pinch/_twist/_press/_press_end`). **Rebuild from
  scratch.**
* **New shaders and passes, never run on GL:** `core/src/shaders/bloom.glsl`
  (13-tap downsample / tent upsample over RGBA16F half-res targets), the
  composite's Anod branch and its `linear_out` / `bloom_in` variants, the
  palette path (`pal_gradient` …), the Chladni `cells_fs` pass reading an
  **RGBA16F index map** at field resolution, the export pass (a separate
  offscreen target, then the one swapchain readback slot). The export and
  the ledger read back through `swapchain_gl.cpp`'s readback — the same
  slot the paper-dip print uses.
* **Known trap from the Mac:** sokol-shdc binding NAMES are global across
  the generated headers — two shaders using the same sampler name defined
  the same macro with different slots and sokol refused the composite pass
  (#69). If a GL pass silently draws nothing, check the validation log for a
  binding mismatch first.
* **Desktop shell:** the settings window grew Medium, Substrate, Palette
  (library + custom editor), Presets, Prints (the ledger), Chladni / Burst /
  Spark / Chirikov sections; `desktop/src/print_ledger.cpp` (fields kept per
  dip, thumbnails as GL textures, PNGs written on a background thread);
  presets through `presets/` (pure C11) — `last_session.json` and
  `presets/*.json` beside `settings.ini` in `~/.config/midi-sink/`.

## Checklist

1. **Build + ctest.** `cmake -B build -G Ninja && cmake --build build &&
   ctest --test-dir build` — five suites (`abi_c_compile`,
   `normalizer_tests`, `hostmpe_tests`, `hostmpe_c_compile`,
   `preset_tests`). All must pass unchanged — they are platform-free.
2. **§4.6 field gate on GL:** `tools/field_gate.py --app build/desktop/
   midi-sink --compare build/tests/field_dump_compare --fixture
   tests/fixtures/field_512_metal.bin --backend gl --out <dir>` — the phase
   invariant is bitwise on Metal; on this box's GL the last measurement was
   **not** bitwise but green at the reference tier (mean 6.85e-6, max
   3.9e-3 — DECISIONS_4 #44). Record the numbers against that; a change is
   worth an entry.
3. **Composite gate on GL:** `tools/composite_gate.py --app … --fixture
   tests/fixtures/composite_512_metal.rgba --out <dir>`. The fixture is
   Metal's and the gate defaults to `--max-diff 0`. **Measure** GL's
   difference first; if it is not 0, find out why (a driver's rounding in
   the 8-bit composite is plausible — a wrong palette path is not) and
   propose a GL tier in a decision entry with the evidence. Do not loosen
   the Metal tier.
4. **Harness tests** (`build/desktop/midi-sink --dev --<test>`; they write
   PNGs into the cwd — run them in a scratch folder): `--anod-test` (9/9),
   `--chladni-test` (7/7), `--palette-test` (5/5 — its eight FNV-1a hashes
   are Metal's prints: on GL they may legitimately differ; if they do,
   report the GL hashes and whether the prints are visually identical,
   don't edit the Metal table), `--print-test` (5/5; the 4k/8k export and
   the Anod-over-alpha export are the GL readback's first real workout),
   `--gesture-test` (6/6), `--burst-test`, `--spark-test`,
   `--torsion-test`, `--chirikov-test`, and the soaks `--soak chladni`,
   `--soak chladni-field`, `--soak spark`, `--soak burst` (3/3 each).
5. **Look at it.** Launch the app, switch the medium to Anod (Settings ›
   Medium): black glass, the water's grid where a note displaced it, the
   bloom halo (Substrate › Glow bloom / reach), the three Anod palettes and
   the custom editor, the paper tints and fiber presets in Sumi.
   `--anod-strike-render <dir>` writes the six-strike, bent-strike and
   stir-alone prints — compare them with `docs/evidence/step43/
   anod_strike_after.png`, `anod_strikes_bend.png`, `anod_stir_alone.png`.
6. **Prints.** Dip a few sheets; the Prints section shows thumbnails
   (GL textures in the settings window's context); re-export one at 4K and
   one Anod sheet over alpha; open the PNGs.
7. **Presets across machines.** Copy
   `docs/evidence/step44a/desktop-anod.json` (the author's Mac session) into
   `~/.config/midi-sink/presets/`, load it from Settings › Presets, type
   `desktop-anod` in the name box (the export stamps that name into the
   file), export to a path, and `diff` the two files — they must be
   **byte-identical** (the iPad and the web page already are).
8. **Gestures (#75)** with the mouse, in both media: click (Sumi a drop,
   Anod the strike), Shift-drag (Sumi the fold, Anod bursts), right-drag
   (Sumi the vortex, Anod the torsion vortex), Shift + right-drag press
   (Sumi feed / swirl, Anod torsion feed / pull = the Chladni stir).
9. **The ROLI over ALSA in Anod:** strike = the spark, bend = the Chladni
   stir (both directions), poly pressure = the torsion / spark wavenumbers,
   the mod wheel = the Chirikov throw (#70–#72).
10. **Packaging unaffected?** One local CPack DEB, install it in a clean
    container, `--version`, launch (the Phase-5 procedure,
    `cmake/LinuxPackaging.cmake`).

## Evidence and hand-back

`docs/evidence/step45a/SUMMARY.md` with the gate numbers, the test counts,
the GL hashes / composite difference and your screenshots; decision entries
for anything you resolved or fixed (a GL-specific fix must keep Metal and
the web bitwise — re-run nothing you can't, but say so). Report "tree ready"
with a commit reference; the author commits.

## Open items that are the author's (don't act on them)

* The Chirikov parameters — the author is tuning them.
* MEDIUM §4's table rows now read differently from what shipped (#70, #71)
  — flagged for the author, who owns the spec.
* A "local spark" (a shear windowed along its length) was proposed in #72
  and not built.

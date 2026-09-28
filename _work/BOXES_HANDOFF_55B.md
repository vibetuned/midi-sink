# Step 55b on the Windows and Linux boxes — re-measure the GL and D3D11 tiers

**What changed (DECISIONS_7 #1):** the field's payload is a displacement
(u − x, v − y, ink, aux) instead of the pre-image (u, v, ink, aux); libsumi is
1.2.0; `tests/fixtures/field_512_metal.bin` was re-captured on the Mac. Ink
and aux are bitwise the old fixture; the pre-images agree to 1.6e-3 (the old
rounding). The Sumi print fixture did not move. The Anod prints did (they
read the strain off a finer field), so the palette test's GL and D3D11
columns need recapturing — the Metal column already is.

Do the same on each box, in this order. Every command from the repo root
after `git pull`.

1. **Build and the headless suites.**
   ```
   cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
   ```
   Expect 9 (Linux) / 10 (Windows, the static ABI check) passed; the ABI
   test expects 1.2.0.

2. **The field gate at the backend's tier** (unchanged tiers: max 1e-2, mean
   1e-4; the fixture is the new one).
   ```
   python3 tools/field_gate.py --app <midi-sink> --compare build/tests/field_dump_compare --fixture tests/fixtures/field_512_metal.bin --backend gl --out gate-55b      # Linux
   python3 tools/field_gate.py --app <midi-sink.exe> --compare build/tests/field_dump_compare.exe --fixture tests/fixtures/field_512_metal.bin --backend d3d11 --out gate-55b   # Windows
   ```
   Record the four channel maxima and the mean from the report — those
   numbers ARE the step's cross-vendor result (the old tier sat at max
   1.5e-2 / mean 6e-4 on both boxes, DECISIONS_5 #44, #83; the web tier on
   the Mac fell to mean 3.9e-9 under the new payload). If the run is under
   the tier, green; if a channel exceeds it, do not loosen anything — send
   the report.

3. **The composite gate** (must stay at its tier: one 8-bit step).
   ```
   python3 tools/composite_gate.py --app <midi-sink> --fixture tests/fixtures/composite_512_metal.rgba --out gate-55b --backend gl     # or d3d11
   ```

4. **The palette test — recapture the Anod four.**
   ```
   <midi-sink> --dev --palette-test > gate-55b/palette_test.txt 2>&1
   ```
   The four `medium 0` lines must say `(= recorded)`. The four `medium 1`
   lines will say `(!= recorded)`: copy their eight-hex hashes into the
   backend's column of the `cases[]` table in `desktop/src/dev_tools.cpp`
   (rows 5–8; the `gl` column on Linux, the `d3d11` column on Windows), keep
   `palette_test.txt` as the evidence, rebuild, and run the test again — it
   must then say 5/5. A column is never edited to make a run pass: the
   Sumi four must match without any edit.

5. **The harness self-tests** (all must pass; they read pre-images through
   the bench's reader, nothing in them changed):
   ```
   for t in wake flick rankine pressure swirl ripple-group ripple-dip ripple-permanence stokeslet torsion chladni burst spark chirikov anod print gesture; do <midi-sink> --dev --$t-test > gate-55b/$t.txt 2>&1; grep -c '^FAIL' gate-55b/$t.txt; done
   ```
   Expect every count 0 (the print test prints two "FAILED to save"
   lines from its own negative case — those are not `^FAIL`).

6. **The soak** — `<midi-sink> --dev --soak all > gate-55b/soak_all.txt` and
   `--soak-negative`; `python3 tools/soak_report.py gate-55b/soak_all.txt`
   for the table. The (b) pre-image tier is 32 texels since 55b
   (DECISIONS_7 #3: the displacement payload keeps the resampler's
   smoothing the coordinates froze); expect the exact operators between 9
   and 23 texels after 500 pairs (the Mac: tine 11.9, torsion 16.4, spark
   shear 20.3, the Chladni stir 22.1) with the ink-mass numbers bitwise the
   Phase-6 ones. The crossed pinch's pair is the known open item
   (DECISIONS_5 #17, 73 texels — the negative control's red); everything
   else must hold its class.

**Send back:** the `gate-55b/` folder (the two gate reports, the palette
test log and the hashes you copied, the self-test counts, the soak table)
and, on Windows, the D3D11 `field_gate_d3d11.txt`. The Mac's evidence for
comparison is `docs/evidence/step55b/`.

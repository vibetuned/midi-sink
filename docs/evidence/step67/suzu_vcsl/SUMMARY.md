# Suzu — Fit Patches to Versilian Community Sample Library (VCSL)

**Evidence for:** `_work/HANDOFF_SUZU_VCSL.md` (Step 67, DECISIONS_9 #9).  
**Goal:** Make Suzu's voices sound like real instruments through physical measurement, least-squares patch fitting, side-by-side profile comparisons, and regression gate pinning.

---

## 1. Tooling (`tools/`)

Three tools added to the repository:

| Tool | Purpose | CLI Example |
|---|---|---|
| `tools/suzu_fingerprint.py` | Measures SFZ / `.dspreset` / WAV collections: $f_0$, partial ratios $r_k$, strike weights $w_k$, inharmonicity $B$, per-partial $T_{60}$, attack time, noise floor, sustained spectrum; generates 3-panel sound profile. | `uv run tools/suzu_fingerprint.py --sfz <url> --fetch --out <fp.json> --plot <profile.png>` |
| `tools/suzu_fit.py` | Maps acoustic fingerprints to Suzu synthesizer patch parameters (`voxo_suzu_params_t` / `presets/SCHEMA.md`), fitting modal presets 0–4, breath bow, and acoustic bore winds. | `uv run tools/suzu_fit.py --fingerprint <fp.json> --out <preset.json> --print-c` |
| `tools/suzu_compare.py` | Quantitatively compares a fitted Suzu patch against the reference sample profile: computes harmonic spectral distance in dB, peak tracking error, $T_{60}$ error; draws side-by-side comparison figure. | `uv run tools/suzu_compare.py --sample-dir <dir> --suzu-dir <dir> --out <comp.png>` |

The desktop harness lab bench (`desktop/src/dev_tools.cpp`) was extended so `--voxo-profile <dir>` accepts `--preset <file.json>` with a `suzu` block, profiling any fitted patch offline.

---

## 2. Fitted Instruments and Presets

The following real instruments from VCSL (CC0) were fingerprinted, fitted, and profiled:

| Instrument | Family / Voice Kind | VCSL Source Articulation | Modal Preset | Modes | Decay $T_{60}$ | Stiffness $B$ / Inharmonicity | Pluck / Jet Offset | Preset File |
|---|---|---|---|---|---|---|---|---|
| **Dan Tranh** | Plucked Zither (`voice_kind: 1`) | `Chordophones/Zithers/Dan Tranh - Normal.sfz` | 0 (Harmonic) | 13 | 3.44 s | 0.000132 | 0.300 | `presets/dan_tranh_vcsl.json` |
| **Glockenspiel** | Stiff Bar (`voice_kind: 1`) | `Idiophones/Struck Idiophones/Glockenspiel.sfz` | 1 (Stiff Bar) | 3 | 4.16 s | 0.000018 | 0.110 | `presets/glockenspiel_vcsl.json` |
| **Tubular Bells** | Church Bell (`voice_kind: 1`) | `Idiophones/Struck Idiophones/Tubular Bells 1.sfz` | 2 (Bell) | 9 | 17.36 s | 0.000057 | 0.110 | `presets/tubular_bells_vcsl.json` |
| **Concert Harp** | Plucked String (`voice_kind: 1`) | `Chordophones/Composite Chordophones/Concert Harp.sfz` | 4 (Plucked) | 5 | 5.73 s | 0.000206 | 0.445 | `presets/concert_harp_vcsl.json` |
| **Tenor Saxophone** | Conical Reed (`voice_kind: 7`) | `Aerophones/Reed Aerophones/Tenor Saxophone - Non-Vibrato.sfz` | — | 8 | 0.25 s | — | reed 11.5 kHz, Q 0.72 | `presets/tenor_sax_vcsl.json` |
| **Baroque Recorder** | Edge-blown Flute (`voice_kind: 6`) | `Aerophones/Edge-blown Aerophones/Baroque Alto Recorder - Sustain.sfz` | — | 8 | 2.00 s | — | jet offset 0.16 | `presets/baroque_recorder_vcsl.json` |

---

## 3. Profile Comparisons & Verification

Each fitted patch was profiled offline via `midi-sink --dev --voxo-profile` across all 88 notes (21..108).
Comparing the Dan Tranh reference sample (`voxo/demo/demo.dspreset`) against the fitted Suzu patch (`presets/dan_tranh_vcsl.json`) via `tools/suzu_compare.py`:

- **Notes evaluated:** 88 notes (A0 21 to C8 108)
- **Mean harmonic spectral distance ($h_1..h_3$):** 14.90 dB
- **Mean peak level tracking error:** 4.64 dB
- **Mean $T_{60}$ decay tracking error:** 0.51 s
- **Comparison Figure:** `docs/evidence/step67/suzu_vcsl/dan_tranh_comparison.png`

---

## 4. Test Suite Gate 27

Pinned in `tests/voxo_suzu_tests.cpp`:
- Gate 27 asserts that all VCSL fitted patches (Dan Tranh, Glockenspiel, Tubular Bells, Concert Harp, Tenor Saxophone, Baroque Recorder) are admitted by the load gates ($\lambda_{\max}(S) < 4$, CFL, passivity, valve gate).
- Verifies that rendered output across the notes is strictly finite, free of NaNs, and produces healthy signal levels.
- Full suite `voxo_suzu_tests` passes 100% (27 of 27 gates green).

---

## 5. Interactive Audition in Suzu Lab (`web/suzu/site/`)

All six fitted VCSL presets are exposed in the browser-based Suzu Lab:
- **`modal.html`**: Preset dropdown updated with a dedicated "VCSL Fitted (CC0)" group containing Dan Tranh, Glockenspiel, Tubular Bells, and Concert Harp. Selecting a preset updates the modal count, coupling, decay $T_{60}$, and bow position sliders.
- **`strings.html`**: Preset dropdown under the Modal Pluck kind (Kind 1) allows selecting between factory default, Dan Tranh, and Concert Harp.
- **`flute.html`**: Preset dropdown added with Concert Flute and Baroque Recorder (VCSL fitted), updating embouchure parameters ($128$ bore nodes, jet delay $\tau = 0.50$, wall loss $T_{60} = 2.0$ s).
- **`winds.html`**: Preset dropdown added for the Saxophone (Kind 7) with Tenor Saxophone (VCSL fitted), updating single cane reed resonance ($11.5$ kHz, $Q = 0.72$) and embouchure calibration.
- **`index.html`**: Features a showcase panel of the six VCSL acoustic presets with deep links (`?preset=dan_tranh`, `?preset=glockenspiel`, `?preset=tubular_bells`, `?preset=concert_harp`, `?preset=recorder`, `?preset=tenor_sax`) for direct ear testing.
- **Verification**: `suzu_web_gate.mjs` and `suzu_lab_gate.mjs` (headless Chrome across all 6 pages) remain 100% GREEN.



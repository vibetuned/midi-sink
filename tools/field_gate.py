#!/usr/bin/env python3
"""The §4.6 cross-backend field regression as a RELEASE GATE (Phase 5 §3,
DECISIONS_4 #11), with its own negative control.

  field_gate.py --app <midi-sink binary> --compare <field_dump_compare>
                --fixture tests/fixtures/field_512_metal.bin --backend metal
                --out <dir> [--max-tol 1e-2] [--mean-tol 1e-4]

1. Runs `midi-sink --dev --field-dump <out>/field_<backend>.bin` — the
   canonical seven-pass deform script on a fresh 512x512 field.
2. Compares it against the committed Metal fixture with the backend's
   tolerance. Must PASS.
3. NEGATIVE CONTROL: writes a deliberately corrupted copy of the fixture
   (+0.5 on the u channel of a 32x32 block) and compares THAT. It must FAIL.
   A gate that cannot go red is not a gate; this proves red before green,
   every run (the roadmap's "deliberately broken fixture must block").

Exit codes: 0 gate green; 1 regression FAILED (the field differs beyond
tolerance); 3 the comparator did not fail the corrupted fixture (gate
broken); 4 the app could not produce a dump (no renderer on this machine —
an infrastructure problem, named as such); 2 usage.
"""
import argparse
import os
import struct
import subprocess
import sys


def run(cmd, timeout=None, env=None):
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def corrupt_fixture(src, dst):
    with open(src, "rb") as f:
        data = bytearray(f.read())
    w, h = struct.unpack_from("<II", data, 0)
    # +0.5 on the u channel of a 32x32 block in the middle of the field: a
    # gross, unmistakable regression (max tolerance is 1e-2).
    for y in range(h // 2 - 16, h // 2 + 16):
        for x in range(w // 2 - 16, w // 2 + 16):
            off = 8 + ((y * w + x) * 4) * 4
            (u,) = struct.unpack_from("<f", data, off)
            struct.pack_into("<f", data, off, u + 0.5)
    with open(dst, "wb") as f:
        f.write(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True)
    ap.add_argument("--compare", required=True)
    ap.add_argument("--fixture", required=True)
    ap.add_argument("--backend", required=True, choices=["metal", "d3d11", "gl"])
    ap.add_argument("--out", required=True)
    # Two tiers (DECISIONS_4 #11, #20, #35): reference hardware (the fixture's
    # own GPU family — Apple) 1e-2 / 1e-4; software rasterizers on CI runners
    # (WARP, llvmpipe) 2.5e-2 / 1e-3 — they agree with each other to ~2e-5 and
    # sit 6e-4 from the Apple fixture through half-float rounding alone.
    ap.add_argument("--max-tol", default="1e-2")
    ap.add_argument("--mean-tol", default="1e-4")
    a = ap.parse_args()

    os.makedirs(a.out, exist_ok=True)
    dump = os.path.join(a.out, f"field_{a.backend}.bin")
    report = []

    # 1. the dump
    try:
        code, log = run([a.app, "--dev", "--field-dump", dump], timeout=180)
    except subprocess.TimeoutExpired:
        print("::error::field dump timed out (renderer hung?)")
        return 4
    report.append(f"$ {a.app} --dev --field-dump {dump}\n{log.strip()}")
    if code != 0 or not os.path.exists(dump):
        print(log)
        print(f"::error::no field dump on this machine (exit {code}) — the {a.backend} "
              f"renderer could not be created; this is an infrastructure failure, not a "
              f"regression")
        return 4

    # 2. the regression
    code, log = run([a.compare, dump, a.fixture, a.max_tol, a.mean_tol])
    report.append(f"$ compare dump vs fixture (max {a.max_tol}, mean {a.mean_tol})\n{log.strip()}")
    print(log.strip())
    if code != 0:
        print(f"::error::§4.6 field regression FAILED on {a.backend} (exit {code})")
        write_report(a.out, a.backend, report, "FAIL")
        return 1

    # 3. the negative control
    bad = os.path.join(a.out, "fixture_corrupted.bin")
    corrupt_fixture(a.fixture, bad)
    code, log = run([a.compare, dump, bad, a.max_tol, a.mean_tol])
    report.append(f"$ compare dump vs CORRUPTED fixture (must fail)\n{log.strip()}")
    if code != 1:
        print(log.strip())
        print(f"::error::the comparator did not fail a corrupted fixture (exit {code}) — "
              f"the gate cannot go red; refusing to trust its green")
        write_report(a.out, a.backend, report, "GATE BROKEN")
        return 3
    print(f"negative control: corrupted fixture rejected (exit 1) — the gate can go red")
    print(f"§4.6 field gate GREEN on {a.backend} (tier max {a.max_tol} / mean {a.mean_tol})")
    write_report(a.out, a.backend, report, "PASS")
    return 0


def write_report(out, backend, report, verdict):
    with open(os.path.join(out, f"field_gate_{backend}.txt"), "w") as f:
        f.write(f"verdict: {verdict}\n\n" + "\n\n".join(report) + "\n")


if __name__ == "__main__":
    sys.exit(main())

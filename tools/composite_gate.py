#!/usr/bin/env python3
"""The composite SCREENSHOT regression as a gate (Phase 6 step 41,
DECISIONS_5): the print of the canonical field script — the pixels the
palette table, the washi and the ink-depth curve produce — compared bitwise
against the committed Metal fixture, with its own negative control.

  composite_gate.py --app <midi-sink binary> --fixture tests/fixtures/composite_512_metal.rgba
                    --out <dir> [--max-diff 0]

1. Runs `midi-sink --dev --composite-dump <out>/composite.rgba`: the seven-
   pass field script on a fresh 512x512 field, then a paper dip and its print.
2. Compares it against the fixture: same size, and no channel differs by more
   than --max-diff (0 on Metal: bitwise). Must PASS.
3. NEGATIVE CONTROL: a copy of the fixture with a 32x32 block brightened by
   64 must FAIL the same comparison — the gate goes red before it is trusted.

Exit codes: 0 green; 1 the composite differs; 3 the corrupted fixture passed
(gate broken); 4 the app produced no dump; 2 usage.
"""
import argparse
import os
import struct
import subprocess
import sys


def read_dump(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 8:
        raise ValueError(f"{path}: too short")
    w, h = struct.unpack_from("<II", data, 0)
    px = data[8:]
    if len(px) != w * h * 4:
        raise ValueError(f"{path}: {w}x{h} announces {w * h * 4} bytes, holds {len(px)}")
    return w, h, px


def compare(a_path, b_path, max_diff):
    wa, ha, pa = read_dump(a_path)
    wb, hb, pb = read_dump(b_path)
    if (wa, ha) != (wb, hb):
        return False, f"size {wa}x{ha} vs {wb}x{hb}", 255, len(pa)
    worst = 0
    differing = 0
    for i in range(0, len(pa)):
        d = abs(pa[i] - pb[i])
        if d:
            differing += 1
            if d > worst:
                worst = d
    return worst <= max_diff, f"max channel diff {worst}, {differing} of {len(pa)} channel samples differ", worst, differing


def corrupt(src, dst):
    w, h, px = read_dump(src)
    px = bytearray(px)
    for y in range(h // 2 - 16, h // 2 + 16):
        for x in range(w // 2 - 16, w // 2 + 16):
            o = (y * w + x) * 4
            px[o] = min(255, px[o] + 64)
    with open(dst, "wb") as f:
        f.write(struct.pack("<II", w, h))
        f.write(px)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True)
    ap.add_argument("--fixture", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-diff", type=int, default=0)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    dump = os.path.join(a.out, "composite.rgba")
    p = subprocess.run([a.app, "--dev", "--composite-dump", dump], capture_output=True, text=True, timeout=300)
    if p.returncode != 0 or not os.path.exists(dump):
        print("composite gate: the app produced no dump (infrastructure, not a regression)")
        print((p.stdout or "") + (p.stderr or ""))
        return 4
    ok, msg, worst, differing = compare(dump, a.fixture, a.max_diff)
    print(f"composite vs fixture: {msg} (max allowed {a.max_diff})")
    if not ok:
        print("composite gate FAILED: the print differs from the fixture")
        return 1
    bad = os.path.join(a.out, "composite_corrupted.rgba")
    corrupt(a.fixture, bad)
    ok_bad, msg_bad, _, _ = compare(dump, bad, a.max_diff)
    print(f"negative control (a 32x32 block brightened): {msg_bad}")
    if ok_bad:
        print("composite gate BROKEN: the corrupted fixture passed")
        return 3
    print(f"composite gate GREEN on metal (bitwise, max diff {worst}); the negative control went red as required")
    return 0


if __name__ == "__main__":
    sys.exit(main())

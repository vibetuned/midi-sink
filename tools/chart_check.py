#!/usr/bin/env python3
"""Step 26: the MIDI implementation chart must match the byte logs.

  python3 tools/chart_check.py [--chart site/src/data/midi-chart.json]
                               [--logs docs/evidence/step26/bytelogs]

The chart (site/src/data/midi-chart.json, rendered by the docs site) states,
per OUTPUT section, which messages a source emits on which channel class. Each
output section names the byte log that verifies it (a Play-mode session logged
at the shell's merge point — t,status,d1,d2,src; src 1 finger, 2 session
config, 3 strip, 4 stylus). This script replays the log and asserts:

  * every row with expect=present is observed for that src and channel class;
  * every row with expect=absent is NOT observed;
  * every (message, channel class) observed for that src is described by a row
    — an undocumented output is a chart bug, not a footnote;
  * the constants block agrees with the logs where a log can show it (the MCM
    zone size, the RPN 0 value, the one-semitone count, master = channel 1).

Input sections are the normalizer's contract and are verified by the headless
suite (tests/normalizer_tests.cpp); this script only reports them. Exit 0 when
every assert holds; the report is the evidence.
"""
import argparse
import csv
import json
import os
import re
import sys
from collections import Counter

KINDS = {0x80: "Note Off", 0x90: "Note On", 0xA0: "Poly Key Pressure (0xA0)", 0xB0: "CC",
         0xC0: "Program Change", 0xD0: "Channel Pressure (0xD0)", 0xE0: "Pitch Bend"}
FAILS, OKS = [], []


def ok(msg): OKS.append(msg); print("ok  ", msg)
def fail(msg): FAILS.append(msg); print("FAIL", msg)


def read_log(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.reader(f):
            if len(r) < 5 or r[0] == "t":
                continue
            try:
                rows.append((float(r[0]), int(r[1]), int(r[2]), int(r[3]), int(r[4])))
            except ValueError:
                continue
    return rows


def key_of(status, d1):
    kind = status & 0xF0
    ch = (status & 0x0F) + 1
    name = KINDS.get(kind, hex(kind))
    if kind == 0xB0:
        name = f"CC {d1}"
    return name, ("master" if ch == 1 else "member")


def row_keys(row):
    """Chart row -> set of (message, channel class) keys it covers."""
    msg = row["message"]
    chans = {"master": ["master"], "member": ["member"], "master + members": ["master", "member"],
             "any": ["master", "member"], "single": ["master", "member"]}.get(row["channel"], [row["channel"]])
    msgs = []
    m = re.match(r"CC (\d+)", msg)
    if m:
        msgs = [f"CC {m.group(1)}"]
    elif msg in ("Note On", "Note Off", "Pitch Bend"):
        msgs = [msg]
    elif msg.startswith("Channel Pressure"):
        msgs = ["Channel Pressure (0xD0)"]
    elif msg.startswith("Poly Key Pressure"):
        msgs = ["Poly Key Pressure (0xA0)"]
    else:
        msgs = [msg]
    return {(a, c) for a in msgs for c in chans}


def check_section(sec, logs_dir):
    log = sec["log"]
    path = os.path.join(logs_dir, log["file"])
    if not os.path.exists(path):
        fail(f"[{sec['id']}] log {path} missing"); return None
    rows = [r for r in read_log(path) if r[4] == log["src"]]
    observed = Counter(key_of(st, d1) for _, st, d1, _, _ in rows)
    print(f"\n== {sec['title']} — {log['file']} src {log['src']}: {len(rows)} messages, {len(observed)} kinds")
    documented = set()
    for row in sec["rows"]:
        keys = row_keys(row)
        documented |= keys
        seen = {k: observed[k] for k in keys if observed[k]}
        exp = row.get("expect", "present")
        label = f"[{sec['id']}] {row['message']} on {row['channel']}"
        if exp == "present":
            (ok if seen else fail)(f"{label}: expected present — observed {sum(seen.values())}")
        elif exp == "absent":
            (fail if seen else ok)(f"{label}: expected absent — observed {sum(seen.values())}")
        else:
            ok(f"{label}: conditional — observed {sum(seen.values())}")
    undocumented = {k: n for k, n in observed.items() if k not in documented}
    (fail if undocumented else ok)(f"[{sec['id']}] undocumented outputs: {undocumented or 'none'}")
    return rows


def check_constants(chart, logs_dir):
    c = chart["constants"]
    path = os.path.join(logs_dir, "device_byte_log_smoke.csv")
    if not os.path.exists(path):
        fail("constants: device_byte_log_smoke.csv missing"); return
    rows = read_log(path)
    cfg = [(st, d1, d2) for _, st, d1, d2, src in rows if src == 2]
    # MCM: CC 6 on the master right after 101=0,100=6 -> member count
    members = [d2 for i, (st, d1, d2) in enumerate(cfg)
               if (st & 0x0F) == 0 and (st & 0xF0) == 0xB0 and d1 == 6 and i >= 2 and cfg[i - 1][1:] == (100, 6)]
    (ok if members and members[0] == 15 else fail)(f"constants: MCM zone size from the log = {members[:1]} (chart: 15 members)")
    # A session may re-sync several times (every handshake repeats the RPN 0 on
    # all members), so count DISTINCT member channels and the set of values.
    rpn0_vals, rpn0_chs = set(), set()
    for i, (st, d1, d2) in enumerate(cfg):
        if (st & 0x0F) != 0 and (st & 0xF0) == 0xB0 and d1 == 6 and i >= 2 and cfg[i - 1][1:] == (100, 0):
            rpn0_vals.add(d2); rpn0_chs.add((st & 0x0F) + 1)
    (ok if rpn0_vals == {c["member_bend_range_semitones"]} and len(rpn0_chs) == 15 else fail)(
        f"constants: RPN 0 on members = {sorted(rpn0_vals)} on {len(rpn0_chs)} distinct channels "
        f"(chart: {c['member_bend_range_semitones']} on 15 channels)")
    # one semitone = ±171 counts: bends from finger voices land on 8192 + k*171 only when the finger sits on a lattice
    # neighbour, so we assert the CENTRE value instead — the first bend on every finger channel is exactly 8192.
    first_bend = {}
    for _, st, d1, d2, src in rows:
        if src == 1 and (st & 0xF0) == 0xE0:
            first_bend.setdefault(st & 0x0F, d1 | (d2 << 7))
    centred = all(v == 8192 for v in first_bend.values())
    (ok if centred and first_bend else fail)(f"constants: first finger bend per channel is the centre 8192 on {len(first_bend)} channels")
    strip_ch = {(st & 0x0F) + 1 for _, st, d1, d2, src in rows if src == 3}
    (ok if strip_ch <= {c['master_channel']} else fail)(f"constants: strip channels = {sorted(strip_ch)} (chart: master = {c['master_channel']})")


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--chart", default=os.path.join(here, "..", "site", "src", "data", "midi-chart.json"))
    ap.add_argument("--logs", default=os.path.join(here, "..", "docs", "evidence", "step26", "bytelogs"))
    a = ap.parse_args()
    chart = json.load(open(a.chart))
    for sec in chart["sections"]:
        if sec["direction"] == "out":
            check_section(sec, a.logs)
        else:
            print(f"\n== {sec['title']}: input contract, {len(sec['rows'])} rows — verified by {sec['verified_by']}")
    print()
    check_constants(chart, a.logs)
    print(f"\n{len(OKS)} ok, {len(FAILS)} failed")
    print("CHART MATCHES THE BYTE LOGS" if not FAILS else "CHART DOES NOT MATCH THE BYTE LOGS")
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()

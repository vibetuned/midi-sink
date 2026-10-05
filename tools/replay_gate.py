#!/usr/bin/env python3
"""Phase 9 step 65 (QOL §1, DECISIONS_8 #24): THE REPLAY GATE.

  replay_gate.py --bench <midi-sink> --compare <field_dump_compare> --replay <file.sumireplay>
                 --field <recorded.field.bin> --out <dir> [--max-tol 1e-2] [--mean-tol 1e-4]
                 [--wall-hz 60] [--label <text>]

Replays the file on this machine's bench at the recorded size on the scripted
clock and compares the field after the last frame with the dump the RECORDING
device wrote beside the file — within the §4.6 tier given (the fixture's tiers:
Apple GPUs 1e-2 / 1e-4, software rasterizers 2.5e-2 / 1e-3). Must PASS.
Then the NEGATIVE test: the same file re-bucketed by wall time at --wall-hz —
what a host re-grouping the bytes on its own cadence would do — must FAIL the
same tier: the frame field is load-bearing. Exit 0 = GREEN (both as required),
1 = the positive replay failed, 2 = the negative did not diverge, 3 = a run
failed. Writes <out>/replayed.bin, <out>/wall.bin and <out>/report.txt.
"""
import argparse, os, subprocess, sys

def run(cmd):
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return p.returncode, p.stdout

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bench", required=True)
    ap.add_argument("--compare", required=True)
    ap.add_argument("--replay", required=True)
    ap.add_argument("--field", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-tol", default="1e-2")
    ap.add_argument("--mean-tol", default="1e-4")
    ap.add_argument("--wall-hz", default="60")
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    report = []
    def say(s):
        report.append(s); print(s)
    say(f"replay gate{(' — ' + a.label) if a.label else ''}: {a.replay}")
    with open(a.replay, "r", encoding="utf-8", errors="replace") as f:
        head = [next(f, "").rstrip("\n") for _ in range(11)]
    say("  " + " | ".join(l for l in head if l and not l.startswith("#")))

    replayed = os.path.join(a.out, "replayed.bin")
    code, log = run([a.bench, "--dev", "--replay", a.replay, "--field-dump", replayed])
    lines = [l for l in log.splitlines() if l.startswith("[replay]") or l.startswith("[field-dump]")]
    say("$ bench --dev --replay <file> --field-dump replayed.bin\n  " + "\n  ".join(lines))
    if code != 0 or not os.path.exists(replayed):
        say(f"RED: the replay run failed (exit {code})"); write(a, report); return 3
    code, log = run([a.compare, a.field, replayed, a.max_tol, a.mean_tol])
    say(f"$ compare recorded.field.bin vs replayed.bin (max {a.max_tol}, mean {a.mean_tol})\n  " + "\n  ".join(log.strip().splitlines()))
    positive = code == 0

    wall = os.path.join(a.out, "wall.bin")
    code, log = run([a.bench, "--dev", "--replay", a.replay, "--replay-wall", a.wall_hz, "--field-dump", wall])
    lines = [l for l in log.splitlines() if l.startswith("[replay]") or l.startswith("[field-dump]")]
    say(f"$ bench --dev --replay <file> --replay-wall {a.wall_hz} --field-dump wall.bin   (the NEGATIVE test)\n  " + "\n  ".join(lines))
    if code != 0 or not os.path.exists(wall):
        say(f"RED: the wall-time run failed (exit {code})"); write(a, report); return 3
    code, log = run([a.compare, a.field, wall, a.max_tol, a.mean_tol])
    say(f"$ compare recorded.field.bin vs wall.bin (must FAIL)\n  " + "\n  ".join(log.strip().splitlines()))
    negative_diverged = code != 0

    if positive and negative_diverged:
        say(f"replay gate GREEN{(' (' + a.label + ')') if a.label else ''}: the replay within the tier (max {a.max_tol} / mean {a.mean_tol}); the wall-time re-bucketing at {a.wall_hz} Hz diverged, as required")
        rc = 0
    elif not positive:
        say("replay gate RED: the replay left the tier"); rc = 1
    else:
        say("replay gate RED: the wall-time re-bucketing did NOT diverge — the frame field is not load-bearing for this recording"); rc = 2
    write(a, report)
    return rc

def write(a, report):
    with open(os.path.join(a.out, "report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")

if __name__ == "__main__":
    sys.exit(main())

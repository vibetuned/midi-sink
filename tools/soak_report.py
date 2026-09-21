#!/usr/bin/env python3
"""Soak logs -> the evidence table (Phase 6 step 35, ROADMAP_5; DECISIONS_3 #33).

  soak_report.py <log> [<log> ...] [--out table.md]

Reads the `[soak]` lines the desktop harness prints under
`midi-sink --dev --soak <operator|all>` (and `--soak-negative`) and writes one
Markdown table: per operator its declared class, the four verdicts and the
numbers behind them. The gate's verdict lines are the source of truth; this
script only tabulates — it never re-decides.

Lines it consumes (everything else is ignored):
  [soak] <op> SUMMARY class=<exact|sub-stepped> pairs_mass_lo=<%> pairs_mass_hi=<%>
         preimage_dev=<texels> growth=<%> rate=<per pass> control=<per pass> ratio=<x>
  ok:   [soak] <op> (b|c|d) ...      /  FAIL: [soak] <op> (b|c|d) ...
  ok:   [soak] negative-<name> ...   /  FAIL: [soak] negative-<name> ...
"""
import argparse
import re
import sys

SUMMARY = re.compile(r"\[soak\] (\S+) SUMMARY (.*)$")
VERDICT = re.compile(r"^(ok|FAIL):\s+\[soak\] (\S+) \((b|c|d)\)")
NEGATIVE = re.compile(r"^(ok|FAIL):\s+\[soak\] (negative-\S+) (.*)$")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out", default="")
    a = ap.parse_args()

    ops = {}       # name -> {"class":..., numbers..., "b":ok/FAIL, "c":..., "d":...}
    order = []
    negatives = []
    for path in a.logs:
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError as e:
            print(f"cannot read {path}: {e}", file=sys.stderr)
            return 2
        for line in text.splitlines():
            m = SUMMARY.search(line)
            if m:
                name = m.group(1)
                d = ops.setdefault(name, {})
                if name not in order:
                    order.append(name)
                for kv in m.group(2).split():
                    if "=" in kv:
                        k, v = kv.split("=", 1)
                        d[k] = v
                continue
            m = VERDICT.match(line)
            if m:
                d = ops.setdefault(m.group(2), {})
                if m.group(2) not in order:
                    order.append(m.group(2))
                d[m.group(3)] = "PASS" if m.group(1) == "ok" else "FAIL"
                continue
            m = NEGATIVE.match(line)
            if m:
                negatives.append((m.group(2), "red as required" if m.group(1) == "ok" else "NOT RED", m.group(3)))

    out = []
    if order:
        out.append("| Operator | Class (a) | (b) reversibility: ±k pairs mass lo / hi, pre-image dev (sub-stepped: one-step Jacobian) | (c) max growth | (d) erosion / pass vs glide-tine control | Verdicts b · c · d |")
        out.append("|---|---|---|---|---|---|")
        for name in order:
            d = ops[name]
            g = d.get
            b_cell = f"{g('pairs_mass_lo','?')} / {g('pairs_mass_hi','?')}, {g('preimage_dev','?')} texel"
            if "det_min" in d:
                b_cell += f" (pairs informational); one sub-step det min {float(d['det_min']):.3f}, mean {float(d['det_mean']):.5f}"
            out.append(
                f"| `{name}` | {g('class','?')} | {b_cell} | "
                f"{g('growth','?')} | {g('rate','?')} vs {g('control','?')} (×{g('ratio','?')}) | "
                f"{g('b','—')} · {g('c','—')} · {g('d','—')} |"
            )
    if negatives:
        out.append("")
        out.append("| Negative control | Outcome | Detail |")
        out.append("|---|---|---|")
        for name, outcome, detail in negatives:
            out.append(f"| `{name}` | {outcome} | {detail} |")
    if not out:
        print("no [soak] lines found", file=sys.stderr)
        return 1
    table = "\n".join(out) + "\n"
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(table)
        print(f"wrote {a.out} ({len(order)} operators, {len(negatives)} negative controls)")
    else:
        sys.stdout.write(table)
    return 0


if __name__ == "__main__":
    sys.exit(main())

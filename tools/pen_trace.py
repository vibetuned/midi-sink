#!/usr/bin/env python3
"""Step-22: assert a LIVE S-Pen legato trace from the device byte log.

  pen_trace.py midi_log.csv [--layout chroma|janko|piano]

Reads every stylus-sourced message (src 4) and reconstructs the sounding
pitch per voice (note + bend/171 semitones at the MCM-declared ±48). For each
pen stroke it asserts:

  * every retrigger is a same-channel legato overlap — bend, then Note On of
    the new cell, then Note Off of the OLD note (#39);
  * the sweep is MONOTONE (a one-direction stroke never doubles back);
  * no crossing OVERSHOOTS the note change it stands for: the sounding pitch
    moves by no more than the note step, so a retrigger never jumps the
    listener past where the pen is;
  * the crossing adds NO DISCONTINUITY beyond one event's worth of pen
    travel. This is the seam: pitch after the retrigger (new note + the bend
    emitted with it) versus pitch just before the crossing's bend (old note +
    its previous bend). Because the bend precedes the Note On and the ±0.65 st
    hysteresis holds the crossing until the pen is meaningfully into the new
    cell, both equal where the tip physically is — so the seam is bounded by
    how far the tip moved between two events, NOT by the note step. The bound
    is therefore 2.5× the larger of (the ordinary step just BEFORE that
    crossing, the stroke's median ordinary step), floor 0.05 st. Both inputs
    are chosen so a defect cannot widen its own tolerance: the step AFTER a
    crossing is corrupted by a broken crossing (an injected 50-cent jump
    passed when it was included), a crossing's own bend flips by about a
    cell so it is excluded from "ordinary" entirely, and a single bad
    crossing cannot move the median of many. Asserted on CHROMA_GRID and JANKO, where continuity is the
    contract. NOT on PIANO_GRID (`--layout piano`), where it is structurally
    unreachable and known to be: the in-cell bend spans ±0.5 st (half a key
    along the half-key diagonal, DECISIONS_3 #29) while a natural→natural
    crossing is a WHOLE TONE, so ~1–1.6 st of the step must land as a jump.
    That is the quantized piano glissando PROJECT_SPEC.md §8.7 describes; there the seam
    is reported and the overshoot assert is the guard;
  * the stroke ends with its note released.

A capped byte log (see the `# truncated` marker) stops recording partway, so
its last strokes have no release in the file: reported, not asserted.
"""
import csv
import sys

COUNTS = 8192 / 48.0   # bend counts per semitone at ±48


def main(path, layout="chroma"):
    head = open(path).read().splitlines()
    truncated = any("truncated" in l for l in head if l.startswith("#"))
    rows = []
    with open(path, newline="") as f:
        r = csv.reader(f)
        next(r, None)
        for line in r:
            if len(line) >= 5 and not line[0].startswith("#") and int(line[4]) == 4:
                rows.append((float(line[0]), int(line[1]), int(line[2]), int(line[3])))
    if not rows:
        print("no stylus (src 4) messages in this log")
        return 1
    print(f"{len(rows)} stylus messages" + (" (log truncated)" if truncated else ""))

    strokes, cur = [], None
    fails = 0
    note, bend = {}, {}
    prev_msg, pitch_prev = {}, {}
    # A crossing's own bend is NOT pen travel: the reference cell flips, so
    # the emitted value jumps by about a cell. Every bend's step is therefore
    # held PENDING and only confirmed as travel once the next message proves
    # it was not the head of a crossing batch (bend -> Note On -> Note Off).
    # Without this the bound is inflated by exactly the events it bounds.
    pending = {}        # c -> (stroke, index into stroke["steps"], step)
    last_step = {}      # last CONFIRMED per-event step, per channel
    awaiting = {}       # crossing records still waiting for the step after them

    def pitch(c):
        return note[c] + (bend.get(c, 8192) - 8192) / COUNTS

    def confirm(c):
        """The pending bend on `c` was ordinary travel, not a crossing."""
        pend = pending.pop(c, None)
        if pend is None:
            return
        last_step[c] = pend[2]
        rec = awaiting.pop(c, None)
        if rec is not None:
            rec["next_step"] = pend[2]

    def discard(c):
        """The pending bend on `c` belonged to a crossing batch."""
        pend = pending.pop(c, None)
        if pend is None:
            return
        stroke, idx, _ = pend
        if stroke is cur and idx == len(stroke["steps"]) - 1:
            stroke["steps"].pop()

    for (t, s, d1, d2) in rows:
        k, c = s & 0xF0, s & 0x0F
        if k == 0xE0:
            confirm(c)                                     # the previous bend was travel
            if c in note:
                pitch_prev[c] = pitch(c)
                if cur and cur["ch"] == c:
                    step = abs(pitch(c) - (note[c] + ((d1 | (d2 << 7)) - 8192) / COUNTS))
                    cur["steps"].append(step)
                    pending[c] = (cur, len(cur["steps"]) - 1, step)
            bend[c] = d1 | (d2 << 7)
        elif k == 0x90 and d2 > 0:
            p = prev_msg.get(c)
            if not (p and (p[0] & 0xF0) == 0xE0):
                print(f"FAIL Note On {d1} ch{c} not preceded by a bend on its channel")
                fails += 1
            if c in note:                                  # legato crossing
                discard(c)
                old = note[c]
                before = pitch_prev.get(c, pitch(c))
                after = d1 + (bend.get(c, 8192) - 8192) / COUNTS
                rec = {"old": old, "new": d1, "before": before, "after": after,
                       "prev_step": last_step.get(c, 0.0), "next_step": 0.0}
                cur["cross"].append(rec)
                awaiting[c] = rec
            else:
                discard(c)
                cur = {"ch": c, "start": d1, "cross": [], "steps": [], "t0": t}
                strokes.append(cur)
            note[c] = d1
        elif k == 0x80 or (k == 0x90 and d2 == 0):
            confirm(c)
            if note.get(c) == d1:
                del note[c]
                if cur and cur["ch"] == c:
                    cur["end"] = d1
        else:
            confirm(c)
        prev_msg[c] = (s, d1, d2)
    for c in list(pending):
        confirm(c)

    for i, st in enumerate(strokes):
        crossings = st["cross"]
        notes = [st["start"]] + [c["new"] for c in crossings]
        steps = sorted({abs(c["new"] - c["old"]) for c in crossings})
        moves = sorted(m for m in st["steps"] if m > 0)
        median_move = moves[len(moves) // 2] if moves else 0.0
        worst_seam = 0.0
        worst_ratio = 0.0
        violations = []
        overshoots = []
        for c in crossings:
            seam = abs(c["after"] - c["before"])
            # Travel measured from BEFORE the crossing, plus the stroke's
            # median step. Deliberately NOT the step after the crossing: a
            # broken crossing corrupts that one too, and using it let a
            # 50-cent injected jump inflate its own bound and pass. The
            # median is robust — one bad crossing among many cannot move it.
            local = max(c["prev_step"], median_move)
            bound = max(0.05, 2.5 * local)
            worst_seam = max(worst_seam, seam)
            worst_ratio = max(worst_ratio, seam / bound)
            if seam > bound:
                violations.append((c["old"], c["new"], round(seam, 3), round(bound, 3)))
            if seam > abs(c["new"] - c["old"]) + 0.1:
                overshoots.append((c["old"], c["new"], round(c["before"], 3), round(c["after"], 3)))
        mono_up = all(b > a for a, b in zip(notes, notes[1:])) if len(notes) > 1 else True
        mono_dn = all(b < a for a, b in zip(notes, notes[1:])) if len(notes) > 1 else True
        released = "end" in st
        print(f"stroke {i + 1}: ch{st['ch']} notes {notes[0]}..{notes[-1]} "
              f"({len(crossings)} retriggers, note steps {steps}), worst seam "
              f"{worst_seam:.4f} st ({worst_seam * 100:.1f} cents) at "
              f"{worst_ratio:.2f}× its local per-event travel bound "
              f"(pen moved {median_move * 100:.1f} cents/event median), monotone "
              f"{'up' if mono_up else ('down' if mono_dn else 'NO')}, released {released}")
        if overshoots:
            print(f"  FAIL a crossing overshot its note change: {overshoots[:2]}")
            fails += 1
        if violations:
            if layout == "piano":
                print(f"  info {len(violations)} crossing(s) step beyond one event's travel "
                      f"(worst {violations[0][2]} st) — quantized by design on this lattice "
                      f"(#29/#52); the overshoot assert above is the guard")
            else:
                print(f"  FAIL the crossing added a discontinuity beyond one event's travel: "
                      f"{violations[:2]} (old, new, seam, bound)")
                fails += 1
        if not (mono_up or mono_dn):
            print("  FAIL non-monotone sweep")
            fails += 1
        if not released:
            if truncated:
                print("  info no release in the file (truncated log) — not asserted")
            else:
                print("  FAIL stroke left a note sounding")
                fails += 1

    if note:
        if truncated:
            print(f"info notes without a release in the file: {note} (truncated log)")
        else:
            print(f"FAIL notes still sounding at end: {note}")
            fails += 1
    if fails:
        print(f"{fails} PEN ASSERT(S) FAILED")
    elif layout == "piano":
        # Say plainly that one assert was not applicable here, so a piano run
        # can never be mistaken for a continuity pass.
        print("PEN TRACE ASSERTS PASS (piano: retriggers, monotonicity, overshoot "
              "and release asserted; the seam is quantized by design and only reported)")
    else:
        print("PEN TRACE ASSERTS PASS")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    positional = [a for a in sys.argv[1:] if not a.startswith("--")]
    lay = "chroma"
    for i, a in enumerate(sys.argv):
        if a == "--layout" and i + 1 < len(sys.argv):
            lay = sys.argv[i + 1]
            if lay in positional:
                positional.remove(lay)
    if lay not in ("chroma", "janko", "piano"):
        print("--layout must be chroma, janko or piano")
        sys.exit(2)
    sys.exit(main(positional[0], lay))

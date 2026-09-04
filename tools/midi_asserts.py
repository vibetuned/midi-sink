#!/usr/bin/env python3
"""Step-22 evidence asserts over the two byte logs.

  midi_asserts.py device  midi_log.csv      -- the tablet's merge-point log
  midi_asserts.py capture capture.csv [--usb NAME] [--ble NAME]
                                           -- the Linux-side ALSA capture

Device log (midi_log.csv: t,status,d1,d2,src — src 0 external, 1 finger,
2 session config, 3 strip, 4 stylus):
  * every finger/pen Note On is preceded by a bend on its channel (the first
    strike of a voice by CENTER bend — §5.1 emit order);
  * every Note Off is preceded by pressure 0 on its channel, or is the Off
    half of a same-channel legato retrigger (pen, #39);
  * FINGER voices emit no CC74 — ever (#19); pen voices may;
  * strip bytes (src 3) are all on the master channel (§8);
  * sustain never sticks: CC64 127s and 0s balance and the last is 0;
  * channel steal: no finger/pen Note On lands on a channel holding an open
    EXTERNAL note (§5.1 masking);
  * on/off balance per voice, zero open voices at the end, distinct channels
    used, the MCM/RPN0 handshake present and ordered (src 2).

Capture (t_s,port,port_name,status,d1,d2 from tests/midi_capture_alsa.cpp):
  * per port: the MCM handshake (101=0,100=6,6=15 on ch1, then 101=0,100=0,
    6=48 on every member) in wire order; Note Ons preceded by a bend; CC64
    balance; per-(channel,dimension) worst 1 s rate; CC118 lag markers;
  * with --usb/--ble port substrings: matched Note Ons (same channel + note,
    nearest in time) give the per-message arrival lag BLE − USB — the
    "wired beats BLE" number, one clock, no device sync needed.
Exit status 0 when every assert holds.
"""
import argparse
import csv
import itertools
import statistics
import sys
from collections import defaultdict

FAILS = []


def check(cond, msg):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        FAILS.append(msg)


def kind(status):
    return status & 0xF0


def ch(status):
    return status & 0x0F


def read_rows(path, has_src):
    rows = []
    with open(path, newline="") as f:
        r = csv.reader(f)
        first = next(r, None)
        # Only skip the first row when it IS a header — a headerless file must
        # not lose its first message.
        if first is not None and first and first[0].strip() not in ("t", "t_s"):
            r = itertools.chain([first], r)
        for line in r:
            if not line or line[0].startswith("#"):
                continue
            if has_src:
                t, s, d1, d2, src = line[:5]
                rows.append((float(t), int(s), int(d1), int(d2), int(src)))
            else:
                t, port, name, s, d1, d2 = line[:6]
                rows.append((float(t), f"{port} {name}".strip(), int(s), int(d1), int(d2)))
    return rows


def handshake_ok(msgs):
    """msgs: list of (status, d1, d2) on one stream, wire order. Returns
    (found, ordered, members_at_48)."""
    seq = [(s, d1, d2) for (s, d1, d2) in msgs if kind(s) == 0xB0]
    master = [(d1, d2) for (s, d1, d2) in seq if ch(s) == 0]
    found = False
    ordered = True
    for i in range(len(master) - 2):
        if master[i] == (101, 0) and master[i + 1] == (100, 6):
            found = True
            if master[i + 2][0] != 6 or master[i + 2][1] != 15:
                ordered = False
            break
    members = 0
    for c in range(1, 16):
        m = [(d1, d2) for (s, d1, d2) in seq if ch(s) == c]
        for i in range(len(m) - 2):
            if m[i] == (101, 0) and m[i + 1] == (100, 0) and m[i + 2] == (6, 48):
                members += 1
                break
    return found, found and ordered, members


def device(path):
    rows = read_rows(path, True)
    print(f"{len(rows)} messages in {path}")
    # The on-device byte log is a CAPPED ring-less buffer (300 k entries): a
    # long session stops recording partway, and the tail — the releases —
    # never lands. When the writer says it truncated, the balance checks are
    # not assertable here; the WIRE capture is the complete record.
    comments = [l for l in open(path).read().splitlines() if l.startswith("#")]
    truncated = any("truncated" in l for l in comments)
    dropped = None
    for l in comments:
        if "dropped_loopback_messages" in l:
            try:
                dropped = int(l.split(":")[1])
            except (IndexError, ValueError):
                pass
    if dropped is None:
        print("info this log carries no dropped_loopback_messages line "
              "(written since Step 22; older logs predate it)")
    else:
        check(dropped == 0, f"zero dropped loopback messages ({dropped})")
    if truncated:
        print("info the byte log hit its cap — on/off balance is reported, not asserted "
              "(use the wire capture for a full-session balance)")
    by_src = defaultdict(int)
    for r in rows:
        by_src[r[4]] += 1
    print("  by src:", dict(by_src))

    # handshake
    cfg = [(s, d1, d2) for (t, s, d1, d2, src) in rows if src == 2]
    found, ordered, members = handshake_ok(cfg)
    check(found and ordered, f"session config: MCM present and ordered (101=0,100=6,6=15)")
    check(members == 15, f"session config: RPN 0 = 48 on {members}/15 member channels")

    last_on_ch = {}           # ch -> last message (status,d1,d2,src) on that channel (any src)
    ext_open = defaultdict(int)   # ch -> open external notes
    voice_open = {}           # ch -> (note, src_at_on)
    pen_voices = set()        # channels whose current voice began as a pen
    on_count = off_count = 0
    on_preceded_by_bend = 0
    on_preceded_by_center = 0
    strikes = 0
    strikes_centered = 0
    retriggers = 0
    off_preceded_ok = 0
    legato_offs = 0
    steal_violations = 0
    finger_cc74 = 0
    pen_cc74 = 0
    strip_offmaster = 0
    cc64 = []
    channels_used = set()
    prev_same_ch = {}
    for (t, s, d1, d2, src) in rows:
        k = kind(s)
        c = ch(s)
        if src == 0:
            if k == 0x90 and d2 > 0:
                ext_open[c] += 1
            elif k == 0x80 or (k == 0x90 and d2 == 0):
                ext_open[c] = max(0, ext_open[c] - 1)
            prev_same_ch[c] = (s, d1, d2, src)
            continue
        if src == 3:
            if c != 0:
                strip_offmaster += 1
            if k == 0xB0 and d1 == 64:
                cc64.append(d2)
            prev_same_ch[c] = (s, d1, d2, src)
            continue
        if src == 2:
            prev_same_ch[c] = (s, d1, d2, src)
            continue
        # src 1 (finger) / 4 (pen)
        if k == 0x90 and d2 > 0:
            on_count += 1
            channels_used.add(c)
            p = prev_same_ch.get(c)
            # A STRIKE is a Note On on a channel holding no voice: §3.1/§5.1
            # promise it is preceded by a CENTER bend (the in-tune attack). A
            # Note On on a channel that already holds one is a legato
            # retrigger (#39) whose bend is the new cell's offset, not center.
            strike = c not in voice_open
            if strike:
                strikes += 1
            else:
                retriggers += 1
            if p and kind(p[0]) == 0xE0:
                on_preceded_by_bend += 1
                if (p[1] | (p[2] << 7)) == 8192:
                    on_preceded_by_center += 1
                    if strike:
                        strikes_centered += 1
            if ext_open[c] > 0:
                steal_violations += 1
            if c in voice_open and voice_open[c][1] == 4 and src == 4:
                pass   # legato retrigger: the old note's Off follows
            voice_open[c] = (d1, src)
            if src == 4:
                pen_voices.add(c)
            else:
                pen_voices.discard(c)
        elif k == 0x80 or (k == 0x90 and d2 == 0):
            off_count += 1
            if src == 4:
                pen_voices.discard(c)
            p = prev_same_ch.get(c)
            if p and kind(p[0]) == 0xD0 and p[1] == 0:
                off_preceded_ok += 1
            elif p and kind(p[0]) == 0x90 and p[1] != d1:
                legato_offs += 1          # bend -> On(new) -> Off(old)
            elif p and kind(p[0]) == 0xA0 and p[2] == 0:
                off_preceded_ok += 1      # 0xD0 0 -> 0xA0 0 -> Off (bipolar lift)
            if c in voice_open and voice_open[c][0] == d1:
                del voice_open[c]
        elif k == 0xB0 and d1 == 74:
            if src == 4 or c in pen_voices:
                pen_cc74 += 1
            else:
                finger_cc74 += 1
        prev_same_ch[c] = (s, d1, d2, src)

    check(on_count > 0, f"touch/pen Note Ons: {on_count}")
    check(on_preceded_by_bend == on_count,
          f"every Note On preceded by a bend on its channel ({on_preceded_by_bend}/{on_count})")
    check(strikes > 0 and strikes_centered == strikes,
          f"every STRIKE preceded by a CENTER bend ({strikes_centered}/{strikes}; "
          f"{retriggers} legato retriggers carry their cell offset instead)")
    check(off_preceded_ok + legato_offs == off_count,
          f"every Note Off preceded by pressure 0 ({off_preceded_ok}) or a legato retrigger ({legato_offs}) "
          f"of {off_count}")
    check(finger_cc74 == 0, f"zero CC74 from finger voices ({finger_cc74}); pen CC74 = {pen_cc74}")
    check(strip_offmaster == 0, f"strip traffic on the master channel only ({strip_offmaster} off-master)")
    if cc64:
        # The strip announce repeats CC 64 = 0 (exempt, by design), so the
        # counts need not match; the invariant is that every ON is answered by
        # an OFF, so no ON is ever the last word.
        ons = cc64.count(127)
        offs = cc64.count(0)
        unanswered = 0
        held = False
        for v in cc64:
            if v >= 64 and not held:
                held = True
            elif v < 64 and held:
                held = False
        if held:
            unanswered = 1
        check(not held, f"sustain never sticks: CC64 127×{ons} / 0×{offs}, "
                        f"last = {cc64[-1]}, unanswered ON at the end: {unanswered}")
    else:
        print("info sustain: no CC64 traffic in this log")
    check(steal_violations == 0, f"zero touch allocations on externally-held channels ({steal_violations})")
    if truncated:
        print(f"info open touch voices where the log stops: {len(voice_open)} "
              f"(on {on_count} / off {off_count}) — truncation artifact")
    else:
        check(len(voice_open) == 0,
              f"open touch voices at end: {len(voice_open)} (on {on_count} / off {off_count})")
    print(f"info distinct member channels used by touches: {sorted(channels_used)}")
    return 0 if not FAILS else 1


def capture(path, usb_match, ble_match, policy=None):
    rows = read_rows(path, False)
    print(f"{len(rows)} messages in {path}")
    # A capture with nothing in it must never read as a pass: every check
    # below is per-port and per-feature, so zero rows would assert nothing at
    # all (and DECISIONS_3 #51 has us running a throwaway DRAIN capture before
    # every timing run — pointing the tool at that file is one typo away).
    check(len(rows) > 0, f"the capture contains messages ({len(rows)})")
    if not rows:
        return 1
    ports = sorted(set(r[1] for r in rows))
    print("  ports:", ports)
    if usb_match:
        check(any(usb_match in p for p in ports),
              f"a port matching the USB name '{usb_match}' is in the capture")
    if ble_match:
        check(any(ble_match in p for p in ports),
              f"a port matching the BLE name '{ble_match}' is in the capture")
    for port in ports:
        pr = [r for r in rows if r[1] == port]
        msgs = [(s, d1, d2) for (t, p, s, d1, d2) in pr]
        found, ordered, members = handshake_ok(msgs)
        if found or members:
            check(found and ordered, f"[{port}] MCM handshake present and ordered")
            check(members == 15, f"[{port}] RPN 0 = 48 on {members}/15 members")
        else:
            # The handshake is sent at Play-mode entry and when a sink appears
            # (§5.3 / #28), so a capture that starts mid-session legitimately
            # contains none — assert what is in the file, not what is not.
            print(f"info [{port}] no handshake in this capture window "
                  f"(sent at Play-mode entry / on a sink appearing; see the "
                  f"dual-transport capture for the assert)")
        # note-ons preceded by bend (same channel)
        prev = {}
        ons = 0
        ok = 0
        cc64 = []
        slot_times = defaultdict(list)
        for (t, p, s, d1, d2) in pr:
            k = kind(s)
            c = ch(s)
            if k == 0x90 and d2 > 0:
                ons += 1
                q = prev.get(c)
                if q and kind(q[0]) == 0xE0:
                    ok += 1
            if k == 0xB0 and d1 == 64 and c == 0:
                cc64.append(d2)
            dim = None
            if k == 0xE0:
                dim = "bend"
            elif k == 0xD0:
                dim = "press"
            elif k == 0xA0:
                dim = "poly"
            elif k == 0xB0 and (c == 0 or d1 == 74):
                dim = f"cc{d1}"
            if dim:
                slot_times[(c, dim)].append(t)
            prev[c] = (s, d1, d2)
        if ons:
            check(ok == ons, f"[{port}] Note Ons preceded by a bend: {ok}/{ons}")
        if cc64:
            check(cc64.count(127) <= cc64.count(0) and cc64[-1] == 0,
                  f"[{port}] sustain never sticks: CC64 127×{cc64.count(127)} / 0×{cc64.count(0)}, last {cc64[-1]}")
        # Zero stuck notes on the wire: what a DAW is left holding at the end.
        held = {}
        n_on = n_off = 0
        for (t, p2, s2, d1, d2) in pr:
            k2, c2 = s2 & 0xF0, s2 & 0x0F
            if k2 == 0x90 and d2 > 0:
                n_on += 1
                held[(c2, d1)] = held.get((c2, d1), 0) + 1
            elif k2 == 0x80 or (k2 == 0x90 and d2 == 0):
                n_off += 1
                if held.get((c2, d1)):
                    held[(c2, d1)] -= 1
                    if held[(c2, d1)] == 0:
                        del held[(c2, d1)]
        check(not held, f"[{port}] zero notes left sounding at the end "
                        f"({n_on} on / {n_off} off; still held: {sorted(held)})")
        worst = 0.0
        worst_slot = None
        for slot, ts in slot_times.items():
            ts.sort()
            j = 0
            for i in range(len(ts)):
                while ts[i] - ts[j] > 1.0:
                    j += 1
                n = i - j + 1
                if n > worst:
                    worst = n
                    worst_slot = slot
        # §5.3: the per-transport policy is a NUMBER, so assert it when the
        # caller says which policy this port is under. Rate class = ≤100 Hz
        # per (channel, dimension); budget class = a global ceiling with
        # round-robin fairness, which says nothing about any one slot. The
        # tolerance covers a burst straddling the 1 s window edge.
        pol = None
        if policy:
            if usb_match and usb_match in port:
                pol = policy[0]
            elif ble_match and ble_match in port:
                pol = policy[1] if len(policy) > 1 else None
            elif len(policy) == 1:
                pol = policy[0]
        if worst_slot:
            if pol == "rate":
                check(worst <= 110,
                      f"[{port}] per-slot rate under the ≤100 Hz policy: worst {worst:.0f}/s "
                      f"on {worst_slot} ({len(slot_times)} active slots)")
            else:
                print(f"info [{port}] worst per-slot 1 s rate: {worst:.0f}/s on {worst_slot}")
        # BLE's policy is a GLOBAL budget with round-robin fairness, not a
        # per-slot ceiling (DECISIONS_3 #3) — so report both.
        allt = sorted(t for (t, p2, s2, d1, d2) in pr)
        gworst = 0
        j = 0
        for i in range(len(allt)):
            while allt[i] - allt[j] > 1.0:
                j += 1
            gworst = max(gworst, i - j + 1)
        if pol == "budget":
            check(gworst <= 360,
                  f"[{port}] global rate under the ~300 msg/s budget: worst {gworst}/s "
                  f"over {len(pr)} messages")
        else:
            print(f"info [{port}] worst GLOBAL 1 s rate: {gworst}/s over {len(pr)} messages")
        # Fairness (§5.3): every active slot keeps updating — the gap
        # distribution inside the window every slot was active in.
        if len(slot_times) > 1:
            lo = max(min(v) for v in slot_times.values())
            hi = min(max(v) for v in slot_times.values())
            gaps = []
            for slot, ts in slot_times.items():
                ts2 = [t for t in sorted(ts) if lo <= t <= hi]
                gaps.extend(b - a for a, b in zip(ts2, ts2[1:]))
            if gaps:
                gaps.sort()
                print(f"info [{port}] per-slot update gap in the common window: "
                      f"median {gaps[len(gaps)//2]*1000:.0f} ms, "
                      f"p95 {gaps[min(int(len(gaps)*0.95), len(gaps)-1)]*1000:.0f} ms, "
                      f"max {gaps[-1]*1000:.0f} ms ({len(slot_times)} slots)")
        markers = [(t, d2) for (t, p, s, d1, d2) in pr if kind(s) == 0xB0 and ch(s) == 0 and d1 == 118]
        if len(markers) >= 3:
            # The marker counts 0..127 and wraps; unwrap it before differencing
            # or the drift reads as hundreds of seconds on a long session.
            t0, v0 = markers[0]
            n = 0
            prev_v = v0
            drift = []
            for (t, v) in markers:
                if v < prev_v:
                    n += 128
                prev_v = v
                drift.append((t - t0) - ((v + n) - v0))
            print(f"info [{port}] CC118 1 Hz marker drift over {len(markers)} markers: "
                  f"first {drift[1]*1000:+.0f} ms, last {drift[-1]*1000:+.0f} ms (cumulative lag)")
    if usb_match and ble_match:
        usb = [r for r in rows if usb_match in r[1]]
        ble = [r for r in rows if ble_match in r[1]]
        usb_on = [(t, ch(s), d1) for (t, p, s, d1, d2) in usb if kind(s) == 0x90 and d2 > 0]
        ble_on = [(t, ch(s), d1) for (t, p, s, d1, d2) in ble if kind(s) == 0x90 and d2 > 0]
        lags = []
        used = set()
        for (t, c, n) in usb_on:
            best = None
            for j, (tb, cb, nb) in enumerate(ble_on):
                if j in used or cb != c or nb != n:
                    continue
                if best is None or abs(tb - t) < abs(ble_on[best][0] - t):
                    best = j
            if best is not None and abs(ble_on[best][0] - t) < 1.0:
                used.add(best)
                lags.append((ble_on[best][0] - t) * 1000.0)
        if lags:
            lags.sort()
            med = statistics.median(lags)
            print(f"info matched Note Ons USB<->BLE: {len(lags)}; BLE − USB arrival: "
                  f"median {med:.1f} ms, p10 {lags[len(lags)//10]:.1f}, p90 {lags[(len(lags)*9)//10]:.1f}, "
                  f"min {lags[0]:.1f}, max {lags[-1]:.1f}")
            check(med > 0, f"USB arrives before BLE (median lead {med:.1f} ms)")
        else:
            print("info no matching Note Ons across the USB and BLE ports")
    return 0 if not FAILS else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["device", "capture"])
    ap.add_argument("path")
    ap.add_argument("--usb", default=None, help="port substring of the USB gadget capture")
    ap.add_argument("--ble", default=None, help="port substring of the BLE capture")
    ap.add_argument("--policy", default=None,
                    help="assert the §5.3 per-transport policy: 'rate' (≤100 Hz per "
                         "voice-dimension) or 'budget' (~300 msg/s global). With both "
                         "--usb and --ble, pass 'rate,budget' in that order.")
    a = ap.parse_args()
    pol = a.policy.split(",") if a.policy else None
    rc = device(a.path) if a.mode == "device" else capture(a.path, a.usb, a.ble, pol)
    print("ALL ASSERTS PASS" if rc == 0 else f"{len(FAILS)} ASSERT(S) FAILED")
    sys.exit(rc)


if __name__ == "__main__":
    main()

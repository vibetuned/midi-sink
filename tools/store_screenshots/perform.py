#!/usr/bin/env python3
"""A deterministic MPE performance into a MIDI output — for the 2.0 store screenshots.
   perform.py --port <substring>|--virtual <name> --notes lo hi [--seconds 16] [--seed 3]
              [--shot <png> --shot-at <s> --serial <adb serial>] [--sim <udid>]"""
import argparse, random, time, subprocess, rtmidi

ap = argparse.ArgumentParser()
ap.add_argument("--port"); ap.add_argument("--virtual")
ap.add_argument("--notes", nargs=2, type=int, default=[40, 76])
ap.add_argument("--seconds", type=float, default=16.0); ap.add_argument("--seed", type=int, default=3)
ap.add_argument("--shot"); ap.add_argument("--shot-at", type=float, default=13.0)
ap.add_argument("--serial"); ap.add_argument("--sim")
a = ap.parse_args()

out = rtmidi.MidiOut()
if a.virtual:
    out.open_virtual_port(a.virtual); time.sleep(1.5)
else:
    ports = out.get_ports(); idx = next(i for i, p in enumerate(ports) if a.port in p); out.open_port(idx)
def send(*b): out.send_message(list(b))
def bend(ch, semis):  # ±48 member range
    v = int(8192 + semis / 48.0 * 8192); v = max(0, min(16383, v)); send(0xE0 | ch, v & 0x7F, v >> 7)

random.seed(a.seed)
# MPE: lower zone, 15 members; bend range 48 on the members, 2 on the master
send(0xB0, 101, 0); send(0xB0, 100, 6); send(0xB0, 6, 15)
for ch in range(1, 16): send(0xB0 | ch, 101, 0); send(0xB0 | ch, 100, 0); send(0xB0 | ch, 6, 48)
send(0xB0, 101, 0); send(0xB0, 100, 0); send(0xB0, 6, 2)
time.sleep(0.3)

lo, hi = a.notes
voices = {}          # ch -> (note, t_on, hold, vib, press_peak)
t0 = time.time(); next_on = 0.0; shot_done = False; ch_next = 1
step = 0.0
while True:
    t = time.time() - t0
    if t >= a.seconds: break
    if t >= next_on:
        ch = ch_next; ch_next = ch_next % 10 + 1
        if ch in voices: send(0x80 | ch, voices[ch][0], 64); del voices[ch]
        note = random.randint(lo, hi)
        if random.random() < 0.3: note = lo + (random.randint(0, 3) * 5) + random.choice([0, 4, 7, 12])   # a chord-ish shape
        note = max(lo, min(hi, note))
        bend(ch, 0.0); send(0xD0 | ch, 0)
        send(0x90 | ch, note, random.randint(72, 120))
        voices[ch] = (note, t, random.uniform(0.9, 2.2), random.uniform(0.3, 1.5), random.randint(60, 120))
        next_on = t + random.uniform(0.18, 0.42)
        if random.random() < 0.25: send(0xB0 | ch, 74, random.randint(20, 110))
    for ch, (note, ton, hold, vib, pp) in list(voices.items()):
        age = t - ton
        if age > hold: send(0xD0 | ch, 0); send(0x80 | ch, note, 64); del voices[ch]; continue
        p = pp * (age / hold) if age < hold * 0.6 else pp * (1 - (age - hold * 0.6) / (hold * 0.4))
        send(0xD0 | ch, int(max(0, min(127, p))))
        import math
        bend(ch, vib * math.sin(age * 9.0) * min(1.0, age / 0.4))
    if a.shot and not shot_done and t >= a.shot_at:
        if a.serial: subprocess.run(["adb", "-s", a.serial, "exec-out", "screencap", "-p"], stdout=open(a.shot, "wb"))
        elif a.sim: subprocess.run(["xcrun", "simctl", "io", a.sim, "screenshot", a.shot], capture_output=True)
        shot_done = True
    time.sleep(0.02)
for ch, (note, *_rest) in voices.items(): send(0xD0 | ch, 0); send(0x80 | ch, note, 64)
send(0xB0, 123, 0)
print("performance done", f"{a.seconds:.0f} s")

# Tiny SMF type-0 writer: events as (delta_ms, [bytes]); tempo 120 => 500000 us/qn, 480 tpq => 1 tick = 1.0417 ms
import struct, sys
def vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.insert(0, 0x80 | (n & 0x7F)); n >>= 7
    return bytes(out)
def write(path, events):
    trk = b"\x00\xFF\x51\x03" + (500000).to_bytes(3, "big")
    for dms, msg in events:
        trk += vlq(int(round(dms * 480 / 500.0))) + bytes(msg)
    trk += b"\x00\xFF\x2F\x00"
    data = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(trk)) + trk
    open(path, "wb").write(data)
ON, OFF, CC, BEND, CP = 0x90, 0x80, 0xB0, 0xE0, 0xD0
def bend(ch, v14): return [BEND | ch, v14 & 0x7F, (v14 >> 7) & 0x7F]
# 1. MPE default, plain channel-1 keyboard: a chord (one drop per note), CC 64 on/off (must do nothing)
ev = [(0, [ON, 60, 100]), (0, [ON, 64, 100]), (0, [ON, 67, 100]), (1500, [CC, 64, 127]), (800, [CC, 64, 0]),
      (800, [OFF, 60, 0]), (0, [OFF, 64, 0]), (0, [OFF, 67, 0]), (300, [CC, 123, 0])]
write(sys.argv[1] + "/mpe_ch1_chord_cc64.mid", ev)
# 2. Classic: the same chord (per-note voices) then a channel-1 bend sweep (global shear), CC 64 on/off
ev = [(0, [ON, 60, 100]), (0, [ON, 64, 100]), (0, [ON, 67, 100])]
for i in range(0, 41):
    ev.append((40, bend(0, 8192 + int(6000 * (i / 40.0)))))
ev += [(300, bend(0, 8192)), (300, [CC, 64, 127]), (500, [CC, 64, 0]), (500, [OFF, 60, 0]), (0, [OFF, 64, 0]), (0, [OFF, 67, 0])]
write(sys.argv[1] + "/classic_chord_bend_cc64.mid", ev)
# 3. Wind: one voice, breath CC 2 ramps the drop, legato to the next note (wake), CC 64 on/off
ev = [(0, [CC, 2, 10]), (0, [ON, 60, 90])]
for i in range(1, 31): ev.append((50, [CC, 2, min(127, 10 + 4 * i)]))
ev += [(300, [ON, 67, 90]), (100, [OFF, 60, 0])]          # legato: new note before the old one ends
for i in range(0, 20): ev.append((50, [CC, 2, max(20, 127 - 5 * i)]))
ev += [(300, [ON, 72, 90]), (100, [OFF, 67, 0]), (800, [CC, 64, 127]), (400, [CC, 64, 0]), (500, [OFF, 72, 0]), (200, [CC, 2, 0])]
write(sys.argv[1] + "/wind_breath_legato_cc64.mid", ev)
# 4. Ripple handles: CC 102 amount / 103 wavelength (the settings' route), then Airwave Tilt R (29) = amount per #69
ev = [(0, [CC, 102, 100]), (0, [CC, 103, 40]), (1500, [CC, 102, 0]), (500, [CC, 29, 110]), (1500, [CC, 29, 0])]
write(sys.argv[1] + "/ripple_cc.mid", ev)
print("written")

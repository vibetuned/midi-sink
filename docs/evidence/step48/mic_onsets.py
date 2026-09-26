# Proof-of-sound analysis of the Mac's microphone recording taken beside the
# Tab during the spike: RMS per 10 ms window, tone onsets = the RMS rising
# above 4x the recording's noise floor after a quiet stretch. Pure Python.
import struct, sys, wave, statistics
path = sys.argv[1] if len(sys.argv) > 1 else "spike_mic.wav"
w = wave.open(path, "rb")
n, sr, sw, ch = w.getnframes(), w.getframerate(), w.getsampwidth(), w.getnchannels()
raw = w.readframes(n)
if sw == 2:
    s = struct.unpack("<%dh" % (n * ch), raw); scale = 32768.0
elif sw == 4:
    s = struct.unpack("<%di" % (n * ch), raw); scale = 2147483648.0
else:
    raise SystemExit(f"unsupported sample width {sw}")
mono = s[::ch]
win = sr // 100
rms = []
for i in range(0, len(mono) - win, win):
    seg = mono[i:i + win]
    rms.append((sum(x * x for x in seg) / win) ** 0.5 / scale)
floor = statistics.median(sorted(rms)[: len(rms) // 4]) or 1e-6
thr = 4.0 * floor
onsets = []
quiet = 0
for i, v in enumerate(rms):
    if v > thr and quiet >= 5:
        onsets.append(i / 100.0)
    quiet = quiet + 1 if v <= thr else 0
peak = max(rms)
print(f"{path}: {n / sr:.1f} s at {sr} Hz; noise floor {floor:.5f}, threshold {thr:.5f}, peak {peak:.4f}; {len(onsets)} tone onsets")
print("onsets (s): " + ", ".join(f"{t:.2f}" for t in onsets))

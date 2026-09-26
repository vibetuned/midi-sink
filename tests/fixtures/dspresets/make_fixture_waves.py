#!/usr/bin/env python3
"""The step-51 fixture samples (pure Python, PCM 16 at 8 kHz), committed
beside the presets that use them; rerun to regenerate:
  k_half.wav / k_quarter.wav — constant levels 0.5 / 0.25 (level arithmetic)
  pad3s.wav                  — 3 s of a 220 Hz sine (the loop that must hold 30 s)
  saw.wav                    — 2 s of a 110 Hz sawtooth (the filter's material)"""
import math, os, struct, wave
here = os.path.dirname(os.path.abspath(__file__))
def write(path, samples, rate=8000):
    w = wave.open(path, "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)); w.close()
os.makedirs(os.path.join(here, "levels", "Samples"), exist_ok=True)
write(os.path.join(here, "levels", "Samples", "k_half.wav"), [0.5] * 4000)
write(os.path.join(here, "levels", "Samples", "k_quarter.wav"), [0.25] * 4000)
os.makedirs(os.path.join(here, "loop", "Samples"), exist_ok=True)
write(os.path.join(here, "loop", "Samples", "pad3s.wav"), [0.8 * math.sin(2 * math.pi * 220 * i / 8000) for i in range(24000)])
os.makedirs(os.path.join(here, "filter", "Samples"), exist_ok=True)
write(os.path.join(here, "filter", "Samples", "saw.wav"), [0.8 * (2.0 * ((i * 110 / 8000) % 1.0) - 1.0) for i in range(16000)])
print("fixture waves written")

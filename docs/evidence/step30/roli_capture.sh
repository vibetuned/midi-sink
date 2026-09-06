#!/usr/bin/env bash
# ROLI-over-ALSA byte capture for the Step 30 DONE line — a human plays.
# Run from the repo root on the Linux box with the ROLI plugged in:
#     docs/evidence/step30/roli_capture.sh [seconds]
# Captures every ALSA sequencer port matching "ROLI" into
# docs/evidence/step30/roli_capture.csv while you play (the app may run at
# the same time — the sequencer fans out to every subscriber), then prints
# the byte summary and tries tools/midi_asserts.py capture (informational
# here: its MCM-handshake assert describes the TABLET's stream, the ROLI
# configures MPE its own way).
set -euo pipefail
SECS="${1:-20}"; OUT="docs/evidence/step30/roli_capture.csv"
test -x build/tests/midi_capture_alsa || { echo "build the tests first: cmake --build build -j6"; exit 1; }
amidi -l | grep -q ROLI || { echo "no ROLI on ALSA (amidi -l)"; exit 1; }
echo "== capturing ROLI for $SECS s — PLAY NOW (strike, press, glide, slide) =="
build/tests/midi_capture_alsa --seconds "$SECS" --match ROLI --out "$OUT"
python3 - "$OUT" <<'PY'
import csv, sys, collections
rows = list(csv.DictReader(open(sys.argv[1])))
kinds = collections.Counter()
ports = collections.Counter()
for r in rows:
    s = int(r["status"]); ports[r["port_name"]] += 1
    k = {0x90: "note-on", 0x80: "note-off", 0xA0: "poly-pressure", 0xB0: "cc", 0xD0: "channel-pressure", 0xE0: "pitch-bend"}.get(s & 0xF0, hex(s))
    kinds[k] += 1
print(f"{len(rows)} channel messages from {dict(ports)}")
for k, n in kinds.most_common(): print(f"  {k:17s} {n}")
ok = kinds["note-on"] > 0 and (kinds["channel-pressure"] + kinds["poly-pressure"]) > 0 and kinds["pitch-bend"] > 0
print("MPE byte evidence:", "PASS (notes, pressure and bend all present)" if ok else "INCOMPLETE — play more (need note-on, pressure and bend)")
sys.exit(0 if ok else 1)
PY
python3 tools/midi_asserts.py capture "$OUT" --usb ROLI --policy rate || echo "(midi_asserts capture is informational for the ROLI — see the header)"

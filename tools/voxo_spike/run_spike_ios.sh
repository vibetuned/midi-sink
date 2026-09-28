#!/bin/zsh
# Step 48b orchestration: install on the iPad, launch with the spike argument,
# record the Mac's mic as proof of sound, wait for the spike, pull the CSV.
set -u
OUT=/private/tmp/claude-502/-Users-osf-pprojects-midi-sink/255f1472-4224-4afa-842e-f25f47fddc1b/scratchpad/spike
APP=/Users/osf/pprojects/midi-sink/ios/build-dd/Build/Products/Debug-iphoneos/midi-sink.app
ID=00008132-000224C00160C01C
BID=com.vibetuned.midi-sink
cd $OUT
xcrun devicectl device install app --device $ID $APP 2>&1 | tail -1
T0=$(date +%s)
xcrun devicectl device process launch --terminate-existing --console --device $ID $BID -- --voxo-spike 40 > ios_console.log 2>&1 &
LP=$!
sleep 8
ffmpeg -y -loglevel error -f avfoundation -i ":1" -t 26 -ac 1 -ar 48000 spike_mic_ios.wav &
FF=$!
for i in {1..90}; do
  if grep -q VOXO_SPIKE_DONE ios_console.log 2>/dev/null; then break; fi
  sleep 1
done
echo "spike done at t=$(( $(date +%s) - T0 ))s"
wait $FF 2>/dev/null
kill $LP 2>/dev/null
rm -f voxo_spike_ios.csv
xcrun devicectl device copy from --device $ID --domain-type appDataContainer --domain-identifier $BID --source Documents/voxo_spike.csv --destination voxo_spike_ios.csv 2>&1 | tail -1
echo "=== csv ($(wc -l < voxo_spike_ios.csv 2>/dev/null) lines) ==="
grep "^#" voxo_spike_ios.csv
python3 - <<'PY'
import csv, statistics
rows = [r for r in csv.reader(open("voxo_spike_ios.csv")) if r and not r[0].startswith("#") and r[0] != "kind"]
for kind in ("push", "touch"):
    d = [float(r[4]) for r in rows if r[0] == kind and r[4] != "nan"]
    if d:
        d.sort()
        print(f"{kind}: n={len(d)} median {statistics.median(d):.2f} ms  p90 {d[int(len(d)*0.9)-1]:.2f}  min {d[0]:.2f}  max {d[-1]:.2f}")
    else:
        print(f"{kind}: no pairs")
PY
grep -i "voxo\|storm" ios_console.log | head -12
ls -la spike_mic_ios.wav

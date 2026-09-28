#!/bin/zsh
# Step 48a orchestration: install, start the app in Play mode with the spike,
# record the Mac's mic as proof of sound, inject touches, add the visual storm
# as load, wait for the spike, pull the CSV.
set -u
OUT=/private/tmp/claude-502/-Users-osf-pprojects-midi-sink/255f1472-4224-4afa-842e-f25f47fddc1b/scratchpad/spike
APK=/Users/osf/pprojects/midi-sink/android/app/build/outputs/apk/debug/app-debug.apk
PKG=com.vibetuned.midisink
cd $OUT
adb install -r $APK 2>&1 | tail -1
adb shell am force-stop $PKG
adb shell media volume --stream 3 --set 9 >/dev/null 2>&1
adb logcat -c
T0=$(date +%s)
adb shell am start -n $PKG/.MainActivity --es playMode 1 --ei voxoSpike 40 | tail -1
sleep 7
# 26 s of the Mac's microphone while the pushes end and the taps play (proof of sound)
ffmpeg -y -loglevel error -f avfoundation -i ":1" -t 26 -ac 1 -ar 48000 spike_mic.wav &
FF=$!
sleep 3
for i in {1..20}; do
  x=$((800 + i * 60)); y=$((700 + (i % 4) * 100))
  adb shell input swipe $x $y $x $y 250
  sleep 0.75
done
echo "taps injected at t=$(( $(date +%s) - T0 ))s"
adb shell am start -n $PKG/.MainActivity --ei stormSeconds 12 >/dev/null
for i in {1..60}; do
  if adb logcat -d -s sumi-shell:I 2>/dev/null | grep -q VOXO_SPIKE_DONE; then break; fi
  sleep 1
done
echo "spike done at t=$(( $(date +%s) - T0 ))s"
wait $FF 2>/dev/null
adb exec-out run-as $PKG cat files/voxo_spike.csv > voxo_spike.csv
adb logcat -d -s sumi-shell:I sumi-shell:E 2>/dev/null | grep -i "voxo\|storm\|play effective" > spike_logcat.txt
adb shell media volume --stream 3 --set 3 >/dev/null 2>&1
adb shell am force-stop $PKG
echo "=== csv ($(wc -l < voxo_spike.csv) lines) ==="
grep "^#" voxo_spike.csv
python3 - <<'PY'
import csv, statistics, math
rows = [r for r in csv.reader(open("voxo_spike.csv")) if r and not r[0].startswith("#") and r[0] != "kind"]
for kind in ("push", "touch"):
    d = [float(r[4]) for r in rows if r[0] == kind and r[4] != "nan"]
    if d:
        d.sort()
        print(f"{kind}: n={len(d)} median {statistics.median(d):.2f} ms  p90 {d[int(len(d)*0.9)-1]:.2f}  min {d[0]:.2f}  max {d[-1]:.2f}")
    else:
        print(f"{kind}: no pairs")
PY
ls -la spike_mic.wav

#!/bin/zsh
# Step 54 evidence on the Tab: install, first launch (the demo), the gate on the
# harp library over a 10 MB advice, then the touch-to-sound spike with the demo.
set -u
OUT=/private/tmp/claude-502/-Users-osf-pprojects-midi-sink/255f1472-4224-4afa-842e-f25f47fddc1b/scratchpad/spike
APK=/Users/osf/pprojects/midi-sink/android/app/build/outputs/apk/debug/app-debug.apk
PKG=com.vibetuned.midisink
HARP="/Users/osf/pprojects/tuned-samples/instruments/ConcertHarp-SF2-20200702/Arpa Chiquitana MPE.dslibrary"
cd $OUT
adb install -r $APK 2>&1 | tail -1
adb shell am force-stop $PKG
adb logcat -c
echo "=== first launch: the demo"
adb shell am start -n $PKG/.MainActivity --es voxoInstrument demo >/dev/null
sleep 8
adb logcat -d -s sumi-shell:I sumi-shell:E sumi-shell:W 2>/dev/null | grep -i "voxo\|demo instrument\|instruments in\|audio focus" | head -12
echo "=== the harp library into Instruments"
adb push "$HARP" /data/local/tmp/harp.dslibrary >/dev/null
adb shell "run-as $PKG mkdir -p files/Instruments && run-as $PKG cp /data/local/tmp/harp.dslibrary 'files/Instruments/Arpa Chiquitana MPE.dslibrary' && run-as $PKG ls files/Instruments"
adb shell am force-stop $PKG
adb logcat -c
# The name carries spaces: quote it for the DEVICE shell too (adb shell passes the line unquoted).
adb shell "am start -n $PKG/.MainActivity --ei voxoBudgetMb 10 --es voxoInstrument 'Arpa Chiquitana MPE.dslibrary'" >/dev/null
sleep 10
adb logcat -d -s sumi-shell:I 2>/dev/null | grep -A3 "\[voxo\] Arpa" | head -8
echo "=== the spike with the demo (touch-to-sound)"
adb shell am force-stop $PKG
adb shell am start -n $PKG/.MainActivity --es voxoInstrument demo >/dev/null
sleep 4
adb shell am force-stop $PKG
adb logcat -c
adb shell media volume --stream 3 --set 9 >/dev/null 2>&1
adb shell am start -n $PKG/.MainActivity --es playMode 1 --ei voxoSpike 30 >/dev/null
sleep 10
for i in {1..20}; do
  x=$((800 + i * 60)); y=$((700 + (i % 4) * 100))
  adb shell input swipe $x $y $x $y 250
  sleep 0.75
done
for i in {1..60}; do
  if adb logcat -d -s sumi-shell:I 2>/dev/null | grep -q VOXO_SPIKE_DONE; then break; fi
  sleep 1
done
adb exec-out run-as $PKG cat files/voxo_spike.csv > voxo_spike_54.csv
adb shell media volume --stream 3 --set 3 >/dev/null 2>&1
grep "^#" voxo_spike_54.csv
python3 - <<'PY'
import csv, statistics
rows = [r for r in csv.reader(open("voxo_spike_54.csv")) if r and not r[0].startswith("#") and r[0] != "kind"]
for kind in ("push", "touch"):
    d = sorted(float(r[4]) for r in rows if r[0] == kind and r[4] != "nan")
    print(f"{kind}: n={len(d)} median {statistics.median(d):.2f} ms  p90 {d[int(len(d)*0.9)-1]:.2f}  max {d[-1]:.2f}" if d else f"{kind}: no pairs")
PY
adb shell am force-stop $PKG
adb shell am start -n $PKG/.MainActivity --es voxoInstrument demo >/dev/null
sleep 3
adb shell am force-stop $PKG
echo "=== done"

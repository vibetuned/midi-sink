# The tablets' Voxo spike (Phase 7 steps 48 and 54)

The runners that measured touch-to-sound on the Galaxy Tab and the iPad, kept
from the steps' evidence (DECISIONS_6 #7–#9, #28): each installs the current
build, launches the app with the spike hook (`--ei voxoSpike <s>` on Android,
`--voxo-spike <s>` on iOS), injects touches where it can (`adb shell input
swipe`; the iPad takes the author's finger), records the Mac's microphone as
proof of sound (`ffmpeg`, avfoundation input 1 = the built-in mic), adds the
visual storm as load, and pulls `voxo_spike.csv` — push → callback and
touch-down → callback pairs on the one clock, the device's burst, buffer,
underruns and timestamp-derived output latency. `mic_onsets.py` counts tone
onsets in the recording. `run_step54_android.sh` is the step-54 pass: the
demo at first launch, the gate on a pushed library, the spike with the demo
playing. Paths inside point at this Mac's scratch folders; edit before use.

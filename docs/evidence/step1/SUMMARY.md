# Step 1 DONE evidence — skeleton, build system, black window

Date: 2026-08-31. Hardware: Apple Silicon (arm64), macOS 25.6.0, Xcode 26.6.
Build: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build`.

| DONE check | Result | Evidence |
|---|---|---|
| App opens and clears at 60 fps | **PASS** — 499 frames / 5.02 s = 99.5 fps (vsync on a 100/120 Hz ProMotion display, i.e. > 60) | `run_validation_resize.log` |
| Live resize, no Metal validation warnings (`METAL_DEVICE_WRAPPER_TYPE=1`) | **PASS** — `--resize-test` resized 1280×720 → 900×500 → 1440×900 logical (1800×1000 / 2880×1800 px at 2× retina); validation enabled, zero validation messages, zero asserts | `run_validation_resize.log` |
| Clean `sumi_destroy` on close, no leaks | **PASS** — `leaks --atExit` (Instruments' malloc introspection, scriptable; see DECISIONS.md #9): 287 leaks / 18 720 bytes, **all** rooted in Foundation `NSXPCConnection` cycles (`LNDaemonApplicationInterface` daemon connections AppKit opens at startup); zero frames from sumi/sokol/Metal in any stack; totals byte-identical between 4 s and 12 s runs → static OS startup noise, no per-frame growth | `leaks_atexit.log` |
| `sumi_version()` returns 0.1.0 | **PASS** — harness prints `sumi_version: 0.1.0`; test checks `(0<<16)\|(1<<8)\|0` | `run_validation_resize.log`, `ctest_abi_c_compile.log` |
| C11 compile test passes | **PASS** — `abi_c_compile.c` compiled as strict C11 (`-Wall -Wextra -Wpedantic`, extensions off), linked against **libsumi.dylib**, all 18 ABI symbols resolved, `sumi_create(NULL)` → NULL | `ctest_abi_c_compile.log` |

Also proven in this step:
- sokol-shdc (pinned sokol-tools-bin `11d0cf6`) downloads at configure time and
  cross-compiles `placeholder.glsl` → `metal_macos:hlsl5:glsl410:glsl300es` at
  build time.
- Static (`libsumi.a`, consumed by the desktop harness) and shared
  (`libsumi.dylib`, consumed by the ABI test) both build from one object set.

Screenshot: `window_indigo.png` — the running "Suminagashi" window cleared to
deep indigo, captured window-only via `screencapture -x -o -l <window-id>`
(window ID resolved with a CGWindowList helper while the app ran; required
granting VS Code the Screen Recording TCC permission).

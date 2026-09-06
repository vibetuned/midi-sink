# Evidence — Step 30: Linux release lane

PHASE5 §3; ROADMAP_4 Step 30. Decisions: `_work/DECISIONS_4.md` #44–#46.
Machine: the author's Linux box (Ubuntu 25.10, GNOME on Wayland, NVIDIA RTX
5090 / GL 4.1, glibc 2.42; docker without sudo). No core change. The runner
itself has not run this lane yet — the DONE table says which cells wait for
the `v0.5.0-rc.5` tag.

## What landed

* **`cmake/LinuxPackaging.cmake`** (included by the root list on Linux) —
  CPack DEB of the `desktop-integration` component only
  (`CPACK_DEB_COMPONENT_INSTALL` + `ALL_COMPONENTS_IN_ONE`): exactly eleven
  files — `/usr/bin/midi-sink`, the `.desktop` with absolute `Exec`/`TryExec`,
  seven hicolor icons, `copyright`, and a generated Debian `changelog.gz`
  (lintian's one error for a native package); a CPack pre-build script
  normalizes permissions to 0755/0644 regardless of the host umask. Package version `0.5.0~rc.N` from tag
  `0.5.0-rc.N`, file name `midi-sink_<tag>_amd64.deb` (#44). Depends lists
  the `dlopen`ed X11/Wayland/EGL/xkbcommon/libdecor/ALSA libraries by hand
  on top of shlibdeps. `desktop/CMakeLists.txt` install rules made
  DESTDIR-safe (no maintainer scripts; dpkg triggers refresh the caches).
* **`linux` job in `release.yml`** — `needs: [version, gates]`,
  `ubuntu-22.04` + Kitware CMake + gcc-12 (glibc 2.34 floor), configure with
  `-DSUMI_APP_VERSION=<tag>`, `--version` check, `cpack -G DEB`, the bare
  binary tarball `midi-sink-<v>-linux-x64.tar.gz`, package inspection (eleven
  files, `Exec=/usr/bin/midi-sink`, `desktop-file-validate`, lintian
  advisory), install/run/remove in a clean `ubuntu:22.04` container,
  sha256s, `dist-linux`. `publish` now needs `linux`.
* **`publish-apt.yml`** on `release: published` — rebuilds the Pages tree
  from the tag (site + the release's web tarball under `/marble/`) and adds
  `/apt/` with `stable` (releases) and `rc` (pre-releases) suites signed by
  `APT_GPG_PRIVATE_KEY`, public key at `/apt/midi-sink.asc`; skips cleanly
  without the secret (#45).
* Install page (`site/src/content/docs/guide/install.md`) and README: the
  keyring + `deb [signed-by=…] https://midi-sink.vibetuned.com/apt stable
  main` instructions, the `rc` suite for testers, the tarball's caveats.
* **Flatpak spike** — `packaging/linux/flatpak/com.vibetuned.midi-sink.yml`,
  built and run here; verdict closed (#46): ALSA MIDI needs `--device=all`
  (without it the sandboxed app sees no MIDI port at all), FetchContent needs
  network at build time, the desktop entry/icon are renamed to the app id.
  No publish hook.

## Verified here

| Check | Result | Evidence |
|---|---|---|
| Settings window + `--dev` on **Wayland** | **PASS** — GLFW picks Wayland (`WAYLAND_DISPLAY` set), second window created, ImGui settings render, `--exit-after` ends cleanly; the "does not support setting the window position" lines are GLFW's expected Wayland notices | `run_wayland.log` |
| Settings window + `--dev` on **X11** (Xwayland) | **PASS** — `env -u WAYLAND_DISPLAY XDG_SESSION_TYPE=x11`: both windows on X (`xwininfo` lists `midi-sink` 1280×720 and `midi-sink — Settings` 560×760), settings placed beside the canvas; screenshot of the settings window | `run_x11.log`, `x11_settings.png` |
| §4.6 field gate on the real GPU, reference tier | **PASS** — mean 6.85e-6, max 3.9e-3 (≤ 1e-2 / 1e-4); negative control rejected. Not bitwise — the handoff's claim is corrected in #44 | `gate_gl_local.txt` |
| 22.04 toolchain for the lane | **PASS** — `ubuntu:22.04` container: Kitware CMake + gcc-12 configure and build this tree; 22.04's cmake 3.22 / gcc 11 refused | `jammy_probe.sh`, `jammy_probe.log` |
| The deb is only the desktop-integration component | **PASS** — 11 files, 0755/0644 throughout, control fields, Depends complete (shlibdeps + the dlopen list), `Version: 0.5.0~rc.4~…` from describe `0.5.0-rc.4-…` | `deb_control.txt`, `deb_contents.txt` |
| lintian | **PASS** — one remaining tag, `W: no-manual-page` (not taken) | `lintian.txt` |
| `.desktop` integration intact | **PASS** — `desktop-file-validate` OK on the entry extracted from the deb; `Exec=/usr/bin/midi-sink`, `TryExec`, `Icon=midi-sink`, `Categories=AudioVideo;Audio;`; 256×256 icon present in the container | `desktop_validate.txt`, `container_roli.log` |
| Clean Ubuntu install + **ROLI over ALSA** | **PASS** (clean `ubuntu:24.04` container, `/dev/snd` + `/dev/dri` + the host's Wayland socket): `apt-get install ./midi-sink_…deb` resolves every dependency, `amidi -l` shows `ROLI Piano MIDI 1`, `midi-sink --dev --exit-after 6` opens **`ROLI Piano MIDI 1`** (and the tablet's USB port) over ALSA, renders 966 frames on Mesa (no NVIDIA userspace in the container → llvmpipe; on the host the same binary uses the RTX), exits 0 | `container_roli.log` |
| `apt install midi-sink` from the repository shape `publish-apt` builds | **PASS** — throwaway key, the identical `apt-ftparchive`/`gpg` commands, served over http; a clean `ubuntu:24.04` adds the keyring + sources line from the docs, `apt update` with no signature warning, candidate `0.5.0~rc.4~…` from `stable/main`, installs, `--version` runs, `.desktop` present | `aptlocal.sh`, `aptlocal.log` |
| Workflows parse | **PASS** — `release.yml` jobs `version, gates, web, macos, windows, linux, publish`; `linux` needs `[version, gates]`, `publish` needs all lanes; `publish-apt.yml` on `release` | (PyYAML load) |

## DONE table (roadmap line) — what remains is the tag and two human minutes

| Cell | State | Proof / what to do |
|---|---|---|
| A test tag yields the Linux artifacts | **YOURS** — push `v0.5.0-rc.5` (rc.4 exists) after a `dry_run` of the release workflow; `dist-linux` must hold `midi-sink_0.5.0-rc.5_amd64.deb` + `midi-sink-0.5.0-rc.5-linux-x64.tar.gz`; the lane's own container test is in the job log | lane log |
| `apt install midi-sink` on a clean Ubuntu | **PARTIAL** — proven locally against the same repository layout (`aptlocal.log`); the real `https://midi-sink.vibetuned.com/apt` exists after you **publish** the rc.5 draft (pre-release → `rc` suite: `deb […] https://midi-sink.vibetuned.com/apt rc main`) | `publish-apt` run, then `apt update && apt install midi-sink` in any clean 22.04+/Debian 12+ |
| ROLI playable over ALSA from the installed deb | **PASS** — the installed binary opens `ROLI Piano MIDI 1` in a clean container (`container_roli.log`); the author played for 18 s: 1728 channel messages on channels 2–16 — 93 note-ons, 915 channel-pressure, 635 pitch-bend (MPE per-note bend and pressure, as the ROLI sends them) | `roli_capture.csv`, `roli_capture_summary.txt`, `roli_capture.sh` |
| Screenshot of the running app | **PARTIAL** — X11: `x11_settings.png`. Wayland: on this GNOME (Ubuntu 25.10) every programmatic route is closed to a non-allowlisted caller — `org.gnome.Shell.Screenshot` (AccessDenied), `gnome-screenshot` (falls back to X11 and captures nothing), the `org.freedesktop.portal.Screenshot` non-interactive call (response 2). The Shell's own UI works: press **Print** with the app running, pick the window, the file lands in `~/Pictures/Screenshots` — copy it here as `wayland.png` | — |
| `.desktop` / icon integration intact | **PASS** | `desktop_validate.txt`, `container_roli.log` |
| Flatpak verdict recorded | **PASS** (#46) — built with `flatpak-builder` against freedesktop 24.08, installed as `com.vibetuned.midi-sink` (exported `.desktop` + icon); `flatpak run … --version` prints the version; `--dev --exit-after 6` renders on the NVIDIA GL and, with `--device=all`, opens every ALSA port the host has; with `--nodevice=all` no MIDI port opens. Verdict: closed, the deb/apt/tarball are the channels. Remove with `flatpak uninstall --user com.vibetuned.midi-sink` | `flatpak_build_tail.log`, `flatpak_run.log`, `flatpak_run_nodev.log`, `flatpak_permissions.txt` |

## Found on the way

* `CPACK_COMPONENTS_ALL` alone does not filter a monolithic DEB: the first
  package carried ~190 GLFW/libremidi/readerwriterqueue headers, static
  libraries and CMake config files. Component-install mode with one group is
  what restricts it.
* Step 14's `install(CODE)` rules wrote through `CMAKE_INSTALL_PREFIX`
  ignoring `DESTDIR`, and ran `gtk-update-icon-cache` on the packaging host:
  fixed (the `.desktop` is generated under `$ENV{DESTDIR}`, the cache refresh
  returns when staging). The `Exec` inside the file stays `/usr/bin/midi-sink`.
* shlibdeps misses everything GLFW and libremidi `dlopen`: without the hand
  list the deb installs and then fails at `glfwInit`/ALSA on a minimal
  system. The container test exists to catch exactly this.
* Unsetting `WAYLAND_DISPLAY` does not force X11 — libwayland falls back to
  the `wayland-0` socket; `XDG_SESSION_TYPE=x11` with it unset does.
* A deb built on this box needs glibc 2.38 (24.04+); the lane's 22.04 build
  needs 2.34 — the reason for the runner choice (#44).
* **ROLI Airwave does not work on Linux** (author's information; `lsusb`
  shows only the Piano here): the Airwave's tracking is turned into MIDI by
  ROLI's macOS/Windows host software, so on Linux it never appears as a MIDI
  device. Recorded on the site's devices and install pages; nothing in the
  package can change it.
* `gnome-screenshot` from a terminal no longer captures on this GNOME (see the
  DONE table); the Shell's Print-key UI is the way.
* The first flatpak-builder run failed at its debuginfo step (`eu-strip`
  missing — elfutils); `no-debuginfo: true` in the manifest for the spike.
* The handoff's "next decision number 39" was stale (the Windows lane took
  #39–#43) and so was "rc.4": entries start at #44, the next tag is rc.5.

## Not taken

* No maintainer scripts (postinst) — dpkg file triggers own the icon cache
  and the desktop database.
* No `lintian` gate: advisory in the lane (warnings printed, never red).
* No `linux-arm64` — the runners' Linux arm64 image is not in the LANE
  INTERFACE; the interface names amd64 only.
* No signing of the deb itself; the repository metadata is signed
  (`InRelease`), which is what `apt` verifies.
* Flatpak publish hook / Flathub submission (#46).

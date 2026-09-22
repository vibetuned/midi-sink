#!/usr/bin/env bash
# coremidi_recover.sh - diagnose and reset a wedged CoreMIDI setup store (macOS).
#
# Symptom: MIDI inputs vanish mid-session, and keep vanishing every couple of
# minutes. The cause is a NULL-pointer crash inside Apple's own MIDIServer as
# it saves the MIDI setup:
#
#   SetupManager::PrefSaverTimerCallback
#     -> MIDISetup::CheckWritePrefFile
#       -> WriteFileFromCFData(path, NULL)
#         -> CFDataGetLength(NULL)            SIGSEGV, read at 0x0
#
# The setup serializer returns NULL and the result is never checked before
# being dereferenced. That save timer is armed by device-list changes, so it
# tends to fire when something is plugged in or unplugged. Once the loop
# starts every relaunch dies ~3 s in, taking every client's endpoints with it.
# Discarding the persisted setup breaks the loop; CoreMIDI rebuilds it.
#
# Nothing here needs sudo - MIDIServer is a per-user LaunchAgent, and both
# files belong to you.
#
# Usage:
#   tools/coremidi_recover.sh              diagnose only, no changes (default)
#   tools/coremidi_recover.sh --fix        back up, reset, restart, verify
#   tools/coremidi_recover.sh --fix -y     the same, without the prompt
#
# Exit: 0 healthy or repaired, 1 crash loop looks LIVE right now (--check),
# 2 repair failed verification, 3 usage/platform error.
#
# Past crashes alone are history, not a fault: the verdict only reports LIVE
# when one landed inside the last LIVE_WINDOW minutes.

set -uo pipefail
shopt -s nullglob

# Overridable only so the verdict logic can be tested against a fake report dir.
readonly CRASH_DIR="${COREMIDI_CRASH_DIR:-$HOME/Library/Logs/DiagnosticReports}"
readonly MCFG_DIR="$HOME/Library/Audio/MIDI Configurations"
readonly BYHOST_DIR="$HOME/Library/Preferences/ByHost"
readonly SIGNATURE='CheckWritePrefFile'     # the frame that identifies this bug
readonly AMS='Audio MIDI Setup'
readonly BACKUP_ROOT="$HOME/Library/Application Support/coremidi-recover"
readonly SETTLE=14                          # seconds to watch; crash lands at ~3
readonly LIVE_WINDOW=15                     # minutes; a crash newer than this = live loop

MODE=check
ASSUME_YES=0

die()  { printf 'error: %s\n' "$*" >&2; exit 3; }
note() { printf '  %s\n' "$*"; }
head2(){ printf '\n== %s ==\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --fix)          MODE=fix ;;
    --check)        MODE=check ;;
    -y|--yes)       ASSUME_YES=1 ;;
    -h|--help)      sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)              die "unknown argument: $1 (try --help)" ;;
  esac
  shift
done

[ "$(uname -s)" = Darwin ] || die "macOS only (this is $(uname -s))"

setup_files() {
  printf '%s\n' "$BYHOST_DIR/"com.apple.MIDI.*.plist "$MCFG_DIR/"*.mcfg
}

crash_reports() { printf '%s\n' "$CRASH_DIR/"MIDIServer-*.ips; }

# Reports whose stack contains the bug's signature frame.
matching_reports() {
  local f found=()
  for f in $(crash_reports); do
    grep -q "$SIGNATURE" "$f" 2>/dev/null && found+=("$f")
  done
  # Guarded: printf on an empty array trips `set -u`.
  [ "${#found[@]}" -gt 0 ] && printf '%s\n' "${found[@]}"
  return 0
}

server_pid() { pgrep -x MIDIServer || true; }

# Minutes since the newest matching report. Filenames embed the timestamp, so
# the sorted glob already ends with the most recent one.
newest_age_min() {
  local newest
  newest=$(matching_reports | tail -1)
  [ -n "$newest" ] || return 1
  echo $(( ( $(date +%s) - $(stat -f %m "$newest") ) / 60 ))
}

ams_running() {
  # Match the bundle path, never the pattern itself (pgrep -f self-matches).
  ps ax -o command= | grep -q "${AMS}.app/Contents/MacOS/" 
}

diagnose() {
  local reports=() f pid
  while IFS= read -r f; do [ -n "$f" ] && reports+=("$f"); done < <(matching_reports)

  head2 'MIDIServer crash reports'
  if [ "${#reports[@]}" -eq 0 ]; then
    note 'none with this signature - the setup-save bug has not fired here'
  else
    note "${#reports[@]} report(s) match $SIGNATURE, newest last:"
    # Show the last five. A plain negative offset would print nothing at all
    # for arrays shorter than five, so compute the start index.
    local start=0
    [ "${#reports[@]}" -gt 5 ] && start=$(( ${#reports[@]} - 5 ))
    for f in "${reports[@]:start}"; do
      note "  $(basename "$f")"
    done
  fi

  head2 'Current state'
  pid=$(server_pid)
  note "MIDIServer: ${pid:-not resident (normal - it is on-demand)}"
  note "$AMS: $(ams_running && echo running || echo 'not running')"

  head2 'Persisted setup'
  local files=() n
  while IFS= read -r n; do [ -e "$n" ] && files+=("$n"); done < <(setup_files)
  if [ "${#files[@]}" -eq 0 ]; then
    note 'no setup files - CoreMIDI will regenerate them (this is a clean slate)'
  else
    for n in "${files[@]}"; do note "$(ls -lh "$n" | awk '{print $5, $6, $7, $8}')  $(basename "$n")"; done
  fi

  head2 'Verdict'
  if [ "${#reports[@]}" -eq 0 ]; then
    note 'healthy - this signature has never fired on this machine'
    return 0
  fi
  local age; age=$(newest_age_min)
  if [ "${age:-99999}" -le "$LIVE_WINDOW" ]; then
    note "LIVE - newest crash was ${age} min ago; the loop is probably still running"
    note "fix it with:  $0 --fix"
    return 1
  fi
  note "healthy now - ${#reports[@]} past occurrence(s), newest ${age} min ago"
  note 'no action needed unless inputs start vanishing again'
  return 0
}

repair() {
  local stamp backup files=() n before_count after_count pid
  stamp=$(date +%Y%m%d-%H%M%S)
  backup="$BACKUP_ROOT/$stamp"
  before_count=$(matching_reports | grep -c . || true)

  while IFS= read -r n; do [ -e "$n" ] && files+=("$n"); done < <(setup_files)

  head2 'Repair plan'
  note "quit \"$AMS\" (it holds a client and would re-persist the bad setup)"
  note "back up ${#files[@]} setup file(s) -> $backup"
  note 'remove them, then restart MIDIServer and watch it for '"$SETTLE"'s'

  if [ "$ASSUME_YES" -ne 1 ]; then
    printf '\nProceed? [y/N] '
    local reply; read -r reply
    case "$reply" in y|Y|yes|YES) ;; *) echo 'aborted - nothing changed'; exit 0 ;; esac
  fi

  head2 'Repairing'
  if ams_running; then
    osascript -e "quit app \"$AMS\"" >/dev/null 2>&1
    local i
    for i in $(seq 1 10); do ams_running || break; sleep 0.5; done
    note "$(ams_running && echo "warning: $AMS still running" || echo "$AMS quit")"
  else
    note "$AMS was not running"
  fi

  if [ "${#files[@]}" -gt 0 ]; then
    mkdir -p "$backup" || die "cannot create $backup"
    for n in "${files[@]}"; do
      cp -p "$n" "$backup/" || die "backup failed for $n - refusing to remove anything"
    done
    note "backed up: $(ls "$backup" | tr '\n' ' ')"
    for n in "${files[@]}"; do rm -f "$n" && note "removed $(basename "$n")"; done
  else
    note 'no setup files to remove (already a clean slate)'
  fi

  # "No matching processes" is the normal, healthy answer here.
  killall MIDIServer >/dev/null 2>&1 && note 'signalled MIDIServer' \
                                     || note 'MIDIServer was not resident (expected)'

  head2 'Verifying'
  # Audio MIDI Setup is a MIDI client, so launching it spawns a fresh
  # MIDIServer - no compiler or helper binary needed. -g keeps it unfocused.
  open -g -a "$AMS" 2>/dev/null || die "could not launch $AMS to verify"
  local i
  for i in $(seq 1 20); do pid=$(server_pid); [ -n "$pid" ] && break; sleep 0.5; done
  [ -n "${pid:-}" ] || { echo 'FAIL: MIDIServer never started' >&2; return 2; }
  note "MIDIServer started, pid $pid - watching ${SETTLE}s (it used to die at ~3s)"

  for i in $(seq 1 "$SETTLE"); do
    sleep 1
    if ! kill -0 "$pid" 2>/dev/null; then
      local now; now=$(server_pid)
      echo "FAIL: pid $pid died after ${i}s (respawned as ${now:-nothing})" >&2
      return 2
    fi
  done

  after_count=$(matching_reports | grep -c . || true)
  if [ "$after_count" -gt "$before_count" ]; then
    echo "FAIL: a new crash report appeared during verification" >&2
    return 2
  fi

  head2 'Result'
  note "PASS - survived ${SETTLE}s past the crash window, no new crash report"
  note "$AMS is open: re-tick \"Device is online\" for the IAC Driver if you use it"
  note "backup kept at $backup"
  return 0
}

case "$MODE" in
  check) diagnose ;;
  fix)   diagnose; repair ;;
esac

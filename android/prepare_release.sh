#!/usr/bin/env bash
# Prepare an Android release build from THIS checkout (ROADMAP_4 Step 31,
# DECISIONS_4 #46) — the sibling of ios/prepare_release.sh. Run from anywhere;
# no arguments. It BUILDS NOTHING: Android Studio builds the signed bundle
# (Build → Generate Signed Bundle / APK, the author's keystore, see
# android/RELEASING.md). What it does:
#
# 1. Derives the version from git exactly like the desktop build, the spine
#    and the iOS script (`git describe --tags --always --dirty`, `v` stripped):
#      versionName  = X.Y.Z   (numeric — Play, like ASC, rejects anything
#                              else; an RC's "-rc.N" is NOT part of it)
#      versionCode  = commit count on HEAD (monotonic: Play requires strictly
#                              increasing codes; two RCs of one X.Y.Z differ)
#      describe     = the full string, in BuildConfig.BUILD_DESCRIBE and About
# 2. Asks Gradle what IT will build (`:app:printVersion`) and refuses if the
#    two disagree — the Gradle script derives them independently.
# 3. Warns when the tree is dirty or not exactly on a tag: fine for a device
#    build, not for a Play upload.
# Unlike iOS there is no prebuilt-archive trap: Gradle's externalNativeBuild
# compiles the core from source with -DSUMI_APP_VERSION at every build.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

describe="${SUMI_APP_VERSION:-$(git describe --tags --always --dirty)}"
describe="${describe#v}"
if [[ "$describe" =~ ^([0-9]+\.[0-9]+\.[0-9]+) ]]; then name="${BASH_REMATCH[1]}"; else name="0.0.0"; fi
code="$(git rev-list --count HEAD)"
tag_ok="yes"; git describe --tags --exact-match >/dev/null 2>&1 || tag_ok="NO — not on a tag (fine for a device build, not for a Play upload)"
dirty=""; [[ "$describe" == *-dirty ]] && dirty=" (working tree DIRTY)"

echo "== version (git)"
echo "   describe (About):   $describe$dirty"
echo "   versionName (Play): $name"
echo "   versionCode (Play): $code"
echo "   exactly on a tag:   $tag_ok"

echo "== version (Gradle, what the bundle will carry)"
gradle_out="$(cd android && SUMI_APP_VERSION="${SUMI_APP_VERSION:-}" ./gradlew -q --offline :app:printVersion 2>/dev/null || ./gradlew -q :app:printVersion)"
echo "$gradle_out" | sed 's/^/   /'
g_name="$(echo "$gradle_out" | sed -n 's/^versionName=//p')"
g_code="$(echo "$gradle_out" | sed -n 's/^versionCode=//p')"
g_desc="$(echo "$gradle_out" | sed -n 's/^describe=//p')"
if [[ "$g_name" != "$name" || "$g_code" != "$code" || "$g_desc" != "$describe" ]]; then
    echo "!! Gradle and git disagree — do not upload this build" >&2
    exit 1
fi
echo "== ready: open android/ in Android Studio → Build → Generate Signed Bundle / APK (see android/RELEASING.md)"

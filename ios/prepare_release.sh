#!/usr/bin/env bash
# Prepare the iOS Xcode project for an archive from THIS checkout (ROADMAP_4
# Step 28, DECISIONS_4 #37). Run from anywhere; no arguments.
#
#   ios/prepare_release.sh            # then: open ios/midi-sink-ios.xcodeproj
#
# 1. Derives the version from git exactly like the desktop build and the
#    release spine (`git describe --tags --always --dirty`, `v` stripped):
#      MARKETING_VERSION        = X.Y.Z   (numeric — App Store Connect rejects
#                                          anything else; an RC's "-rc.N" is
#                                          NOT part of it)
#      CURRENT_PROJECT_VERSION  = commit count on HEAD (monotonic build number:
#                                          two RCs of one version differ here)
#      SumiBuildDescribe        = the full describe string, shown in About
# 2. Rebuilds libsumi.a / libhostmpe.a for iOS with that version injected —
#    the app links the PREBUILT archives, so skipping this ships a stale core
#    (it has happened).
# 3. Regenerates the Xcode project with xcodegen, which reads the three
#    values from the environment.
# The archive itself is Xcode's (Product → Archive), see ios/RELEASING.md.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

describe="${SUMI_APP_VERSION:-$(git describe --tags --always --dirty)}"
describe="${describe#v}"
if [[ "$describe" =~ ^([0-9]+\.[0-9]+\.[0-9]+) ]]; then marketing="${BASH_REMATCH[1]}"; else marketing="0.0.0"; fi
build="$(git rev-list --count HEAD)"
tag_ok="yes"; git describe --tags --exact-match >/dev/null 2>&1 || tag_ok="NO — not on a tag (fine for a device build, not for a TestFlight upload)"
dirty=""; [[ "$describe" == *-dirty ]] && dirty=" (working tree DIRTY)"

echo "== version"
echo "   describe (About):           $describe$dirty"
echo "   MARKETING_VERSION:          $marketing"
echo "   CURRENT_PROJECT_VERSION:    $build"
echo "   exactly on a tag:           $tag_ok"

echo "== libsumi / libhostmpe for iOS (arm64, iOS 16.0, version injected)"
cmake -B build-ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
      -DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
      -DSUMI_APP_VERSION="$describe" -Wno-dev >/dev/null
cmake --build build-ios | tail -1
ls -la build-ios/core/libsumi.a build-ios/hostmpe/libhostmpe.a | awk '{print "   " $6, $7, $8, $9}'

echo "== xcodegen"
export MARKETING_VERSION="$marketing" CURRENT_PROJECT_VERSION="$build" SUMI_BUILD_DESCRIBE="$describe"
( cd ios && xcodegen generate --quiet )
# ios/Info.plist is the TEMPLATE (xcodegen leaves $(MARKETING_VERSION) etc. for
# Xcode to resolve at build time); the built app's Info.plist carries the values.
echo "   project.pbxproj MARKETING_VERSION:       $(grep -m1 -o 'MARKETING_VERSION = [^;]*' ios/midi-sink-ios.xcodeproj/project.pbxproj | cut -d= -f2 | tr -d ' ')"
echo "   project.pbxproj CURRENT_PROJECT_VERSION: $(grep -m1 -o 'CURRENT_PROJECT_VERSION = [^;]*' ios/midi-sink-ios.xcodeproj/project.pbxproj | cut -d= -f2 | tr -d ' ')"
echo "   Info.plist SumiBuildDescribe:            $(plutil -extract SumiBuildDescribe raw ios/Info.plist)"
echo "   (the archive's Info.plist resolves CFBundleShortVersionString / CFBundleVersion from the two above)"
echo "== ready: open ios/midi-sink-ios.xcodeproj  (scheme midi-sink, destination Any iOS Device, Product > Archive)"

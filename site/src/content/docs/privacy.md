---
title: Privacy policy
description: midi-sink collects nothing — no telemetry, no accounts, no analytics, no network traffic beyond the MIDI transports you enable yourself.
---

**Stable URL:** `https://midi-sink.vibetuned.com/privacy/` — this address is
referenced by the App Store, Google Play and Homebrew listings and will not
change.

*Last updated: September 2026. Applies to every midi-sink app — macOS,
Windows, Linux, iPadOS, Android — and to the web canvas at `/marble/`.*

## The short version

midi-sink collects **no data**. It has no accounts, no telemetry, no crash
reporting service, no analytics, no advertising and no third-party SDKs. It
never contacts a server of ours — there is none.

## What the app does with data, in full

* **MIDI.** The app receives MIDI from instruments you connect (USB, network
  and Bluetooth) and, in Play mode on the tablets, sends MIDI to destinations
  you enable (a virtual port, a MIDI network session, a Bluetooth LE MIDI
  link, a USB-MIDI connection). MIDI messages are note and control data; they
  are processed in memory to draw the picture and are not stored.
* **Settings.** Your preferences (layout, palette, routings, transports) are
  stored on your device only — in the platform's application-support folder
  on desktop, in app preferences on iPadOS/Android, and in the browser's
  `localStorage` for the web canvas. Nothing is synced anywhere.
* **Prints.** "Save print" writes a PNG where you choose. On iPadOS and
  Android the image goes to your photo library or a folder you pick, using
  the system dialog, and only when you ask.
* **Evidence logs (tablets).** A developer option writes MIDI byte logs and
  screen captures **into the app's own documents folder on your device** for
  you to inspect. They never leave the device unless you copy them off it.

## Permissions

* **Bluetooth** — to connect Bluetooth LE MIDI instruments and, on the
  tablets, to advertise the Play surface as a BLE-MIDI peripheral. Bluetooth
  is not used for anything else and no device inventory is kept.
* **Local network (iPadOS)** — to join a MIDI network (RTP-MIDI) session you
  configure. The app initiates no other network connection.
* **USB** — to see class-compliant USB-MIDI devices.
* **Photos / storage** — only when you save a print, through the system
  picker.

No microphone, camera, location, contacts or identifiers are requested.

## Store-specific statements

* **Apple App Store — App Privacy:** "Data Not Collected." midi-sink does not
  collect any data from this app. It contains no third-party SDKs and no
  tracking as defined by Apple's App Tracking Transparency framework.
* **Google Play — Data safety:** No data collected, no data shared. Data is
  not encrypted in transit because no data is transmitted to us. You can
  delete every trace of the app's data by uninstalling it (settings and
  local logs live only inside the app's private storage).
* **Homebrew / Linux / Windows:** the desktop app is a bare executable that
  reads MIDI and writes a settings file in your user profile. It performs no
  update checks and opens no network sockets.

## The website

This site and the web canvas are static files served by GitHub Pages. We set
no cookies and run no analytics. GitHub may log requests as described in the
[GitHub Privacy Statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).
The web canvas asks your browser for WebGPU and, on Chrome and Edge, for Web
MIDI access; both stay within your browser. Settings are kept in your
browser's local storage. Videos in the [gallery](../gallery/) may be embedded
from YouTube through its no-cookie domain; YouTube's own policy applies when
you press play.

## Children

midi-sink is a general-audience music tool. It is not directed at children
and, collecting nothing, holds no information about anyone of any age.

## Changes and contact

If this policy ever changes, the change and its date will appear here, at
this URL. Questions: see the [support page](../support/).

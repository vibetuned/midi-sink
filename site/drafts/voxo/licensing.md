---
title: Instruments and their licences
description: What the app bundles, what the format is, and whose terms travel with a library.
---

<!-- Drafted at the Phase-7 close for step 63 (the documentation), in the author's voice to be. -->

midi-sink plays **Decent Sampler** presets and libraries: a `.dspreset` file
with its samples beside it, or a `.dslibrary` that holds both. The format is
Decent Sampler's, and a format is not something a licence can fence off —
file formats are not copyrightable, which is why any sampler may read one.
What midi-sink reads of it is documented honestly: the compat report a
library shows once, calmly, lists what it asked for that this version plays
without.

**The app bundles no libraries.** One tiny demo instrument ships so that a
first launch makes a sound: the **Dan Tranh**, a Vietnamese zither, sixteen
notes from the Versilian Community Sample Library (VCSL), released to the
public domain under CC0 1.0 — its licence text travels in the app beside the
samples. Everything else you play is yours: libraries you bought, downloaded
or made, each under its own terms. midi-sink copies a library you import
into its own folder and never sends it anywhere; sharing a preset of yours
shares the settings, never the samples.

Third parties inside the sound engine, all permissively licensed:
miniaudio (public domain / MIT-0), pugixml (MIT), miniz (MIT), dr_wav and
dr_flac (public domain / MIT-0), and the app's own code under AGPL-3.0.

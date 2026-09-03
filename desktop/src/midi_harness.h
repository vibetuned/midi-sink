// Host-side MIDI plumbing (PROJECT_SPEC.md §1): libremidi observer for
// hotplug, opens every input, forwards raw bytes to the core one message at a
// time, unparsed. The core never links libremidi (§7).
#pragma once

#include "sumi_core.h"

// Starts watching/opening MIDI inputs; forwards to `inst` until destroyed.
// Returns an opaque handle, or nullptr on failure.
void* sumi_midi_harness_start(sumi_instance_t* inst);
void  sumi_midi_harness_stop(void* harness);

// Call once per frame from the main loop. CoreMIDI delivers hotplug
// notifications on the creating thread's run loop, which GLFW's event pump
// does not reliably service — this pumps it (zero-timeout, non-blocking).
void  sumi_midi_harness_poll(void* harness);

// Inject one synthetic message through the harness's §5.2 producer
// serialization (same mutex as device callbacks) — for key bindings and
// scripted tests that must exercise the real MIDI/ctl path (v0.4 ripple
// dims). Never call sumi_push_midi directly beside a running harness.
void  sumi_midi_harness_inject(void* harness, uint8_t status, uint8_t d1, uint8_t d2);

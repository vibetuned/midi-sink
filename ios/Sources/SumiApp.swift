// midi-sink iOS shell (PROJECT_SPEC.md §5.4). The core is the C library
// libsumi (import SumiCore via module.modulemap) — this shell only hosts it:
// CAMetalLayer view + CADisplayLink + CoreMIDI + touch gestures + settings.
import SwiftUI
import SumiCore

@main
struct SumiApp: App {
    @Environment(\.scenePhase) private var scenePhase
    // Host-owned sim_scale default (§ params comment: the core never detects
    // devices): 1.0 on iPad-class GPUs, 0.75 below — see SumiCanvas.defaultSimScale.
    @State private var fullResolution = SumiCanvasView.defaultsToFullResolution
    @State private var layout: UInt32 = 0   // SUMI_LAYOUT_FIFTHS
    // Phase 4 §1: Marble (Step-13 gestures) vs Play (virtual MPE surface).
    @AppStorage("playMode") private var playMode = false
    @AppStorage("velocityFromTouchSize") private var velocityFromTouchSize = false
    // Step 17 outbound transports (per-transport limiters live in the shell).
    @AppStorage("outVirtual") private var outVirtual = true
    @AppStorage("outNetwork") private var outNetwork = false
    @AppStorage("outBLE") private var outBLE = false
    // Step 18 (§8): sustain button behavior — momentary by default (the user
    // wants the press-and-hold pedal feel); toggle stays available here.
    // Wheel VALUES live in hostmpe's strip engine (session-persistent); CC
    // assignments are not persisted (deferred).
    @AppStorage("sustainToggle") private var sustainToggle = false
    // v0.4 (§4.3(5), DECISIONS_3 #34): CC74 slide routing + pinch style.
    @AppStorage("slidePinch") private var slidePinch = false
    @AppStorage("pinchCrossed") private var pinchCrossed = false
    // v0.4 press_mode (§3.4, step 20): 0xD0 routing for pressure hardware.
    @AppStorage("pressSwirl") private var pressSwirl = false
    // v0.4 bend_mode (§4.3(6), DECISIONS_3 #35 corrected): the PER-NOTE bend
    // routing — subtle vibrato as a water shimmer instead of drop dragging.
    @AppStorage("bendRipple") private var bendRipple = false
    @State private var showSettings = false

    var body: some Scene {
        WindowGroup {
            ZStack(alignment: .topTrailing) {
                SumiCanvas(simScale: fullResolution ? 1.0 : 0.75, layout: layout,
                           playMode: playMode,
                           velocityFromTouchSize: velocityFromTouchSize,
                           outVirtual: outVirtual, outNetwork: outNetwork,
                           outBLE: outBLE, sustainToggle: sustainToggle,
                           slidePinch: slidePinch, pinchCrossed: pinchCrossed,
                           bendRipple: bendRipple, pressSwirl: pressSwirl)
                    .ignoresSafeArea()
                Button {
                    showSettings = true
                } label: {
                    Image(systemName: "gearshape.fill")
                        .font(.title2)
                        .foregroundStyle(.secondary.opacity(0.55))
                        .padding(14)
                }
            }
            .statusBarHidden()
            .persistentSystemOverlays(.hidden)
            .sheet(isPresented: $showSettings) {
                SettingsSheet(fullResolution: $fullResolution, layout: $layout,
                              playMode: $playMode,
                              velocityFromTouchSize: $velocityFromTouchSize,
                              outVirtual: $outVirtual, outNetwork: $outNetwork,
                              outBLE: $outBLE, sustainToggle: $sustainToggle,
                              slidePinch: $slidePinch, pinchCrossed: $pinchCrossed,
                              bendRipple: $bendRipple, pressSwirl: $pressSwirl)
            }
            .onChange(of: scenePhase) { phase in
                // Metal work in a backgrounded app is a crash on iOS: the
                // display link pauses on .background and resumes on .active.
                SumiCanvasView.shared?.setScenePhaseActive(phase == .active)
            }
        }
    }
}

struct SettingsSheet: View {
    @Binding var fullResolution: Bool
    @Binding var layout: UInt32
    @Binding var playMode: Bool
    @Binding var velocityFromTouchSize: Bool
    @Binding var outVirtual: Bool
    @Binding var outNetwork: Bool
    @Binding var outBLE: Bool
    @Binding var sustainToggle: Bool
    @Binding var slidePinch: Bool
    @Binding var pinchCrossed: Bool
    @Binding var bendRipple: Bool
    @Binding var pressSwirl: Bool
    @State private var status = ""
    private let statusTimer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    private var layoutIsPlayable: Bool { layout == 1 || layout == 2 || layout == 5 }

    private static let layoutNames: [(UInt32, String)] = [
        (0, "Circle of fifths"),
        (1, "Chromatic grid"),
        (2, "Jankó"),
        (3, "Piano roll (horizontal)"),
        (4, "Piano roll (vertical)"),
        (5, "Piano grid"),
    ]

    var body: some View {
        NavigationStack {
            Form {
                Section("Layout") {
                    Picker("Pitch layout", selection: $layout) {
                        ForEach(Self.layoutNames, id: \.0) { id, name in
                            Text(name).tag(id)
                        }
                    }
                }
                Section("Mode") {
                    Picker("Mode", selection: $playMode) {
                        Text("Marble").tag(false)
                        Text("Play").tag(true)
                    }
                    .pickerStyle(.segmented)
                    .disabled(!layoutIsPlayable)
                    Text(layoutIsPlayable
                         ? (playMode
                            ? "Play: each touch is an MPE joystick on the lattice."
                            : "Marble: tap = drop, drag = tine, twist = vortex.")
                         : "Play mode is available on the Chromatic grid, Jankó and Piano grid layouts.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if playMode && layoutIsPlayable {
                        Toggle("Velocity from touch size", isOn: $velocityFromTouchSize)
                        Text("Glass has no force sensor: velocity is synthesized "
                             + "(96 fixed, or coarse touch-size modulation).")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                }
                if playMode && layoutIsPlayable {
                    Section("Control strip") {
                        Toggle("Sustain button latches (toggle)", isOn: $sustainToggle)
                        Text("The strip floats top-left over the full lattice. "
                             + "Pitch springs back to center on release; Mod and "
                             + "the two assignable wheels latch (drag adds — "
                             + "regrasping never jumps). Long-press an "
                             + "assignable wheel to change its CC. All strip "
                             + "traffic rides the MPE master channel.")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                }
                Section("Note bend") {
                    Picker("Per-note bend", selection: $bendRipple) {
                        Text("Glide").tag(false)
                        Text("Ripple").tag(true)
                    }
                    .pickerStyle(.segmented)
                    Text(bendRipple
                         ? "Subtle vibrato: the shimmer's depth is the bend's "
                           + "distance from center, just like glide — vibrato "
                           + "breathes the water, the motion stills when the "
                           + "note re-centers or releases, and each cycle "
                           + "bakes a faint feathered comb into the ink — "
                           + "permanent, like glide. The drop holds position; "
                           + "CC 103 (or a strip wheel) sets the wavelength."
                         : "Glide (v1): a note's bend drags its drop across "
                           + "the lattice. Switch to Ripple when the music "
                           + "asks for a subtler vibrato.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Pressure (aftertouch)") {
                    Picker("Channel pressure", selection: $pressSwirl) {
                        Text("Feed").tag(false)
                        Text("Swirl").tag(true)
                    }
                    .pickerStyle(.segmented)
                    Text(pressSwirl
                         ? "Hardware aftertouch (Osmose, ROLI press) stirs a "
                           + "Lamb–Oseen swirl at the note — its own rings "
                           + "spin as a solid disk while the far field stirs "
                           + "the neighbors; adjacent notes counter-rotate. "
                           + "On the play surface: pull DOWN to stir (the "
                           + "down half-axis is always the swirl)."
                         : "Hardware aftertouch feeds the drop (the v1 grow). "
                           + "The play surface's down-pull plays the swirl "
                           + "either way; this only routes 0xD0 hardware.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Slide (CC74)") {
                    Picker("CC74 routing", selection: $slidePinch) {
                        Text("Hue").tag(false)
                        Text("Pinch").tag(true)
                    }
                    .pickerStyle(.segmented)
                    if slidePinch {
                        Picker("Pinch style", selection: $pinchCrossed) {
                            Text("Saddle").tag(false)
                            Text("Crossed tines").tag(true)
                        }
                        .pickerStyle(.segmented)
                    }
                    Text(slidePinch
                         ? "Per-note CC74 deltas fold the water at the note "
                           + "(delta-driven; the style also applies to the "
                           + "Step-20 stylus pinch)."
                         : "Per-note CC74 modulates the drop's hue — the v1 "
                           + "behavior. Switch to Pinch to fold the water "
                           + "instead (ROLI slide, Osmose CC74).")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Simulation") {
                    Toggle("Full-resolution simulation", isOn: $fullResolution)
                    Text(fullResolution
                         ? "sim_scale 1.0 — full canvas resolution."
                         : "sim_scale 0.75 — lighter thermals on smaller GPUs.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("MIDI") {
                    NavigationLink("Pair Bluetooth MIDI instrument…") {
                        BluetoothMidiPairingView()
                            .navigationTitle("Bluetooth MIDI")
                            .navigationBarTitleDisplayMode(.inline)
                    }
                    Text("Wired and network MIDI inputs connect automatically.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Outbound MIDI (Play mode)") {
                    Toggle("Virtual source (USB / on-device apps)", isOn: $outVirtual)
                    Text("Feeds on-device apps and — when a Mac is wired — the "
                         + "USB/IDAM link. Just connect the cable: the surface "
                         + "appears on the Mac as the \"iPad\" MIDI port in "
                         + "Audio MIDI Setup (one merged port per link; MIDI "
                         + "transports name ports after the device, not the "
                         + "app, and MIDI needs no Enable — that button is for "
                         + "IDAM audio). Wired is the lowest-latency, "
                         + "highest-bandwidth path.")
                        .font(.footnote).foregroundStyle(.secondary)
                    Toggle("Network session (Wi-Fi)", isOn: $outNetwork)
                    Toggle("Bluetooth (BLE) stream", isOn: $outBLE)
                    NavigationLink("Advertise Bluetooth MIDI…") {
                        BluetoothMidiAdvertiseView()
                            .navigationTitle("Advertise BLE MIDI")
                            .navigationBarTitleDisplayMode(.inline)
                    }
                    Button("Re-sync DAW (MCM + bend range)") {
                        SumiCanvasView.shared?.resyncTransports()
                    }
                    Button(role: .destructive) {
                        SumiCanvasView.shared?.panicAllNotes()
                    } label: {
                        Text("Stop all notes (panic)")
                    }
                    Text("Panic releases every held voice and silences all "
                         + "pipes. A connected Bluetooth device owns its own "
                         + "link — drop it from that device's Bluetooth "
                         + "settings; switching a transport off here silences "
                         + "it so nothing hangs.")
                        .font(.footnote).foregroundStyle(.secondary)
                    Button("Run 60 s storm test (10 voices)") {
                        SumiCanvasView.shared?.startStormTest()
                    }
                    Text("The surface plays external synths as a 15-voice MPE "
                         + "controller. Virtual/network stream at ≤100 Hz per "
                         + "dimension; BLE uses a shared ~300 msg/s budget.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Canvas") {
                    Button("Paper dip (fresh sheet)") {
                        SumiCanvasView.shared?.triggerPaperDip()
                    }
                    Text("Freezes and snapshots the canvas, then starts a "
                         + "clean sheet. The sustain pedal no longer does this "
                         + "in Play mode — it is a musical control there.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Evidence") {
                    Button("Capture screen (3 s delay, 6 frames)") {
                        SumiCanvasView.shared?.startCaptureBurst()
                    }
                    Button("Flush logs to Documents") {
                        SumiCanvasView.shared?.flushLogsNow()
                    }
                    Text("Captures and the byte/latency/session logs land in "
                         + "the app's Documents folder — pull them with "
                         + "devicectl device copy from.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("About") {
                    // App version from the tag (CFBundleShortVersionString +
                    // build number), the full describe, and the engine's ABI
                    // version — the same three the desktop About shows (#3, #37).
                    let info = Bundle.main.infoDictionary ?? [:]
                    let short = info["CFBundleShortVersionString"] as? String ?? "?"
                    let build = info["CFBundleVersion"] as? String ?? "?"
                    let describe = info["SumiBuildDescribe"] as? String ?? short
                    let engine = sumi_version()
                    Text("midi-sink \(short) (\(build)) · \(describe)")
                        .font(.system(.footnote, design: .monospaced))
                    Text("libsumi \(engine >> 16).\((engine >> 8) & 0xFF).\(engine & 0xFF)")
                        .font(.system(.footnote, design: .monospaced)).foregroundStyle(.secondary)
                }
                Section("Session") {
                    Text(status.isEmpty ? "—" : status)
                        .font(.system(.footnote, design: .monospaced))
                        .onReceive(statusTimer) { _ in
                            status = SumiCanvasView.shared?.statusLine ?? ""
                        }
                }
            }
            .navigationTitle("midi-sink")
            .navigationBarTitleDisplayMode(.inline)
        }
        .presentationDetents([.medium])
    }
}

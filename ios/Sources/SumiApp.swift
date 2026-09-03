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
    @State private var showSettings = false

    var body: some Scene {
        WindowGroup {
            ZStack(alignment: .topTrailing) {
                SumiCanvas(simScale: fullResolution ? 1.0 : 0.75, layout: layout,
                           playMode: playMode,
                           velocityFromTouchSize: velocityFromTouchSize,
                           outVirtual: outVirtual, outNetwork: outNetwork,
                           outBLE: outBLE, sustainToggle: sustainToggle)
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
                              outBLE: $outBLE, sustainToggle: $sustainToggle)
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

// midi-sink iOS shell (PROJECT_SPEC.md §5.4). The core is the C library
// libsumi (import SumiCore via module.modulemap) — this shell only hosts it:
// CAMetalLayer view + CADisplayLink + CoreMIDI + touch gestures + settings.
import SwiftUI
import SumiCore

@main
struct SumiApp: App {
    @Environment(\.scenePhase) private var scenePhase
    // Phase 6 step 44a: the session — every core param, the custom palette,
    // the CC map, the input dialect, the routed controls, the strip
    // assignments — is one model persisted through the preset serializer
    // (Session.swift); the 1.0 @AppStorage rows migrate into it once. The
    // print ledger keeps the session's dips. What stays here are the shell's
    // own switches.
    @StateObject private var session = SessionStore()
    @StateObject private var ledger = PrintLedger()
    // Phase 4 §1: Marble (Step-13 gestures) vs Play (virtual MPE surface).
    @AppStorage("playMode") private var playMode = false
    @AppStorage("velocityFromTouchSize") private var velocityFromTouchSize = false
    // Step 17 outbound transports (per-transport limiters live in the shell).
    @AppStorage("outVirtual") private var outVirtual = true
    @AppStorage("outNetwork") private var outNetwork = false
    @AppStorage("outBLE") private var outBLE = false
    // Step 18 (§8): sustain button behavior — momentary by default (the user
    // wants the press-and-hold pedal feel); toggle stays available here.
    @AppStorage("sustainToggle") private var sustainToggle = false
    @State private var showSettings = false

    var body: some Scene {
        WindowGroup {
            ZStack(alignment: .topTrailing) {
                SumiCanvas(session: session, ledger: ledger,
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
                SettingsSheet(session: session, ledger: ledger,
                              playMode: $playMode,
                              velocityFromTouchSize: $velocityFromTouchSize,
                              outVirtual: $outVirtual, outNetwork: $outNetwork,
                              outBLE: $outBLE, sustainToggle: $sustainToggle)
            }
            .onChange(of: scenePhase) { phase in
                // Metal work in a backgrounded app is a crash on iOS: the
                // display link pauses on .background and resumes on .active.
                SumiCanvasView.shared?.setScenePhaseActive(phase == .active)
                if phase != .active { session.saveSession() }
            }
        }
    }
}

struct SettingsSheet: View {
    @ObservedObject var session: SessionStore
    @ObservedObject var ledger: PrintLedger
    @Binding var playMode: Bool
    @Binding var velocityFromTouchSize: Bool
    @Binding var outVirtual: Bool
    @Binding var outNetwork: Bool
    @Binding var outBLE: Bool
    @Binding var sustainToggle: Bool
    // CC map editor scratch state (#56)
    @State private var newCC = 74
    @State private var newTarget: UInt32 = 0
    @State private var newChannel = 0xFF   // 0xFF = any
    @State private var status = ""
    @State private var midiInputs: MidiSource.Snapshot?
    private let statusTimer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    private var layout: UInt32 { session.params.pitch_layout }
    private var layoutIsPlayable: Bool { layout == 1 || layout == 2 || layout == 5 }
    private var anod: Bool { session.params.medium == SUMI_MEDIUM_ANOD.rawValue }

    private static let layoutNames: [(UInt32, String)] = [
        (0, "Circle of fifths"),
        (1, "Chromatic grid (playable)"),
        (2, "Jankó (playable)"),
        (3, "Piano roll (left)"),
        (4, "Piano roll (top)"),
        (5, "Piano grid (playable)"),
        (6, "Piano roll (right)"),
        (7, "Piano roll (bottom)"),
    ]

    @ViewBuilder
    private func valueSlider(_ label: String, _ v: Binding<Double>, _ range: ClosedRange<Double>,
                             _ fmt: String, step: Double? = nil) -> some View {
        HStack {
            Text(label)
            if let step {
                Slider(value: v, in: range, step: step)
            } else {
                Slider(value: v, in: range)
            }
            Text(String(format: fmt, v.wrappedValue)).monospacedDigit()
                .frame(minWidth: 52, alignment: .trailing)
        }
    }

    private func floatSlider(_ label: String, _ v: Binding<Float>, _ range: ClosedRange<Double>,
                             _ fmt: String, step: Double? = nil) -> some View {
        valueSlider(label, Binding(get: { Double(v.wrappedValue) }, set: { v.wrappedValue = Float($0) }),
                    range, fmt, step: step)
    }

    private func intSlider(_ label: String, _ v: Binding<Int>) -> some View {
        HStack {
            Text(label)
            Slider(value: Binding(get: { Double(v.wrappedValue) },
                                  set: { v.wrappedValue = Int($0.rounded()) }),
                   in: 0...127, step: 1)
            Text("\(v.wrappedValue)").monospacedDigit().frame(minWidth: 40, alignment: .trailing)
        }
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Canvas") {   // first: the most-used control (#78)
                    // Step 44a (QOL §6): dip and clear, worded apart — a repeated beta confusion.
                    Button("Dip the paper — keep the print") { SumiCanvasView.shared?.paperDip() }
                    Button("Clear the canvas — discard", role: .destructive) { SumiCanvasView.shared?.clearCanvas() }
                    Text("Dip lifts the sheet as a print into the ledger below — re-export it at any size — "
                         + "then lays fresh paper. Clear lays fresh paper and keeps nothing. The sustain "
                         + "pedal does neither in Play mode: it is a musical control there.")
                        .font(.footnote).foregroundStyle(.secondary)
                    NavigationLink {
                        PrintsPage(session: session, ledger: ledger)
                    } label: {
                        LabeledContent("Prints", value: ledger.entries.isEmpty ? "none yet" : "\(ledger.entries.count) this session")
                    }
                    if !ledger.status.isEmpty {
                        Text(ledger.status).font(.footnote).foregroundStyle(.secondary)
                    }
                }
                Section("Medium & look") {
                    Picker("Medium", selection: $session.params.medium) {
                        Text("Sumi — ink on washi").tag(SUMI_MEDIUM_SUMI.rawValue)
                        Text("Anod — strain-glow").tag(SUMI_MEDIUM_ANOD.rawValue)
                    }
                    .pickerStyle(.segmented)
                    Text(anod
                         ? "Anod: the accumulated strain glows like ionized gas — the same deformation "
                           + "history re-read as a discharge record. Switching is live."
                         : "Sumi: ink phase bands on paper. Switching is live; each medium brings its "
                           + "own palettes and its default expression routing.")
                        .font(.footnote).foregroundStyle(.secondary)
                    NavigationLink("Palette — \(PalettePage.activeName(session))") { PalettePage(session: session) }
                    NavigationLink(anod ? "Substrate — glass & glow" : "Substrate — paper") { SubstratePage(session: session) }
                    NavigationLink("Presets") { PresetsPage(session: session) }
                    NavigationLink("Operators — Chladni, burst, spark, Chirikov") { OperatorsPage(session: session) }
                }
                Section("Layout") {
                    Picker("Pitch layout", selection: $session.params.pitch_layout) {
                        ForEach(Self.layoutNames, id: \.0) { id, name in
                            Text(name).tag(id)
                        }
                    }
                    floatSlider("Viscosity", $session.params.fluid_viscosity, 0...1, "%.2f")
                    floatSlider("Ink feed (pressure)", $session.params.expansion_rate, 0.1...4, "%.2f")
                    if layout == 3 || layout == 4 || layout == 6 || layout == 7 {
                        floatSlider("Tempo (BPM)", $session.params.bpm, 20...300, "%.0f", step: 1)
                        floatSlider("Roll speed", $session.params.roll_speed, 0.02...0.25, "%.4f")
                        Text("Canvas lengths per beat. 1/16 keeps 4 bars of 4/4 on screen.")
                            .font(.footnote).foregroundStyle(.secondary)
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
                Section("Input") {
                    Picker("Input", selection: $session.inputMode) {
                        Text("MPE").tag(1)
                        Text("Classic keyboard").tag(2)
                        Text("Wind").tag(3)
                    }
                    .pickerStyle(.segmented)
                    Text(session.inputMode == 3
                         ? "Wind: one voice, played exactly as MPE — each note a strike drop, breath "
                           + "(CC 2 / 7 / 11 or channel pressure) the unbounded feed, CC 74 / poly "
                           + "pressure / a member-channel bend the IMU layer — plus a wake dragging the "
                           + "sounding drop to the next note on every legato change."
                         : session.inputMode == 2
                         ? "Classic: every note is its own voice on any channel; bend is the global "
                           + "shear tine and the mod wheel the vortex. For a keyboard sending inside "
                           + "the member zone (channels 2–16)."
                         : "MPE (default): per-note bend, pressure and CC 74 on the member channels; "
                           + "a plain keyboard on channel 1 still plays chords. CC 64 never touches "
                           + "the canvas.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Expression routing") {
                    // 1.1.0 (MEDIUM §4): every mode has the medium's default first — the desktop's lists.
                    Picker("Per-note bend", selection: Binding(
                        get: { session.params.bend_mode },
                        set: { m in
                            session.params.bend_mode = m
                            if m == 1 { session.params.ripple_bake = 1 } else if m == 0 { session.params.ripple_bake = 0 }   // the Ripple choice bakes (#36)
                        })) {
                        Text("Medium default").tag(UInt32(SUMI_MODE_MEDIUM_DEFAULT))
                        Text("Glide (drag the drop)").tag(UInt32(0))
                        Text("Ripple amplitude").tag(UInt32(1))
                        Text("Torsion wavelength").tag(UInt32(2))
                        Text("Spark frequency").tag(UInt32(3))
                        Text("Chladni stir").tag(UInt32(4))
                    }
                    Picker("Channel pressure", selection: $session.params.press_mode) {
                        Text("Medium default").tag(UInt32(SUMI_MODE_MEDIUM_DEFAULT))
                        Text("Ink feed").tag(UInt32(0))
                        Text("Lamb–Oseen swirl").tag(UInt32(1))
                        Text("Torsion sweep feed").tag(UInt32(2))
                    }
                    Picker("Slide (CC 74)", selection: $session.params.slide_mode) {
                        Text("Medium default").tag(UInt32(SUMI_MODE_MEDIUM_DEFAULT))
                        Text("Hue").tag(UInt32(0))
                        Text("Pinch").tag(UInt32(1))
                        Text("Spark frequency").tag(UInt32(2))
                    }
                    if session.params.slide_mode == 1 {
                        Picker("Pinch style", selection: $session.params.pinch_variant) {
                            Text("Saddle").tag(UInt32(0))
                            Text("Crossed tines").tag(UInt32(1))
                        }
                        .pickerStyle(.segmented)
                    }
                    Text(anod
                         ? "Anod's defaults: the bend stirs the Chladni cells (its distance the rate, its "
                           + "sign the sense), pressure feeds the torsion sweep, the slide sets the spark's "
                           + "frequency, poly pressure the torsion's and spark's wavenumbers, the mod wheel "
                           + "throws the Chirikov map."
                         : "Sumi's defaults: the bend drags the drop (glide), pressure feeds it, the slide "
                           + "sets its hue, poly pressure (the play surface's down-pull) stirs the swirl, "
                           + "the mod wheel the vortex.")
                        .font(.footnote).foregroundStyle(.secondary)
                    Picker("Vortex profile", selection: $session.params.vortex_profile) {
                        Text("Exponential").tag(UInt32(0))
                        Text("Rankine").tag(UInt32(1))
                        Text("Torsion").tag(UInt32(3))
                    }
                    .pickerStyle(.segmented)
                    Toggle("Torsion sweep on note-on", isOn: Binding(
                        get: { session.params.torsion_sweep == 1 },
                        set: { session.params.torsion_sweep = $0 ? 1 : 0 }))
                    Text("The CC-routed vortex and the two-finger twist use the profile. Torsion: rings of "
                         + "alternating angular shear, exact at any amplitude (wavelength and phase on CC 104 / 105).")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Ripple") {
                    let ampCC = session.route(for: 7)
                    let frqCC = session.route(for: 8)
                    intSlider("Amount", session.control(7)).disabled(ampCC == nil)
                    intSlider("Wavelength", session.control(8)).disabled(frqCC == nil)
                    HStack {
                        Text("Angle")
                        Slider(value: Binding(get: { Double(session.params.ripple_angle) * 57.29578 },
                                              set: { session.params.ripple_angle = Float($0 / 57.29578) }),
                               in: 0...180, step: 1)
                        Text(String(format: "%.0f°", Double(session.params.ripple_angle) * 57.29578)).monospacedDigit()
                            .frame(minWidth: 52, alignment: .trailing)
                    }
                    Text(ampCC == nil || frqCC == nil
                         ? "Route a CC to the ripple dimensions in the CC map to use these."
                         : "Sent as CC \(ampCC!) / CC \(frqCC!) through the MIDI path (the same "
                           + "route a controller would use).")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Stylus wake") {
                    Picker("Fluid", selection: $session.params.wake_profile) {
                        Text("Inviscid doublet").tag(UInt32(0))
                        Text("Viscous stroke").tag(UInt32(1))
                    }
                    .pickerStyle(.segmented)
                    if session.params.wake_profile == 1 {
                        floatSlider("Spread l/a", $session.params.wake_spread, 1.5...12, "%.1f", step: 0.1)
                    }
                    Text(session.params.wake_profile == 1
                         ? "The pen's stroke is an impulse in a viscous layer (the 2-D "
                           + "Stokeslet): small spread is sharp and close, large is soft "
                           + "and far-reaching."
                         : "The pen's stroke is the exact potential flow around a rigid tip.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Simulation") {
                    Toggle("Full-resolution simulation", isOn: Binding(
                        get: { session.params.sim_scale >= 0.99 },
                        set: { session.params.sim_scale = $0 ? 1.0 : 0.75 }))
                    Text(session.params.sim_scale >= 0.99
                         ? "sim_scale 1.0 — full canvas resolution."
                         : "sim_scale 0.75 — lighter thermals on smaller GPUs.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("CC map") {
                    // #56: the desktop's routing table — any CC, any channel or
                    // "any", to any global dimension. Swipe a row to remove it.
                    let routes = session.ccRoutes
                    ForEach(routes) { r in
                        HStack {
                            Text(String(format: "CC %3d", Int(r.cc)))
                                .font(.system(.body, design: .monospaced))
                            Text(r.channel == 0xFF ? "any" : "ch \(r.channel + 1)")
                                .foregroundStyle(.secondary)
                            Spacer()
                            Text(CcMap.ctlName(r.target))
                        }
                    }
                    .onDelete { idx in
                        var rs = routes
                        rs.remove(atOffsets: idx)
                        session.ccRoutes = rs
                    }
                    Stepper("CC \(newCC)", value: $newCC, in: 0...127)
                    Picker("Channel", selection: $newChannel) {
                        Text("any").tag(0xFF)
                        ForEach(0..<16, id: \.self) { Text("\($0 + 1)").tag($0) }
                    }
                    Picker("Dimension", selection: $newTarget) {
                        ForEach(CcMap.ctlNames, id: \.0) { id, name in Text(name).tag(id) }
                    }
                    Button("Add route") {
                        var rs = routes.filter { !($0.cc == UInt8(newCC) && $0.channel == UInt8(newChannel)) }
                        rs.append(CcRoute(channel: UInt8(newChannel), cc: UInt8(newCC), target: newTarget))
                        session.ccRoutes = rs
                    }
                    Button("Restore default map") { session.ccRoutes = CcMap.defaults }
                    Text("Defaults: mod wheel → vortex strength; breath, volume and "
                         + "expression → ink flow; the Airwave's Raise, Glide, Slide, Tilt "
                         + "and Flex; CC 102 / 103 → the ripple; CC 104–109 → the torsion, the Chladni "
                         + "stir and balance, the spark frequency and the Chirikov throw.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("MIDI") {
                    NavigationLink("Pair Bluetooth MIDI instrument…") {
                        BluetoothMidiPairingView()
                            .navigationTitle("Bluetooth MIDI")
                            .navigationBarTitleDisplayMode(.inline)
                    }
                    // #55: the desktop's "MIDI inputs" list, so a USB
                    // keyboard that paints nothing can be placed in one look:
                    // not listed (CoreMIDI never saw it), listed with the
                    // counter still (no bytes reach the app), or counting.
                    if let m = midiInputs, !m.inputs.isEmpty {
                        ForEach(m.inputs, id: \.self) { n in
                            Label(n, systemImage: "pianokeys").font(.footnote)
                        }
                    } else {
                        Text("No MIDI inputs found. Wired, network and paired "
                             + "Bluetooth instruments connect automatically.")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                    if let m = midiInputs {
                        Text("received \(m.forwarded) messages"
                             + (m.last.isEmpty ? "" : " · last \(m.last)")
                             + (m.sourcesSeen > m.inputs.count
                                ? " · \(m.sourcesSeen - m.inputs.count) source(s) skipped" : ""))
                            .font(.system(.footnote, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    Button("Rescan now") { SumiCanvasView.shared?.midiRescanNow() }
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
                            midiInputs = SumiCanvasView.shared?.midiInputs()
                        }
                }
            }
            .navigationTitle("midi-sink")
            .navigationBarTitleDisplayMode(.inline)
        }
        .presentationDetents([.medium, .large])
    }
}

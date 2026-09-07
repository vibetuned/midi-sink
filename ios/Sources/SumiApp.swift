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
    @AppStorage("wakeViscous") private var wakeViscous = false   // v0.7 (#53)
    @AppStorage("wakeSpread") private var wakeSpread = 3.0
    // v0.4 bend_mode (§4.3(6), DECISIONS_3 #35 corrected): the PER-NOTE bend
    // routing — subtle vibrato as a water shimmer instead of drop dragging.
    @AppStorage("bendRipple") private var bendRipple = false
    // Step 33 (#56): the desktop settings window's remaining rows — the same
    // settings on every platform. Defaults are the core's (engine.cpp).
    @AppStorage("palette") private var palette = 0
    @AppStorage("viscosity") private var viscosity = 0.5
    @AppStorage("inkFeed") private var inkFeed = 1.0
    @AppStorage("roughness") private var roughness = 0.5
    @AppStorage("bpm") private var bpm = 120.0
    @AppStorage("rollSpeed") private var rollSpeed = 0.0625
    @AppStorage("vortexRankine") private var vortexRankine = false
    @AppStorage("rippleAmount") private var rippleAmount = 0
    @AppStorage("rippleWavelength") private var rippleWavelength = 32
    @AppStorage("rippleAngle") private var rippleAngle = 0.0
    @AppStorage("ccMap") private var ccMap = ""   // "" = the default map
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
                           bendRipple: bendRipple, pressSwirl: pressSwirl,
                           wakeViscous: wakeViscous, wakeSpread: wakeSpread,
                           palette: palette, viscosity: viscosity, inkFeed: inkFeed,
                           roughness: roughness, bpm: bpm, rollSpeed: rollSpeed,
                           vortexRankine: vortexRankine, rippleAmount: rippleAmount,
                           rippleWavelength: rippleWavelength, rippleAngle: rippleAngle,
                           ccMap: ccMap)
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
                              bendRipple: $bendRipple, pressSwirl: $pressSwirl,
                              wakeViscous: $wakeViscous, wakeSpread: $wakeSpread,
                              palette: $palette, viscosity: $viscosity, inkFeed: $inkFeed,
                              roughness: $roughness, bpm: $bpm, rollSpeed: $rollSpeed,
                              vortexRankine: $vortexRankine, rippleAmount: $rippleAmount,
                              rippleWavelength: $rippleWavelength, rippleAngle: $rippleAngle,
                              ccMap: $ccMap)
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
    @Binding var wakeViscous: Bool
    @Binding var wakeSpread: Double
    @Binding var palette: Int
    @Binding var viscosity: Double
    @Binding var inkFeed: Double
    @Binding var roughness: Double
    @Binding var bpm: Double
    @Binding var rollSpeed: Double
    @Binding var vortexRankine: Bool
    @Binding var rippleAmount: Int
    @Binding var rippleWavelength: Int
    @Binding var rippleAngle: Double
    @Binding var ccMap: String
    // CC map editor scratch state (#56)
    @State private var newCC = 74
    @State private var newTarget: UInt32 = 0
    @State private var newChannel = 0xFF   // 0xFF = any
    @State private var status = ""
    @State private var midiInputs: MidiSource.Snapshot?
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
                Section("Layout & look") {
                    Picker("Pitch layout", selection: $layout) {
                        ForEach(Self.layoutNames, id: \.0) { id, name in
                            Text(name).tag(id)
                        }
                    }
                    // #56: the desktop window's rows, same ranges and names.
                    Picker("Palette", selection: $palette) {
                        Text("Sumi black").tag(0)
                        Text("Indigo").tag(1)
                        Text("Ochre").tag(2)
                    }
                    .pickerStyle(.segmented)
                    valueSlider("Viscosity", $viscosity, 0...1, "%.2f")
                    valueSlider("Ink feed (pressure)", $inkFeed, 0.1...4, "%.2f")
                    valueSlider("Paper roughness", $roughness, 0...1, "%.2f")
                    if layout == 3 || layout == 4 {
                        valueSlider("Tempo (BPM)", $bpm, 20...300, "%.0f", step: 1)
                        valueSlider("Roll speed", $rollSpeed, 0.02...0.25, "%.4f")
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
                Section("Vortex") {
                    Picker("Vortex profile", selection: $vortexRankine) {
                        Text("Exponential").tag(false)
                        Text("Rankine").tag(true)
                    }
                    .pickerStyle(.segmented)
                    Text(vortexRankine
                         ? "Rankine: a rigid core that spins as a disk — the two-finger "
                           + "twist and the CC-routed vortex both use it."
                         : "Exponential: diffuse, breath-like — the two-finger twist and "
                           + "the CC-routed vortex both use it.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Ripple") {
                    let ampCC = CcMap.route(CcMap.decode(ccMap), for: 7)
                    let frqCC = CcMap.route(CcMap.decode(ccMap), for: 8)
                    intSlider("Amount", $rippleAmount).disabled(ampCC == nil)
                    intSlider("Wavelength", $rippleWavelength).disabled(frqCC == nil)
                    valueSlider("Angle", $rippleAngle, 0...180, "%.0f°", step: 1)
                    Text(ampCC == nil || frqCC == nil
                         ? "Route a CC to the ripple dimensions in the CC map to use these."
                         : "Sent as CC \(ampCC!) / CC \(frqCC!) through the MIDI path (the same "
                           + "route a controller would use).")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Stylus wake") {
                    Picker("Fluid", selection: $wakeViscous) {
                        Text("Inviscid doublet").tag(false)
                        Text("Viscous stroke").tag(true)
                    }
                    .pickerStyle(.segmented)
                    if wakeViscous {
                        HStack {
                            Text("Spread l/a")
                            Slider(value: $wakeSpread, in: 1.5...12, step: 0.1)
                            Text(String(format: "%.1f", wakeSpread)).monospacedDigit()
                        }
                    }
                    Text(wakeViscous
                         ? "The pen's stroke is an impulse in a viscous layer (the 2-D "
                           + "Stokeslet): small spread is sharp and close, large is soft "
                           + "and far-reaching."
                         : "The pen's stroke is the exact potential flow around a rigid tip.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Simulation") {
                    Toggle("Full-resolution simulation", isOn: $fullResolution)
                    Text(fullResolution
                         ? "sim_scale 1.0 — full canvas resolution."
                         : "sim_scale 0.75 — lighter thermals on smaller GPUs.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("CC map") {
                    // #56: the desktop's routing table — any CC, any channel or
                    // "any", to any global dimension. Swipe a row to remove it.
                    let routes = CcMap.decode(ccMap)
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
                        ccMap = CcMap.encode(rs)
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
                        ccMap = CcMap.encode(rs)
                    }
                    Button("Restore default map") { ccMap = "" }
                    Text("Defaults: mod wheel → vortex strength; breath, volume and "
                         + "expression → ink flow; the Airwave's Raise, Glide, Slide, Tilt "
                         + "and Flex; CC 102 / 103 → the ripple.")
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
                Section("Canvas") {
                    // #48 / #51: the same two buttons as Android. The saved print
                    // goes to the Photos library (the settings sheet's own
                    // permission string covers the add-only access).
                    Button("Paper dip — save the print") {
                        SumiCanvasView.shared?.paperDip(savePrint: true)
                    }
                    Button("Paper dip — discard (fresh sheet)") {
                        SumiCanvasView.shared?.paperDip(savePrint: false)
                    }
                    Text("Freezes and snapshots the canvas, then starts a "
                         + "clean sheet. Save writes the print to Photos. The "
                         + "sustain pedal no longer does this in Play mode — it "
                         + "is a musical control there.")
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
        .presentationDetents([.medium])
    }
}

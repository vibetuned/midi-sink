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
    @State private var showSettings = false

    var body: some Scene {
        WindowGroup {
            ZStack(alignment: .topTrailing) {
                SumiCanvas(simScale: fullResolution ? 1.0 : 0.75, layout: layout)
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
                SettingsSheet(fullResolution: $fullResolution, layout: $layout)
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
    @State private var status = ""
    private let statusTimer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    private static let layoutNames: [(UInt32, String)] = [
        (0, "Circle of fifths"),
        (1, "Chromatic grid"),
        (2, "Jankó"),
        (3, "Piano roll (horizontal)"),
        (4, "Piano roll (vertical)"),
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

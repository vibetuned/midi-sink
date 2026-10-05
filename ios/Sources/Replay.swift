// Replay.swift — Phase 9 step 65 (QOL §1, DECISIONS_8 #22–#23): session
// replay on the iPad. The recorder and the player live in the canvas (the
// render thread is the main thread, the one MIDI producer the midiQueue —
// SumiCanvas.swift); this file is what the settings and the banner watch,
// and where the files live: Documents/Replays (visible in Files, shareable),
// the lab's `lab.sumireplay` + `lab.field.bin` for the cross-device gate.
import SwiftUI
import SumiCore
import SumiReplay

final class ReplayStatus: ObservableObject {
    static let shared = ReplayStatus()
    @Published var banner = ""            // non-empty while a replay plays: the source device, platform, backend, app, date
    @Published var recording = false
    @Published var recFrames: UInt32 = 0
    @Published var recSeconds = 0.0
    @Published var playElapsed = 0.0
    @Published var playDuration = 0.0
    @Published var status = ""
    @Published var names: [String] = []
    private var lastTick = -1
    /// Once a second from the frame loop (a published change per frame would redraw the sheet at 120 Hz).
    func tickRec(frames: UInt32, seconds: Double) {
        let s = Int(seconds)
        if s != lastTick { lastTick = s; recFrames = frames; recSeconds = seconds }
    }
    func tickPlay(elapsed: Double, duration: Double) {
        let s = Int(elapsed * 4)
        if s != lastTick { lastTick = s; playElapsed = elapsed; playDuration = duration }
    }
    func refreshNames() { names = Replay.names() }
}

enum Replay {
    static var replaysDir: URL {
        let d = SessionStore.documentsDir.appendingPathComponent("Replays", isDirectory: true)
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d
    }
    static func names() -> [String] {
        ((try? FileManager.default.contentsOfDirectory(atPath: replaysDir.path)) ?? [])
            .filter { $0.hasSuffix(".sumireplay") }.sorted(by: >)   // the stamp sorts: newest first
    }
    static func url(_ name: String) -> URL { replaysDir.appendingPathComponent(name) }
    static func delete(_ name: String) { try? FileManager.default.removeItem(at: url(name)) }
    /// A file from the picker: copied into Replays when it opens as a replay; nil otherwise.
    static func importFile(from src: URL) -> String? {
        let access = src.startAccessingSecurityScopedResource()
        defer { if access { src.stopAccessingSecurityScopedResource() } }
        var name = src.lastPathComponent
        if !name.hasSuffix(".sumireplay") { name += ".sumireplay" }
        let dst = url(name)
        try? FileManager.default.removeItem(at: dst)
        do { try FileManager.default.copyItem(at: src, to: dst) } catch { return nil }
        guard let r = sumi_replay_load(dst.path) else { try? FileManager.default.removeItem(at: dst); return nil }
        sumi_replay_close(r)
        return name
    }
    static func timestamp() -> String {
        var buf = [CChar](repeating: 0, count: 32)
        sumi_replay_timestamp(&buf, 32)
        return String(cString: buf)
    }
    static func fileStamp() -> String { timestamp().replacingOccurrences(of: ":", with: "-") }
    static func deviceModel() -> String {
        var sys = utsname(); uname(&sys)
        return withUnsafePointer(to: &sys.machine) { $0.withMemoryRebound(to: CChar.self, capacity: 256) { String(cString: $0) } }
    }
    static func appVersion() -> String {
        let v = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
        let b = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "?"
        return "\(v)+\(b)"
    }
    /// A C char array of the header, as a String.
    static func cstr<T>(_ field: T) -> String {
        withUnsafePointer(to: field) { $0.withMemoryRebound(to: CChar.self, capacity: MemoryLayout<T>.size) { String(cString: $0) } }
    }
    /// The field as the comparator reads it (`tests/field_dump_compare.c`: w, h as uint32, then float32 RGBA rows).
    static func dumpField(_ inst: OpaquePointer, to url: URL) {
        var w: UInt32 = 0, h: UInt32 = 0
        guard sumi_read_field(inst, nil, 0, &w, &h), w > 0, h > 0 else { return }
        var bytes = [UInt8](repeating: 0, count: Int(w) * Int(h) * 8)
        let ok = bytes.withUnsafeMutableBufferPointer { sumi_read_field(inst, $0.baseAddress, $0.count, &w, &h) }
        guard ok else { return }
        var data = Data(capacity: 8 + Int(w) * Int(h) * 16)
        var ww = w.littleEndian, hh = h.littleEndian
        withUnsafeBytes(of: &ww) { data.append(contentsOf: $0) }
        withUnsafeBytes(of: &hh) { data.append(contentsOf: $0) }
        var floats = [Float](repeating: 0, count: Int(w) * Int(h) * 4)
        bytes.withUnsafeBytes { raw in
            let halves = raw.bindMemory(to: UInt16.self)
            for i in 0..<floats.count { floats[i] = Float(Float16(bitPattern: UInt16(littleEndian: halves[i]))) }
        }
        floats.withUnsafeBytes { data.append(contentsOf: $0) }
        try? data.write(to: url)
        NSLog("[replay] field dump %ux%u -> %@", w, h, url.lastPathComponent)
    }
}

/// The settings page (SettingsPages.swift's family).
struct ReplayPage: View {
    @ObservedObject private var st = ReplayStatus.shared
    @State private var importing = false
    var body: some View {
        Form {
            Section {
                if !st.banner.isEmpty {
                    Text(st.banner).font(.footnote)
                    ProgressView(value: st.playDuration > 0 ? min(1, st.playElapsed / st.playDuration) : 0) {
                        Text(String(format: "%.1f / %.1f s", st.playElapsed, st.playDuration)).font(.footnote)
                    }
                    Button("Stop replay") { SumiCanvasView.shared?.stopReplay(finished: false) }
                } else if st.recording {
                    Text(String(format: "Recording: %u frames, %.1f s", st.recFrames, st.recSeconds))
                    Button("Stop recording") { SumiCanvasView.shared?.stopRecording() }
                } else {
                    Button("Record") { SumiCanvasView.shared?.startRecording() }
                }
                Note("Record keeps the sheet, dips, then writes everything the engine sees — the bytes, the gestures, the settings — "
                     + "frame by frame. The file replays on the desktop, the Tab and here, the banner naming the source; a replay "
                     + "runs on the recorded frame clock at this screen's size and palette, re-sounds through Voxo, and leaves its "
                     + "sheet for you to dip and print.")
            }
            Section("Recordings") {
                if st.names.isEmpty { Note("No recordings yet.") }
                ForEach(st.names, id: \.self) { name in
                    HStack {
                        Button(name) { SumiCanvasView.shared?.playReplay(Replay.url(name)) }
                        Spacer()
                        ShareLink(item: Replay.url(name)) { Image(systemName: "square.and.arrow.up") }.buttonStyle(.borderless)
                    }
                }
                .onDelete { idx in for i in idx { Replay.delete(st.names[i]) }; st.refreshNames() }
            }
            Section("Files") {
                Button("Import a recording…") { importing = true }
                Note("Recordings live in Files → On My iPad → midi-sink → Replays; an imported one is copied there. The same file "
                     + "plays on the desktop (Settings → Replay) and the Tab.")
            }
            if !st.status.isEmpty { Section { Note(st.status) } }
        }
        .navigationTitle("Replay")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { st.refreshNames() }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.data, .plainText, .text]) { result in
            switch result {
            case .success(let url):
                st.status = Replay.importFile(from: url).map { "Imported \($0)" } ?? "Not a midi-sink recording: \(url.lastPathComponent)"
                st.refreshNames()
            case .failure(let e):
                st.status = "Import failed: \(e.localizedDescription)"
            }
        }
    }
}

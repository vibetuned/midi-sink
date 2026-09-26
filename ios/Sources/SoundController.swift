// SoundController.swift — the iPad's side of Voxo (Phase 7 step 53, SOUND §4;
// DECISIONS_6 #22–#24). Owns the AVAudioSession (playback, 48 kHz, a
// 128-frame IO buffer — step 48's numbers), starts and stops the device with
// the scene (FOREGROUND ONLY: the sound pauses with the visuals and returns
// with them), handles interruptions (a call, Siri: stop; when it ends with
// "should resume", start again) and route changes (headphones in or out: the
// device restarts on the new route), loads instruments on a background queue
// behind the advisory gate (os_proc_available_memory), and keeps the settings
// — on/off, the volume, the chosen instrument, Local Control.
import AVFoundation
import Combine
import Foundation
import Voxo

final class SoundController: ObservableObject {
    static let shared = SoundController()

    // The settings (UserDefaults; the session file is the visual side's).
    @Published var enabled: Bool { didSet { UserDefaults.standard.set(enabled, forKey: "soundEnabled"); apply(); SumiCanvasView.shared?.refreshInstrumentReach(soundOn: enabled) } }
    @Published var gain: Float { didSet { UserDefaults.standard.set(gain, forKey: "soundGain"); apply() } }
    @Published var localControl: Bool { didSet { UserDefaults.standard.set(localControl, forKey: "soundLocalControl"); SumiCanvasView.shared?.setLocalControl(localControl) } }
    /// The chosen instrument: "demo", or a path relative to Documents/Instruments; "" = the sine.
    @Published var instrument: String { didSet { UserDefaults.standard.set(instrument, forKey: "soundInstrument"); loadInstrument() } }
    // What the shell shows.
    @Published private(set) var report = ""
    @Published private(set) var loading = false
    @Published private(set) var status = ""
    @Published private(set) var memoryAdvice = ""
    @Published private(set) var instruments: [String] = []   // Documents/Instruments, relative paths

    private var active = true
    private var observers: [NSObjectProtocol] = []
    private let loadQueue = DispatchQueue(label: "com.vibetuned.midi-sink.voxo-load", qos: .userInitiated)
    private var loadGeneration = 0

    private init() {
        let d = UserDefaults.standard
        enabled = d.object(forKey: "soundEnabled") as? Bool ?? true          // first launch makes a sound (SOUND §3)
        gain = d.object(forKey: "soundGain") as? Float ?? 0.8
        localControl = d.object(forKey: "soundLocalControl") as? Bool ?? true
        instrument = d.object(forKey: "soundInstrument") as? String ?? "demo"
        let nc = NotificationCenter.default
        observers.append(nc.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { [weak self] n in
            self?.interruption(n)
        })
        observers.append(nc.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil, queue: .main) { [weak self] n in
            self?.routeChange(n)
        })
    }

    private var voxo: OpaquePointer? { SumiCanvasView.shared?.voxo }

    /// The canvas calls this once its Voxo exists: the session, the instrument, the device.
    func canvasReady() {
        SumiCanvasView.shared?.setLocalControl(localControl)
        refreshInstruments()
        loadInstrument()
        apply()
    }

    // MARK: the session and the device

    private func configureSession() {
        let s = AVAudioSession.sharedInstance()
        do {
            try s.setCategory(.playback, mode: .default, options: [.mixWithOthers])
            try s.setPreferredSampleRate(48000)
            try s.setPreferredIOBufferDuration(128.0 / 48000.0)
            try s.setActive(true)
        } catch {
            NSLog("[voxo] session: %@", error.localizedDescription)
        }
    }

    private func apply() {
        guard let v = voxo else { return }
        voxo_set_gain(v, gain)
        let want = enabled && active
        if want && !voxo_running(v) {
            configureSession()
            if voxo_start(v) {
                var st = voxo_stats_t(); voxo_stats(v, &st)
                status = String(format: "%u Hz, %u frames per block", st.sample_rate, st.block_frames)
                NSLog("[voxo] started: %@", status)
            } else {
                status = "No output device; running silent."
            }
        } else if !want && voxo_running(v) {
            voxo_stop(v)
            status = active ? "Off." : "Paused with the app."
            NSLog("[voxo] stopped (%@)", active ? "off" : "background")
        }
    }

    /// The scene phase: foreground only (SOUND §4).
    func setActive(_ on: Bool) {
        active = on
        apply()
    }

    private func interruption(_ n: Notification) {
        guard let raw = n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
              let type = AVAudioSession.InterruptionType(rawValue: raw) else { return }
        switch type {
        case .began:
            NSLog("[voxo] interruption began")
            if let v = voxo, voxo_running(v) { voxo_stop(v); status = "Interrupted." }
        case .ended:
            let opts = AVAudioSession.InterruptionOptions(rawValue: n.userInfo?[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0)
            NSLog("[voxo] interruption ended (resume %d)", opts.contains(.shouldResume) ? 1 : 0)
            if opts.contains(.shouldResume) || active { apply() }
        @unknown default: break
        }
    }

    private func routeChange(_ n: Notification) {
        guard let raw = n.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
              let reason = AVAudioSession.RouteChangeReason(rawValue: raw) else { return }
        switch reason {
        case .newDeviceAvailable, .oldDeviceUnavailable, .override, .routeConfigurationChange:
            NSLog("[voxo] route change (%u): the device restarts", raw)
            if let v = voxo, voxo_running(v) { voxo_stop(v) }
            apply()
        default: break
        }
    }

    // MARK: the instruments

    static var instrumentsDir: URL {
        let d = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Instruments")
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d
    }
    static var demoURL: URL? {
        Bundle.main.resourceURL?.appendingPathComponent("demo/demo.dspreset")
    }

    /// Every .dspreset and .dslibrary under Documents/Instruments, as relative paths.
    /// Both sides are symlink-resolved before the prefix is cut: on iOS the
    /// enumerator hands back /private/var/... while the root reads /var/...,
    /// and a prefix that never matched left absolute paths in the list.
    func refreshInstruments() {
        let root = Self.instrumentsDir
        let rootPath = root.resolvingSymlinksInPath().path
        var found: [String] = []
        if let e = FileManager.default.enumerator(at: root, includingPropertiesForKeys: nil) {
            for case let url as URL in e {
                let ext = url.pathExtension.lowercased()
                guard ext == "dspreset" || ext == "dslibrary" else { continue }
                let full = url.resolvingSymlinksInPath().path
                if full.hasPrefix(rootPath + "/") {
                    found.append(String(full.dropFirst(rootPath.count + 1)))
                } else {
                    found.append(url.lastPathComponent)
                }
            }
        }
        instruments = found.sorted()
        NSLog("[voxo] instruments in %@: %@", rootPath, instruments.joined(separator: ", "))
    }

    /// Copies a picked file or folder into Documents/Instruments; returns the relative path of a preset inside it.
    func importInstrument(from url: URL) -> String? {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let dest = Self.instrumentsDir.appendingPathComponent(url.lastPathComponent)
        do {
            if FileManager.default.fileExists(atPath: dest.path) { try FileManager.default.removeItem(at: dest) }
            try FileManager.default.copyItem(at: url, to: dest)
        } catch {
            status = "Import failed: \(error.localizedDescription)"
            return nil
        }
        refreshInstruments()
        let leaf = url.lastPathComponent
        if leaf.lowercased().hasSuffix(".dslibrary") || leaf.lowercased().hasSuffix(".dspreset") { return leaf }
        return instruments.first { $0.hasPrefix(leaf + "/") }   // a folder: its first preset
    }

    func deleteInstrument(_ rel: String) {
        let top = rel.split(separator: "/").first.map(String.init) ?? rel
        try? FileManager.default.removeItem(at: Self.instrumentsDir.appendingPathComponent(top))
        if instrument == rel { instrument = "" }
        refreshInstruments()
    }

    private func instrumentURL(_ rel: String) -> URL? {
        if rel == "demo" { return Self.demoURL }
        if rel.isEmpty { return nil }
        return Self.instrumentsDir.appendingPathComponent(rel)
    }

    /// The advisory gate's advice: 60% of what the process may still take (os_proc_available_memory).
    private func refreshBudget() -> UInt64 {
        let avail = UInt64(os_proc_available_memory())
        let budget = budgetOverride ?? avail / 10 * 6
        memoryAdvice = budgetOverride != nil
            ? String(format: "Memory advised for instruments: %.0f MB (the lab's --voxo-budget-mb).", Double(budget) / 1048576.0)
            : String(format: "Memory advised for instruments: %.0f MB (of %.0f MB available to the app).",
                     Double(budget) / 1048576.0, Double(avail) / 1048576.0)
        return budget
    }

    private func loadInstrument() {
        guard let v = voxo else { return }
        loadGeneration += 1
        let gen = loadGeneration
        guard let url = instrumentURL(instrument) else {
            voxo_unload_preset(v)
            report = ""
            SumiCanvasView.shared?.refreshInstrumentReach(soundOn: enabled)   // every cell again
            return
        }
        let budget = refreshBudget()
        voxo_set_memory_budget(v, budget)
        loading = true
        report = "Loading \(url.lastPathComponent) …"
        let path = url.path
        loadQueue.async { [weak self] in
            var rep = voxo_report_t()
            let ok = voxo_load_preset(v, path, &rep)
            let raw = rep.text
            let cap = MemoryLayout.size(ofValue: raw)
            let text = withUnsafePointer(to: raw) { p in
                p.withMemoryRebound(to: CChar.self, capacity: cap) { String(cString: $0) }
            }
            DispatchQueue.main.async {
                guard let self, gen == self.loadGeneration else { return }
                self.loading = false
                self.report = ok ? text : "Could not load \(url.lastPathComponent): \(text)"
                NSLog("[voxo] %@", self.report)
                SumiCanvasView.shared?.refreshInstrumentReach(soundOn: self.enabled)   // #27: the play surface's cells
            }
        }
    }

    /// The lab's launch arguments (step 53's evidence): --voxo-budget-mb <n> caps the
    /// advice; --voxo-instrument <relative path | demo> picks the instrument.
    func applyLaunchArguments() {
        let args = CommandLine.arguments
        if let i = args.firstIndex(of: "--voxo-budget-mb"), i + 1 < args.count, let mb = Double(args[i + 1]) {
            budgetOverride = UInt64(mb * 1048576.0)
        }
        if let i = args.firstIndex(of: "--voxo-instrument"), i + 1 < args.count {
            instrument = args[i + 1]
        }
    }
    private var budgetOverride: UInt64? { didSet { if budgetOverride != nil { loadInstrument() } } }
}

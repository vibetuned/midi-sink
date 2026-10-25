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
import SumiPreset   // step 82: a Suzu patch file's block into Voxo's params
import SumiReplay   // step 91: the offline bounce

final class SoundController: ObservableObject {
    static let shared = SoundController()

    // The settings (UserDefaults; the session file is the visual side's).
    @Published var enabled: Bool { didSet { UserDefaults.standard.set(enabled, forKey: "soundEnabled"); apply(); SumiCanvasView.shared?.refreshInstrumentReach(soundOn: enabled) } }
    @Published var gain: Float { didSet { UserDefaults.standard.set(gain, forKey: "soundGain"); apply() } }
    @Published var localControl: Bool { didSet { UserDefaults.standard.set(localControl, forKey: "soundLocalControl"); SumiCanvasView.shared?.setLocalControl(localControl) } }
    /// The chosen instrument: "demo", or a path relative to Documents/Instruments; "" = the sine.
    @Published var instrument: String { didSet { UserDefaults.standard.set(instrument, forKey: "soundInstrument"); loadInstrument() } }
    /// Phase 9 step 63 (step 56's shared UI, consumed): the SOURCE — 0 the sampler, 1 Suzu, 2 both layered —
    /// and Suzu's patch, one of five the shell names (DECISIONS_8 #13). The defaults are Voxo's own: the
    /// bowed harmonic string, the press blowing it (press_blows), so a finger's upward Y sings.
    @Published var source: Int { didSet { UserDefaults.standard.set(source, forKey: "soundSource"); applySource() } }
    @Published var suzuPatch: Int { didSet { UserDefaults.standard.set(suzuPatch, forKey: "soundSuzuPatch"); applySource() } }
    /// Step 82 (TO_PROJECT_SPEC §13.12): a Suzu patch FILE (a midi-sink preset .json with a suzu block — the editable
    /// table rides inside) under Documents/Patches, "" = the table's patch above; and the TUNING, a Scala scale under
    /// Documents/Tunings, "" = equal temperament. Both through the document picker; both persist.
    @Published var patchFile: String { didSet { UserDefaults.standard.set(patchFile, forKey: "soundPatchFile"); applySource() } }
    @Published var tuningFile: String { didSet { UserDefaults.standard.set(tuningFile, forKey: "soundTuningFile"); applyTuning() } }
    @Published var tuningStatus: String = "Equal temperament."
    /// Step 67's app fixes (DECISIONS_9 #10): the patches are Voxo's table — the five of step 63 and the six fitted to
    /// the Versilian Community Sample Library's instruments (#9) — one list for every shell.
    static let suzuPatchNames: [String] = (0..<Int(voxo_suzu_patch_count())).map { String(cString: voxo_suzu_patch_name(UInt32($0))) }
    var patchIndex: Int { min(max(suzuPatch, 0), Self.suzuPatchNames.count - 1) }
    /// Step 67's app fixes (DECISIONS_9 #11): the orbit trace's two routes, as the desktop's Suzu trace section —
    /// the ink into the water at the voice's cell, and the scope on the canvas (0 off, 1 over the water, 2 alone).
    @Published var traceInk: Bool { didSet { UserDefaults.standard.set(traceInk, forKey: "soundTraceInk"); SumiCanvasView.shared?.refreshTrace() } }
    @Published var traceCanvas: Int { didSet { UserDefaults.standard.set(traceCanvas, forKey: "soundTraceCanvas"); SumiCanvasView.shared?.refreshTrace() } }
    /// The trace's scale, canvas heights per unit orbit amplitude (the desktop's knob; 0.25 suits the rotor, the modal voices want more).
    @Published var traceScale: Float { didSet { UserDefaults.standard.set(traceScale, forKey: "soundTraceScale"); SumiCanvasView.shared?.refreshTrace() } }
    /// What sounds, for the settings (the author's fix at step 63): Suzu's patch when the synth is the
    /// source — "Suzu: Trumpet" — the sampler's instrument otherwise, both when layered.
    var activeName: String {
        let lib = instrument == "demo" ? "Dan Tranh (the demo)" : (instrument.isEmpty ? "a sine per voice" : instrument)
        let patch = patchFile.isEmpty ? "Suzu: " + Self.suzuPatchNames[patchIndex] : "Suzu: " + patchFile   // step 82: the file when one is set
        switch source {
        case 1: return patch
        case 2: return patch + " + " + lib
        default: return lib
        }
    }
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
        source = d.object(forKey: "soundSource") as? Int ?? 0
        suzuPatch = d.object(forKey: "soundSuzuPatch") as? Int ?? 0
        patchFile = d.object(forKey: "soundPatchFile") as? String ?? ""     // step 82
        tuningFile = d.object(forKey: "soundTuningFile") as? String ?? ""
        traceInk = d.object(forKey: "soundTraceInk") as? Bool ?? false
        traceCanvas = d.object(forKey: "soundTraceCanvas") as? Int ?? 0
        traceScale = d.object(forKey: "soundTraceScale") as? Float ?? 0.25
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
        applyTuning()   // step 82: the scale before the first patch
        applySource()
        apply()
    }

    /// Step 63: the source and Suzu's patch into Voxo (switching the source ends every voice, as an
    /// instrument swap does). The five patches are Voxo's defaults with the voice kind — and the
    /// modal preset — picked: the bowed harmonic string (the default: the lattice, the bow at 0.15 s,
    /// the press blowing), the bell (the lattice's bell preset), the flute (kind 6), the saxophone (7),
    /// the trumpet (8). A patch a load gate refuses leaves the live one standing (Voxo logs why).
    private func applySource() {
        guard let v = voxo else { return }
        applySource(to: v)
        SumiCanvasView.shared?.refreshInstrumentReach(soundOn: enabled)
        SumiCanvasView.shared?.refreshTrace()   // step 67: the trace follows the source
    }
    /// Step 91: the source and the patch into ANY instance — the live one, or the bounce's deviceless one.
    private func applySource(to v: OpaquePointer) {
        let src: UInt32 = source == 1 ? VOXO_SOURCE_SUZU : (source == 2 ? VOXO_SOURCE_LAYERED : VOXO_SOURCE_SAMPLER)
        voxo_set_source(v, src)
        var sp = voxo_suzu_params_t()
        if !voxo_suzu_patch(UInt32(patchIndex), &sp) { voxo_suzu_default_params(&sp) }   // step 67: the table (DECISIONS_9 #10)
        if !patchFile.isEmpty {   // step 82: the patch file's block over the table's — the editable table rides inside
            let url = Self.patchesDir.appendingPathComponent(patchFile)
            var p = sumi_preset_t()
            if let text = try? String(contentsOf: url, encoding: .utf8), text.withCString({ sumi_preset_read($0, 0, &p) }), p.suzu_present {
                sumi_preset_suzu_to_voxo(&p, &sp)
            } else { NSLog("[voxo] suzu: the patch file could not be read: %@", patchFile) }
        }
        if !voxo_set_suzu_params(v, &sp) { NSLog("[voxo] suzu: the patch was refused by a load gate; the live one stands") }
        NSLog("[voxo] source %d, suzu patch %@", source, Self.suzuPatchNames[patchIndex])
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

    /// Step 82: the patch files and the tunings beside the instruments.
    static var patchesDir: URL { let u = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Patches"); try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true); return u }
    static var tuningsDir: URL { let u = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Tunings"); try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true); return u }
    /// A picked .json with a suzu block: copied into Patches, applied; nil (and a status) otherwise.
    func importPatch(from url: URL) -> String? {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { status = "Could not read \(url.lastPathComponent)"; return nil }
        var p = sumi_preset_t()
        guard text.withCString({ sumi_preset_read($0, 0, &p) }), p.suzu_present else { status = "Not a midi-sink preset with a Suzu patch: \(url.lastPathComponent)"; return nil }
        let dest = Self.patchesDir.appendingPathComponent(url.lastPathComponent)
        do { try text.write(to: dest, atomically: true, encoding: .utf8) } catch { status = "Import failed: \(error.localizedDescription)"; return nil }
        patchFile = url.lastPathComponent
        return patchFile
    }
    /// A picked Scala scale: copied into Tunings, set; Voxo refuses a malformed one with a reason in the log.
    func importTuning(from url: URL) -> String? {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let dest = Self.tuningsDir.appendingPathComponent(url.lastPathComponent)
        do {
            if FileManager.default.fileExists(atPath: dest.path) { try FileManager.default.removeItem(at: dest) }
            try FileManager.default.copyItem(at: url, to: dest)
        } catch { status = "Import failed: \(error.localizedDescription)"; return nil }
        tuningFile = url.lastPathComponent
        return tuningFile
    }
    func applyTuning() {
        guard let v = voxo else { return }
        tuningStatus = applyTuning(to: v)
    }
    /// Step 91: the tuning into any instance; the status line back.
    @discardableResult private func applyTuning(to v: OpaquePointer) -> String {
        if tuningFile.isEmpty { voxo_clear_tuning(v); return "Equal temperament." }
        let path = Self.tuningsDir.appendingPathComponent(tuningFile).path
        if voxo_load_tuning(v, path, nil) { return "\(String(cString: voxo_tuning_name(v))) — \(voxo_tuning_count(v)) degrees (\(tuningFile))." }
        return "The scale was refused (see the log): \(tuningFile)"
    }

    // -- step 91 (TO_PROJECT_SPEC §12.12): the instrument slot and the offline bounce ----------------------------------

    /// The loaded instrument's identity (its name and the FNV-1a 64 of its preset XML), for the session preset's `library` slot.
    var libraryIdentity: (name: String, hash: UInt64)? {
        guard let v = voxo, !instrument.isEmpty, instrument != "demo", !loading else { return nil }
        let h = voxo_preset_hash(v)
        guard h != 0 else { return nil }
        var buf = [CChar](repeating: 0, count: 128)
        voxo_preset_name(v, &buf, buf.count)
        return (String(cString: buf), h)
    }

    /// A preset's library slot resolved against Documents/Instruments — the file of that name first (the library, the
    /// preset, or the preset inside its folder), then any instrument whose preset hashes the same. The sentence when
    /// none does: the preset plays without it.
    func resolveLibrary(name: String, hash: UInt64) {
        refreshInstruments()
        func hashes(_ rel: String) -> Bool {
            guard let url = instrumentURL(rel) else { return false }
            var h: UInt64 = 0
            return voxo_preset_file_hash(url.path, &h, nil, 0) && (hash == 0 || h == hash)
        }
        let byName = instruments.filter { rel in
            let leaf = (rel as NSString).lastPathComponent
            return leaf == name + ".dslibrary" || leaf == name + ".dspreset"
        }
        if let rel = byName.first(where: hashes) ?? (hash != 0 ? instruments.first(where: hashes) : nil) {
            if rel != instrument { instrument = rel }
            return
        }
        if instrument != "" && instrument != "demo" { instrument = "" }
        report = "Library '\(name)' is not on this iPad: the preset plays without it."
        NSLog("[voxo] %@", report)
    }

    /// THE OFFLINE BOUNCE: the replay re-sounded through a deviceless Voxo configured as this controller's live one —
    /// the gain, the dialect, the tempo, the source and the patch, the tuning, the instrument loaded in full — into
    /// Documents/Replays/<name>.wav (32-bit float stereo, 48 kHz, 128-frame blocks). The same replay and session give
    /// the same bytes. Synchronous: call it off the main thread for a large library.
    func bounce(replay url: URL) -> String {
        var cfg = voxo_config_t(); cfg.sample_rate = 48000; cfg.block_frames = 128; cfg.max_voices = 16
        guard let v = voxo_create(&cfg) else { return "Bounce failed: no sound engine" }
        defer { voxo_destroy(v) }
        voxo_set_gain(v, gain)
        voxo_set_input_mode(v, SumiCanvasView.shared?.pendingInputMode ?? 1)
        voxo_set_tempo(v, SumiCanvasView.shared?.session?.params.bpm ?? 120)
        applySource(to: v)
        applyTuning(to: v)
        if let iu = instrumentURL(instrument) {
            var rep = voxo_report_t()
            if !voxo_load_preset(v, iu.path, &rep) { NSLog("[bounce] the instrument did not load: %@", iu.lastPathComponent) }
        }
        let wav = url.deletingPathExtension().appendingPathExtension("wav")
        var info = sumi_replay_bounce_info_t()
        var why = [CChar](repeating: 0, count: 128)
        let push: sumi_replay_push_fn = { user, s, d1, d2, _ in voxo_push_midi(OpaquePointer(user), s, d1, d2) }
        let render: sumi_replay_render_fn = { user, lr, frames in voxo_render(OpaquePointer(user), lr, frames) }
        let raw = UnsafeMutableRawPointer(v)
        let ok = sumi_replay_bounce(url.path, wav.path, 48000, 128, push, raw, render, raw, &info, &why, why.count)
        if !ok { return "Bounce failed: \(String(cString: why))" }
        let line = String(format: "Bounced %u frames (%.2f s, %u events, peak %.3f) to %@", info.frames, info.seconds, info.events, info.peak, wav.lastPathComponent)
        NSLog("[replay] %@", line)
        return line
    }
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
        // step 63's evidence: --voxo-source sampler|suzu|both, --suzu-patch <0..4>
        if let i = args.firstIndex(of: "--voxo-source"), i + 1 < args.count {
            source = args[i + 1] == "suzu" ? 1 : (args[i + 1] == "both" ? 2 : 0)
        }
        if let i = args.firstIndex(of: "--suzu-patch"), i + 1 < args.count, let n = Int(args[i + 1]) {
            suzuPatch = min(max(n, 0), Self.suzuPatchNames.count - 1)
        }
        // step 67's evidence: --trace-ink, --trace-canvas <0|1|2> — the orbit trace's routes
        if args.contains("--trace-ink") { traceInk = true }
        if let i = args.firstIndex(of: "--trace-canvas"), i + 1 < args.count, let n = Int(args[i + 1]) { traceCanvas = min(max(n, 0), 2) }
        if let i = args.firstIndex(of: "--trace-scale"), i + 1 < args.count, let f = Float(args[i + 1]) { traceScale = min(max(f, 0.05), 4) }
    }
    private var budgetOverride: UInt64? { didSet { if budgetOverride != nil { loadInstrument() } } }
}

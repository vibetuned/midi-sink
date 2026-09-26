// Phase 6 step 44a (QOL §1–§3): THE SESSION — everything the shell holds that
// is not the field: the core params (one sumi_params_t, every field), the
// custom palette slot, the CC map, the input dialect, the routed controls'
// values and the strip's latch-wheel assignments. It is persisted through
// the ONE preset serializer (presets/, `import SumiPreset`): the last session
// is Application Support/last_session.json, restored at launch; named presets
// are Documents/Presets/<name>.json (visible in Files, the same file the
// desktop and the web read). The shell-only switches (Play mode, outbound
// transports, the sustain latch) stay @AppStorage in SumiApp.
import Foundation
import SwiftUI
import SumiCore
import SumiPreset

// -- C-struct helpers --------------------------------------------------------

/// Byte equality of an imported C POD (sumi_params_t / sumi_palette_t carry no padding).
func podEqual<T>(_ a: T, _ b: T) -> Bool {
    var a = a, b = b
    return withUnsafeBytes(of: &a) { pa in withUnsafeBytes(of: &b) { pb in pa.elementsEqual(pb) } }
}

func linToSrgb(_ v: Float) -> Float {
    let c = min(max(v, 0), 1)
    return c <= 0.0031308 ? 12.92 * c : 1.055 * powf(c, 1.0 / 2.4) - 0.055
}
func srgbToLin(_ v: Float) -> Float {
    let c = min(max(v, 0), 1)
    return c <= 0.04045 ? c / 12.92 : powf((c + 0.055) / 1.055, 2.4)
}

/// A linear-RGB triple as a SwiftUI colour (the pickers speak sRGB, the core linear).
func colorFromLinear(_ r: Float, _ g: Float, _ b: Float) -> Color {
    Color(.sRGB, red: Double(linToSrgb(r)), green: Double(linToSrgb(g)), blue: Double(linToSrgb(b)))
}
func linearFromColor(_ c: Color) -> (Float, Float, Float) {
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    UIColor(c).getRed(&r, green: &g, blue: &b, alpha: &a)   // extended sRGB; clamped below
    return (srgbToLin(Float(r)), srgbToLin(Float(g)), srgbToLin(Float(b)))
}

extension sumi_palette_t {
    func stop(_ i: Int) -> sumi_palette_stop_t {
        var s = stops
        return withUnsafeBytes(of: &s) { $0.bindMemory(to: sumi_palette_stop_t.self)[i] }
    }
    mutating func setStop(_ i: Int, _ v: sumi_palette_stop_t) {
        withUnsafeMutableBytes(of: &stops) { $0.bindMemory(to: sumi_palette_stop_t.self)[i] = v }
    }
}

extension sumi_params_t {
    var paperTint: (Float, Float, Float) {
        get { (paper_tint.0, paper_tint.1, paper_tint.2) }
        set { paper_tint = (newValue.0, newValue.1, newValue.2) }
    }
}

// -- the session ---------------------------------------------------------------

/// The routed controls the shell sends through the MIDI path (the desktop's
/// list, presets' `controls`): ripple amount / wavelength, the Chladni stir
/// and balance, the spark frequency, the Chirikov throw.
let sessionControlDefaults: [UInt32: Int] = [7: 0, 8: 32, 16: 0, 17: 0, 18: 64, 19: 0]

final class SessionStore: ObservableObject {
    @Published var params = sumi_params_t()
    @Published var palette = sumi_palette_t()
    @Published var ccRoutes: [CcRoute] = CcMap.defaults
    @Published var inputMode = 1
    @Published var controls: [UInt32: Int] = sessionControlDefaults
    @Published var stripAssignA: UInt8 = 0   // 0 = unset (hostmpe's default stands)
    @Published var stripAssignB: UInt8 = 0
    /// False until the canvas has created the instance and handed over the core's defaults.
    @Published private(set) var ready = false
    @Published private(set) var presetNames: [String] = []

    private(set) var coreDefaults = sumi_params_t()
    private(set) var paletteDefault = sumi_palette_t()
    private var saveWork: DispatchWorkItem?
    private var changeSink: Any?

    static var supportDir: URL {
        let d = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d
    }
    static var documentsDir: URL { FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0] }
    static var presetsDir: URL {
        let d = documentsDir.appendingPathComponent("Presets", isDirectory: true)
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d
    }
    static var sessionURL: URL { supportDir.appendingPathComponent("last_session.json") }

    init() {
        refreshPresetNames()
        // Every change after attach re-writes the last session, debounced (a slider drag is one write).
        changeSink = objectWillChange.sink { [weak self] _ in
            guard let self, self.ready else { return }
            self.saveWork?.cancel()
            let w = DispatchWorkItem { [weak self] in self?.saveSession() }
            self.saveWork = w
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.8, execute: w)
        }
    }

    /// Called once by the canvas right after sumi_create: the core's own
    /// defaults are what a preset's missing keys fall back to. Restores the
    /// last session, or — on the first 1.1 launch — migrates the 1.0 rows.
    func attach(coreDefaults d: sumi_params_t, paletteDefault pd: sumi_palette_t) {
        guard !ready else { return }
        coreDefaults = d
        paletteDefault = pd
        var p = defaultPreset()
        if let text = try? String(contentsOf: Self.sessionURL, encoding: .utf8) {
            _ = readPreset(text, into: &p)
        } else {
            migrateLegacy(&p)
        }
        fromPreset(p)
        ready = true
        saveSession()
    }

    /// The core's defaults plus the host's: the device's sim_scale, the default
    /// CC map, the controls at rest, MPE.
    func defaultPreset() -> sumi_preset_t {
        var d = coreDefaults
        d.sim_scale = SumiCanvasView.defaultsToFullResolution ? 1.0 : 0.75
        var pd = paletteDefault
        var p = sumi_preset_t()
        sumi_preset_init(&p, &d, &pd)
        p.input_mode = 1
        setCC(&p, CcMap.defaults)
        setControls(&p, sessionControlDefaults)
        return p
    }

    // -- preset <-> session ----------------------------------------------------

    func toPreset(name: String) -> sumi_preset_t {
        var p = defaultPreset()
        p.params = params
        p.palette = palette
        p.input_mode = UInt32(min(3, max(1, inputMode)))
        setCC(&p, ccRoutes)
        setControls(&p, controls)
        p.strip_assign_a = stripAssignA
        p.strip_assign_b = stripAssignB
        withUnsafeMutableBytes(of: &p.name) { buf in
            buf.initializeMemory(as: UInt8.self, repeating: 0)
            let bytes = Array(name.utf8.prefix(Int(SUMI_PRESET_NAME_MAX) - 1))
            for (i, b) in bytes.enumerated() { buf[i] = b }
        }
        return p
    }

    func fromPreset(_ p: sumi_preset_t) {
        params = p.params
        palette = p.palette
        inputMode = (1...3).contains(Int(p.input_mode)) ? Int(p.input_mode) : 1
        var routes: [CcRoute] = []
        var cc = p.cc
        withUnsafeBytes(of: &cc) { raw in
            let arr = raw.bindMemory(to: sumi_preset_cc_t.self)
            for i in 0..<min(Int(p.cc_count), Int(SUMI_PRESET_MAX_CC)) {
                let r = arr[i]
                if r.cc < 128, r.target < 20 || (1000...1005).contains(r.target), r.channel == 0xFF || r.channel < 16 {   // step 54: Voxo's bus targets survive the reload
                    routes.append(CcRoute(channel: r.channel, cc: r.cc, target: r.target))
                }
            }
        }
        ccRoutes = routes
        var ctl = sessionControlDefaults
        var cs = p.controls
        withUnsafeBytes(of: &cs) { raw in
            let arr = raw.bindMemory(to: sumi_preset_control_t.self)
            for i in 0..<min(Int(p.control_count), Int(SUMI_PRESET_MAX_CONTROLS)) {
                ctl[arr[i].ctl] = Int(min(arr[i].value, 127))
            }
        }
        controls = ctl
        stripAssignA = p.strip_assign_a
        stripAssignB = p.strip_assign_b
    }

    private func setCC(_ p: inout sumi_preset_t, _ routes: [CcRoute]) {
        let n = min(routes.count, Int(SUMI_PRESET_MAX_CC))
        withUnsafeMutableBytes(of: &p.cc) { raw in
            let arr = raw.bindMemory(to: sumi_preset_cc_t.self)
            for i in 0..<n { arr[i] = sumi_preset_cc_t(channel: routes[i].channel, cc: routes[i].cc, target: routes[i].target) }
        }
        p.cc_count = UInt32(n)
    }

    private func setControls(_ p: inout sumi_preset_t, _ c: [UInt32: Int]) {
        let items = c.sorted { $0.key < $1.key }.prefix(Int(SUMI_PRESET_MAX_CONTROLS))
        withUnsafeMutableBytes(of: &p.controls) { raw in
            let arr = raw.bindMemory(to: sumi_preset_control_t.self)
            for (i, kv) in items.enumerated() { arr[i] = sumi_preset_control_t(ctl: kv.key, value: UInt8(min(127, max(0, kv.value)))) }
        }
        p.control_count = UInt32(items.count)
    }

    func writePreset(name: String) -> String {
        var p = toPreset(name: name)
        let need = sumi_preset_write(&p, sumi_version(), nil, 0)
        var buf = [CChar](repeating: 0, count: need + 1)
        sumi_preset_write(&p, sumi_version(), &buf, buf.count)
        return String(cString: buf)
    }

    /// Reads over `p` (keys the file lacks keep p's values); false = not a preset.
    func readPreset(_ text: String, into p: inout sumi_preset_t) -> Bool {
        text.withCString { sumi_preset_read($0, 0, &p) }
    }

    func saveSession() {
        guard ready else { return }
        try? writePreset(name: "last session").write(to: Self.sessionURL, atomically: true, encoding: .utf8)
    }

    // -- named presets (Documents/Presets) -------------------------------------

    static func safeName(_ n: String) -> String {
        let bad = CharacterSet(charactersIn: "/\\:\"<>|?*")
        let s = n.unicodeScalars.map { bad.contains($0) ? "_" : String($0) }.joined()
            .trimmingCharacters(in: .whitespaces)
        return s.isEmpty ? "preset" : s
    }
    static func presetURL(_ name: String) -> URL { presetsDir.appendingPathComponent(safeName(name) + ".json") }

    func refreshPresetNames() {
        let files = (try? FileManager.default.contentsOfDirectory(at: Self.presetsDir, includingPropertiesForKeys: nil)) ?? []
        presetNames = files.filter { $0.pathExtension.lowercased() == "json" }
            .map { $0.deletingPathExtension().lastPathComponent }.sorted()
    }

    @discardableResult
    func savePreset(named name: String) -> Bool {
        let ok = (try? writePreset(name: name).write(to: Self.presetURL(name), atomically: true, encoding: .utf8)) != nil
        refreshPresetNames()
        return ok
    }

    /// Applies preset text over the session as it stands (the schema rule:
    /// missing keys keep what is playing, unknown keys are skipped).
    @discardableResult
    func load(text: String) -> Bool {
        var p = toPreset(name: "")
        guard readPreset(text, into: &p) else { return false }
        fromPreset(p)
        return true
    }

    @discardableResult
    func loadPreset(named name: String) -> Bool {
        guard let text = try? String(contentsOf: Self.presetURL(name), encoding: .utf8) else { return false }
        return load(text: text)
    }

    func deletePreset(named name: String) {
        try? FileManager.default.removeItem(at: Self.presetURL(name))
        refreshPresetNames()
    }

    /// Import from Files: applied, and kept in the library under its own name.
    func importPreset(from url: URL) -> String? {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        guard let text = try? String(contentsOf: url, encoding: .utf8), load(text: text) else { return nil }
        var probe = sumi_preset_t()
        _ = readPreset(text, into: &probe)
        var nm = probe.name
        let named = withUnsafeBytes(of: &nm) { String(decoding: $0.prefix { $0 != 0 }, as: UTF8.self) }
        let name = named.isEmpty || named == "last session" ? url.deletingPathExtension().lastPathComponent : named
        try? text.write(to: Self.presetURL(name), atomically: true, encoding: .utf8)
        refreshPresetNames()
        return name
    }

    /// The session as a file to share (the share sheet's item).
    func exportFile(named name: String) -> URL? {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(Self.safeName(name) + ".json")
        return (try? writePreset(name: name).write(to: url, atomically: true, encoding: .utf8)) != nil ? url : nil
    }

    // -- the 1.0 rows (@AppStorage) → the session, once ------------------------

    private func migrateLegacy(_ p: inout sumi_preset_t) {
        let ud = UserDefaults.standard
        func has(_ k: String) -> Bool { ud.object(forKey: k) != nil }
        if has("palette") { p.params.active_palette_id = UInt32(min(2, max(0, ud.integer(forKey: "palette")))) }
        if has("viscosity") { p.params.fluid_viscosity = Float(ud.double(forKey: "viscosity")) }
        if has("inkFeed") { p.params.expansion_rate = Float(ud.double(forKey: "inkFeed")) }
        if has("roughness") { p.params.paper_roughness = Float(ud.double(forKey: "roughness")) }
        if has("bpm") { p.params.bpm = Float(ud.double(forKey: "bpm")) }
        if has("rollSpeed") { p.params.roll_speed = Float(ud.double(forKey: "rollSpeed")) }
        if has("vortexRankine") { p.params.vortex_profile = ud.bool(forKey: "vortexRankine") ? 1 : 0 }
        if has("rippleAngle") { p.params.ripple_angle = Float(ud.double(forKey: "rippleAngle")) / 57.29578 }
        if has("slidePinch") { p.params.slide_mode = ud.bool(forKey: "slidePinch") ? 1 : 0 }
        if has("pinchCrossed") { p.params.pinch_variant = ud.bool(forKey: "pinchCrossed") ? 1 : 0 }
        if has("pressSwirl") { p.params.press_mode = ud.bool(forKey: "pressSwirl") ? 1 : 0 }
        if has("bendRipple") {
            let r = ud.bool(forKey: "bendRipple")
            p.params.bend_mode = r ? 1 : 0
            p.params.ripple_bake = r ? 1 : 0
        }
        if has("wakeViscous") { p.params.wake_profile = ud.bool(forKey: "wakeViscous") ? 1 : 0 }
        if has("wakeSpread") { p.params.wake_spread = Float(ud.double(forKey: "wakeSpread")) }
        if has("inputMode") { p.input_mode = UInt32(min(3, max(1, ud.integer(forKey: "inputMode")))) }
        if has("ccMap") { setCC(&p, CcMap.decode(ud.string(forKey: "ccMap") ?? "")) }
        var c = sessionControlDefaults
        if has("rippleAmount") { c[7] = ud.integer(forKey: "rippleAmount") }
        if has("rippleWavelength") { c[8] = ud.integer(forKey: "rippleWavelength") }
        setControls(&p, c)
    }

    // -- bindings the settings use ---------------------------------------------

    func control(_ ctl: UInt32) -> Binding<Int> {
        Binding(get: { self.controls[ctl] ?? 0 }, set: { self.controls[ctl] = min(127, max(0, $0)) })
    }
    func route(for ctl: UInt32) -> UInt8? { CcMap.route(ccRoutes, for: ctl) }
}

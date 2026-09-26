// Phase 6 step 44a (MEDIUM §1; QOL §1–§4): the settings sheet's new pages —
// the palette library and editor, the substrate, presets (Files export and
// import), the print ledger and the Phase-6 operators. Every row is the
// desktop settings window's (desktop/src/settings_ui.cpp), with the same
// names and ranges; the sheet sits at the medium detent so the canvas stays
// in view while a palette or substrate is edited live.
import SwiftUI
import UIKit
import UniformTypeIdentifiers
import SumiCore

// -- small rows ------------------------------------------------------------------

struct FloatRow: View {
    let label: String
    @Binding var value: Float
    let range: ClosedRange<Float>
    let fmt: String
    var step: Float? = nil
    var body: some View {
        HStack {
            Text(label)
            if let step {
                Slider(value: $value, in: range, step: step)
            } else {
                Slider(value: $value, in: range)
            }
            Text(String(format: fmt, value)).monospacedDigit().frame(minWidth: 64, alignment: .trailing)
        }
    }
}

struct Note: View {
    let text: String
    init(_ t: String) { text = t }
    var body: some View { Text(text).font(.footnote).foregroundStyle(.secondary) }
}

/// A 0..127 routed control (sent as its CC), disabled while no CC is routed to it.
struct ControlRow: View {
    @ObservedObject var session: SessionStore
    let label: String
    let ctl: UInt32
    var body: some View {
        let v = session.control(ctl)
        HStack {
            Text(label)
            Slider(value: Binding(get: { Double(v.wrappedValue) }, set: { v.wrappedValue = Int($0.rounded()) }),
                   in: 0...127, step: 1)
            Text("\(v.wrappedValue)").monospacedDigit().frame(minWidth: 40, alignment: .trailing)
        }
        .disabled(session.route(for: ctl) == nil)
    }
}

// -- the palette (QOL §1) ----------------------------------------------------------

struct PalettePage: View {
    @ObservedObject var session: SessionStore
    @State private var libraryPick = 0

    private var medium: UInt32 { session.params.medium == SUMI_MEDIUM_ANOD.rawValue ? 1 : 0 }
    private var anod: Bool { medium == 1 }

    static func presetName(_ medium: UInt32, _ i: UInt32) -> String {
        var nm: UnsafePointer<CChar>? = nil
        guard sumi_palette_preset(medium, i, nil, &nm), let nm else { return "?" }
        return String(cString: nm)
    }
    static func activeName(_ s: SessionStore) -> String {
        let m: UInt32 = s.params.medium == SUMI_MEDIUM_ANOD.rawValue ? 1 : 0
        let id = s.params.active_palette_id
        return id >= SUMI_PALETTE_CUSTOM ? "Custom" : presetName(m, id)
    }

    private func stopColor(_ i: Int) -> Binding<Color> {
        Binding(get: {
            let st = session.palette.stop(i)
            return colorFromLinear(st.rgb.0, st.rgb.1, st.rgb.2)
        }, set: { c in
            var st = session.palette.stop(i)
            let (r, g, b) = linearFromColor(c)
            st.rgb = (r, g, b)
            session.palette.setStop(i, st)
        })
    }
    private func stopPosition(_ i: Int) -> Binding<Float> {
        Binding(get: { session.palette.stop(i).position }, set: { v in
            var st = session.palette.stop(i)
            st.position = v
            session.palette.setStop(i, st)
        })
    }
    private func tripleColor(_ get: @escaping () -> (Float, Float, Float), _ set: @escaping ((Float, Float, Float)) -> Void) -> Binding<Color> {
        Binding(get: { let t = get(); return colorFromLinear(t.0, t.1, t.2) }, set: { set(linearFromColor($0)) })
    }

    var body: some View {
        let n = sumi_palette_preset_count(medium)
        Form {
            Section {
                Picker("Active", selection: $session.params.active_palette_id) {
                    ForEach(0..<Int(min(3, n)), id: \.self) { i in
                        Text(Self.presetName(medium, UInt32(i))).tag(UInt32(i))
                    }
                    Text("Custom").tag(SUMI_PALETTE_CUSTOM)
                }
                Note("The medium's three built-in palettes and the custom slot. A palette-morph CC travels "
                     + "the ring from the active one — built-in to built-in, or custom to the three built-ins.")
            }
            Section("Library") {
                Picker("Preset", selection: $libraryPick) {
                    ForEach(0..<Int(n), id: \.self) { i in Text(Self.presetName(medium, UInt32(i))).tag(i) }
                }
                Button("Load into custom") {
                    var pal = sumi_palette_t()
                    if sumi_palette_preset(medium, UInt32(libraryPick), &pal, nil) {
                        session.palette = pal
                        session.params.active_palette_id = SUMI_PALETTE_CUSTOM
                    }
                }
                Note("The curated library: the built-ins and the colour-blind-considerate presets (Cobalt & "
                     + "amber — the Okabe–Ito pair — Viridis and Cividis). Loading one fills the custom slot as "
                     + "a starting point; every palette renders through the same path.")
            }
            if session.params.active_palette_id == SUMI_PALETTE_CUSTOM {
                let count = Int(min(max(session.palette.stop_count, 2), UInt32(SUMI_PALETTE_MAX_STOPS)))
                Section(anod ? "The glow: dim charge to burning charge" : "The ink: thin to pooled") {
                    ForEach(0..<count, id: \.self) { i in
                        let label = i == 0 ? (anod ? "Dim" : "Thin") : (i == count - 1 ? (anod ? "Burning" : "Pooled") : "Stop \(i + 1)")
                        HStack {
                            ColorPicker(label, selection: stopColor(i), supportsOpacity: false)
                            if i > 0 && i < count - 1 {
                                Slider(value: stopPosition(i), in: 0...1).frame(maxWidth: 160)
                                Text(String(format: "at %.2f", session.palette.stop(i).position)).monospacedDigit()
                            }
                        }
                    }
                    HStack {
                        Button("Add stop") {
                            var pal = session.palette
                            let c = Int(pal.stop_count)
                            guard c < Int(SUMI_PALETTE_MAX_STOPS) else { return }
                            // insert before the last stop, midway between its neighbours (the desktop's rule)
                            let last = pal.stop(c - 1), prev = pal.stop(c - 2)
                            pal.setStop(c, last)
                            var mid = last
                            mid.rgb = ((prev.rgb.0 + last.rgb.0) / 2, (prev.rgb.1 + last.rgb.1) / 2, (prev.rgb.2 + last.rgb.2) / 2)
                            mid.position = (prev.position + 1) / 2
                            pal.setStop(c - 1, mid)
                            pal.stop_count = UInt32(c + 1)
                            session.palette = pal
                        }
                        .disabled(count >= Int(SUMI_PALETTE_MAX_STOPS))
                        .buttonStyle(.borderless)
                        Spacer()
                        Button("Remove stop") {
                            var pal = session.palette
                            let c = Int(pal.stop_count)
                            guard c > 2 else { return }
                            pal.setStop(c - 2, pal.stop(c - 1))
                            pal.stop_count = UInt32(c - 1)
                            session.palette = pal
                        }
                        .disabled(count <= 2)
                        .buttonStyle(.borderless)
                    }
                }
                Section("Depth & drift") {
                    FloatRow(label: "Depth curve", value: $session.palette.depth_gamma, range: 0.25...4, fmt: "%.2f")
                    Note(anod ? "How the glow walks the ramp: below 1 the charge burns early, above 1 late."
                              : "How the ink's thickness walks the ramp: below 1 thin ink is already deep, above 1 only pooled ink is.")
                    FloatRow(label: "Depth floor", value: $session.palette.depth_floor, range: 0...1, fmt: "%.2f")
                    FloatRow(label: "Hue drift", value: $session.palette.hue_drift, range: 0...1, fmt: "%.2f")
                    ColorPicker(anod ? "Drift toward (the halo)" : "Drift toward",
                                selection: tripleColor({ (session.palette.accent_rgb.0, session.palette.accent_rgb.1, session.palette.accent_rgb.2) },
                                                       { session.palette.accent_rgb = $0 }),
                                supportsOpacity: false)
                    Note("Every drop takes a slightly different hue: its selector blends the ramp's colour toward "
                         + "this one, by up to the drift. In Anod the charge phase bands the filament between the "
                         + "ramp and this colour.")
                    if !anod {
                        ColorPicker("Clear water",
                                    selection: tripleColor({ (session.palette.clear_rgb.0, session.palette.clear_rgb.1, session.palette.clear_rgb.2) },
                                                           { session.palette.clear_rgb = $0 }),
                                    supportsOpacity: false)
                    }
                }
            }
        }
        .navigationTitle("Palette")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// -- the substrate (QOL §2) --------------------------------------------------------

struct SubstratePage: View {
    @ObservedObject var session: SessionStore
    private var anod: Bool { session.params.medium == SUMI_MEDIUM_ANOD.rawValue }
    // the desktop's presets (settings_ui.cpp), verbatim
    private static let tints: [(String, (Float, Float, Float))] = [
        ("Cream (washi)", (0.900, 0.868, 0.790)), ("White", (0.955, 0.950, 0.935)), ("Toned", (0.760, 0.690, 0.560))]
    private static let papers: [(String, Float, Float)] = [("Smooth", 0.25, 1.4), ("Washi", 0.5, 1.0), ("Coarse", 0.8, 0.7)]

    var body: some View {
        Form {
            if anod {
                Section("The glass") {
                    FloatRow(label: "Glass darkness", value: $session.params.anod_dark, range: 0...1, fmt: "%.2f")
                    Note("The vacuum glass under the discharge: 1 black, 0.5 the step-42 near-black, 0 twice as bright.")
                    FloatRow(label: "Phosphor grain", value: $session.params.anod_grain, range: 0...1, fmt: "%.2f")
                    Note("The speckle in the glass, screen-locked.")
                }
                Section("The glow") {
                    FloatRow(label: "Glow bloom", value: $session.params.anod_bloom, range: 0...3, fmt: "%.2f")
                    Stepper("Glow reach: \(session.params.anod_bloom_levels) octaves",
                            value: Binding(get: { Int(session.params.anod_bloom_levels) },
                                           set: { session.params.anod_bloom_levels = UInt32($0) }), in: 1...5)
                    Note("The discharge blooms like a photograph in a lens: the emission is blurred over the reach "
                         + "and added back, and the brightest filaments clip toward white. 0 is the plain composite. "
                         + "Prints and exports bloom the same.")
                    FloatRow(label: "Glow scale", value: $session.params.anod_glow, range: 0.2...5, fmt: "%.2f")
                    Note("The strain a texel needs to glow: smaller = hotter, sooner.")
                    let lines = session.params.anod_pitch > 0 ? 1 / session.params.anod_pitch : 0
                    HStack {
                        Text("Grid lines")
                        Slider(value: Binding(get: { Double(lines) }, set: { v in
                            session.params.anod_pitch = v < 8 ? 0 : Float(1 / min(256, v))
                        }), in: 0...256, step: 1)
                        Text(lines < 8 ? "Off" : String(format: "%.0f", lines)).monospacedDigit().frame(minWidth: 44, alignment: .trailing)
                    }
                    Note("How many grid lines the displaced water would show across the canvas height; Off leaves the water glass.")
                    FloatRow(label: "Strike charge", value: $session.params.anod_drop, range: 0.1...1, fmt: "%.2f×")
                    Note("The charge a strike seeds, as a fraction of the Sumi drop; the burst and the spark shear play "
                         + "on it (the classic spark). Small charges are torn into threads this renderer loses; 1 floods.")
                }
            } else {
                Section("Paper tint") {
                    Picker("Tint", selection: Binding(get: {
                        Self.tints.firstIndex { t in
                            abs(t.1.0 - session.params.paper_tint.0) < 1e-4 && abs(t.1.1 - session.params.paper_tint.1) < 1e-4 &&
                            abs(t.1.2 - session.params.paper_tint.2) < 1e-4 } ?? 3
                    }, set: { i in if i < 3 { session.params.paperTint = Self.tints[i].1 } })) {
                        ForEach(0..<3, id: \.self) { Text(Self.tints[$0].0).tag($0) }
                        Text("Custom").tag(3)
                    }
                    ColorPicker("Custom tint", selection: Binding(get: {
                        let t = session.params.paperTint; return colorFromLinear(t.0, t.1, t.2)
                    }, set: { session.params.paperTint = linearFromColor($0) }), supportsOpacity: false)
                    Note("The washi's base tone; the mottle, grain and fibers ride on it.")
                }
                Section("Paper") {
                    Picker("Paper", selection: Binding(get: {
                        Self.papers.firstIndex { abs($0.1 - session.params.paper_roughness) < 1e-4 && abs($0.2 - session.params.fiber_scale) < 1e-4 } ?? 3
                    }, set: { i in if i < 3 { session.params.paper_roughness = Self.papers[i].1; session.params.fiber_scale = Self.papers[i].2 } })) {
                        ForEach(0..<3, id: \.self) { Text(Self.papers[$0].0).tag($0) }
                        Text("Custom").tag(3)
                    }
                    .pickerStyle(.segmented)
                    FloatRow(label: "Roughness", value: $session.params.paper_roughness, range: 0...1, fmt: "%.2f")
                    FloatRow(label: "Fiber scale", value: $session.params.fiber_scale, range: 0.5...2, fmt: "%.2f×")
                    Note("Roughness is the strength of the mottle, grain and fiber strands (a CC can ride it live); "
                         + "fiber scale their fineness — below 1 longer, coarser strands, above 1 finer.")
                }
            }
        }
        .navigationTitle(anod ? "Glass & glow" : "Paper")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// -- presets (QOL §3) --------------------------------------------------------------

struct PresetsPage: View {
    @ObservedObject var session: SessionStore
    @State private var newName = ""
    @State private var status = ""
    @State private var importing = false
    @State private var confirmLoad: String? = nil

    var body: some View {
        Form {
            Section {
                HStack {
                    TextField("Name", text: $newName).textInputAutocapitalization(.never)
                    Button("Save") {
                        let n = newName.trimmingCharacters(in: .whitespaces)
                        guard !n.isEmpty else { return }
                        status = session.savePreset(named: n) ? "Saved '\(n)'" : "Could not write '\(n)'"
                        newName = ""
                    }
                    .disabled(newName.trimmingCharacters(in: .whitespaces).isEmpty)
                }
                Note("A preset is the whole session but the ink: every parameter, the input dialect, the custom "
                     + "palette, the CC map, the control values and the strip's wheel assignments. The last session "
                     + "is saved on every change and restored at launch.")
            }
            Section("Saved") {
                if session.presetNames.isEmpty { Note("No saved presets yet.") }
                ForEach(session.presetNames, id: \.self) { name in
                    HStack {
                        Button(name) { confirmLoad = name }
                        Spacer()
                        ShareLink(item: SessionStore.presetURL(name)) { Image(systemName: "square.and.arrow.up") }
                            .buttonStyle(.borderless)
                    }
                }
                .onDelete { idx in for i in idx { session.deletePreset(named: session.presetNames[i]) } }
            }
            Section("Files") {
                Button("Import a preset…") { importing = true }
                if let url = session.exportFile(named: newName.isEmpty ? "midi-sink session" : newName) {
                    ShareLink("Export this session…", item: url)
                }
                Note("The same JSON loads on the desktop and the web (presets/SCHEMA.md). Saved presets live in "
                     + "Files → On My iPad → midi-sink → Presets; an imported one is applied and added to the list.")
            }
            if !status.isEmpty { Section { Note(status) } }
        }
        .navigationTitle("Presets")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { session.refreshPresetNames() }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.json]) { result in
            switch result {
            case .success(let url):
                status = session.importPreset(from: url).map { "Imported '\($0)'" } ?? "Not a midi-sink preset: \(url.lastPathComponent)"
            case .failure(let e):
                status = "Import failed: \(e.localizedDescription)"
            }
        }
        .confirmationDialog("Load '\(confirmLoad ?? "")'?", isPresented: Binding(get: { confirmLoad != nil }, set: { if !$0 { confirmLoad = nil } }),
                            titleVisibility: .visible) {
            Button("Load") {
                if let n = confirmLoad { status = session.loadPreset(named: n) ? "Loaded '\(n)'" : "Could not read '\(n)'" }
                confirmLoad = nil
            }
        } message: {
            Note("The session's settings are replaced; the ink on the canvas stays.")
        }
    }
}

// -- the sound (Phase 7 step 53, SOUND §4) -----------------------------------------

struct SoundPage: View {
    @ObservedObject var sound = SoundController.shared
    @State private var importing = false
    @State private var confirmDelete: String? = nil

    var body: some View {
        Form {
            Section {
                Toggle("Internal sound (Voxo)", isOn: $sound.enabled)
                HStack {
                    Text("Volume")
                    Slider(value: Binding(get: { Double(sound.gain) }, set: { sound.gain = Float($0) }), in: 0...1.5)
                    Text(String(format: "%.2f", sound.gain)).monospacedDigit().frame(minWidth: 44, alignment: .trailing)
                }
                Note("The instrument below plays what you play, from the iPad's speaker or whatever is plugged in. "
                     + "Foreground only: the sound pauses with the app and returns with it. Off, midi-sink is the "
                     + "controller alone and the sound is your synth's.")
                if !sound.status.isEmpty { Note(sound.status) }
            }
            Section("Instrument") {
                Button {
                    sound.instrument = "demo"
                } label: {
                    HStack { Text("Dan Tranh (the demo)"); Spacer(); if sound.instrument == "demo" { Image(systemName: "checkmark") } }
                }
                ForEach(sound.instruments, id: \.self) { rel in
                    Button {
                        sound.instrument = rel
                    } label: {
                        HStack { Text(rel); Spacer(); if sound.instrument == rel { Image(systemName: "checkmark") } }
                    }
                }
                .onDelete { idx in if let i = idx.first { confirmDelete = sound.instruments[i] } }
                Button {
                    sound.instrument = ""
                } label: {
                    HStack { Text("A sine per voice (no instrument)"); Spacer(); if sound.instrument.isEmpty { Image(systemName: "checkmark") } }
                }
                if sound.loading { ProgressView().progressViewStyle(.circular) }
                if !sound.report.isEmpty { Note(sound.report) }
                if !sound.memoryAdvice.isEmpty { Note(sound.memoryAdvice) }
            }
            Section("Files") {
                Button("Import an instrument…") { importing = true }
                Note("A Decent Sampler .dslibrary, or the folder that holds a .dspreset and its samples; copied into "
                     + "Files → On My iPad → midi-sink → Instruments, where you can also drop them yourself. "
                     + "A .dslibrary sent by AirDrop, Mail or \"Open in midi-sink\" lands here too. "
                     + "Nothing is bundled but the demo: libraries are yours and travel with their own terms.")
                Toggle("The play surface sounds here (Local Control)", isOn: $sound.localControl)
                Note("Off, your fingers and the pen still go out over MIDI but do not sound inside; external "
                     + "controllers always do.")
            }
        }
        .navigationTitle("Sound")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { sound.refreshInstruments() }
        .fileImporter(isPresented: $importing,
                      allowedContentTypes: [.folder, UTType("com.decentsamples.dslibrary") ?? .zip, UTType("com.decentsamples.dspreset") ?? .xml]) { result in
            switch result {
            case .success(let url):
                if let rel = sound.importInstrument(from: url) { sound.instrument = rel }
            case .failure(let e):
                NSLog("[voxo] import failed: %@", e.localizedDescription)
            }
        }
        .confirmationDialog("Remove '\(confirmDelete ?? "")'?", isPresented: Binding(get: { confirmDelete != nil }, set: { if !$0 { confirmDelete = nil } }),
                            titleVisibility: .visible) {
            Button("Remove", role: .destructive) { if let r = confirmDelete { sound.deleteInstrument(r) }; confirmDelete = nil }
        } message: {
            Note("The files are deleted from the app's Instruments folder.")
        }
    }
}

// -- the print ledger (QOL §4) -----------------------------------------------------

struct ShareSheet: UIViewControllerRepresentable {
    let url: URL
    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: [url], applicationActivities: nil)
    }
    func updateUIViewController(_ vc: UIActivityViewController, context: Context) {}
}

struct PrintsPage: View {
    @ObservedObject var session: SessionStore
    @ObservedObject var ledger: PrintLedger
    @AppStorage("exportSize") private var sizePick = 2
    @AppStorage("exportAnodAlpha") private var anodAlpha = false
    private static let sizes = ["Screen", "2K wide", "4K wide", "8K wide"]

    var body: some View {
        Form {
            Section {
                Picker("Export size", selection: $sizePick) {
                    ForEach(0..<4, id: \.self) { Text(Self.sizes[$0]).tag($0) }
                }
                .pickerStyle(.segmented)
                Toggle("Anod over alpha", isOn: $anodAlpha)
                Note("Every dip lands here with the sheet it printed. The field is resolution-independent, so a "
                     + "dip re-renders at any size from the same field — up to 8192 a side; detail below a field "
                     + "texel is interpolation (the true re-dip is replay). Anod over alpha writes the glow and the "
                     + "grid over transparent glass. Exports go to Files → midi-sink → Prints and the share sheet.")
                Button("Save the newest print to Photos") { ledger.saveNewestToPhotos() }
                    .disabled(!ledger.entries.contains { $0.printSeen })
            }
            Section("This session") {
                if ledger.entries.isEmpty { Note("No dips yet this session.") }
                ForEach(ledger.entries.reversed()) { e in
                    HStack(alignment: .top, spacing: 12) {
                        if let t = e.thumb {
                            Image(uiImage: t).resizable().aspectRatio(contentMode: .fit).frame(width: 160)
                                .clipShape(RoundedRectangle(cornerRadius: 4))
                        } else {
                            RoundedRectangle(cornerRadius: 4).fill(.quaternary).frame(width: 160, height: 100)
                        }
                        VStack(alignment: .leading, spacing: 6) {
                            Text("\(e.when.formatted(date: .omitted, time: .standard)) · \(e.anod ? "Anod" : "Sumi")")
                            let (w, h) = PrintLedger.size(for: sizePick, fw: e.fw, fh: e.fh)
                            Text("field \(e.fw)×\(e.fh) → \(w)×\(h)\(anodAlpha && e.anod ? " over alpha" : "")")
                                .font(.footnote).foregroundStyle(.secondary).monospacedDigit()
                            Button("Export PNG") {
                                SumiCanvasView.shared?.exportPrint(id: e.id, choice: sizePick, anodAlpha: anodAlpha)
                            }
                            .buttonStyle(.bordered)
                            .disabled(ledger.busy)
                        }
                    }
                }
            }
            if !ledger.status.isEmpty { Section { Note(ledger.status) } }
        }
        .navigationTitle("Prints")
        .navigationBarTitleDisplayMode(.inline)
        .sheet(isPresented: Binding(get: { ledger.shareURL != nil }, set: { if !$0 { ledger.shareURL = nil } })) {
            if let url = ledger.shareURL { ShareSheet(url: url) }
        }
    }
}

// -- the Phase-6 operators (MEDIUM §2) ---------------------------------------------

struct OperatorsPage: View {
    @ObservedObject var session: SessionStore

    var body: some View {
        Form {
            Section("Chladni") {
                ControlRow(session: session, label: "Stir (A)", ctl: 16)
                ControlRow(session: session, label: "Balance (B)", ctl: 17)
                Note("Every key's disc turns as an eddy (still at the core and the rim) while the water between the "
                     + "keys rests. In Anod the per-note bend plays the stir. Balance: 0 neighbours counter-rotate, "
                     + "½ every other cell rests, 1 all turn the same way.")
                Picker("Mode", selection: $session.params.chladni_mode) {
                    Text("Discs (exact)").tag(UInt32(0))
                    Text("Inverse Chladni").tag(UInt32(1))
                }
                .pickerStyle(.segmented)
                FloatRow(label: "Cell size", value: $session.params.chladni_cell, range: 0.5...1.5, fmt: "%.2f")
                Note("Discs: exact rotation inside each key, capped at the key. Inverse Chladni: the eddies are "
                     + "summed into one flow, may grow past the keys (to 1.5) and stir the water between them.")
            }
            Section("Burst") {
                FloatRow(label: "Age", value: $session.params.burst_age, range: 1.5...12, fmt: "%.1f× core")
                FloatRow(label: "Life", value: $session.params.burst_life, range: 0...4, fmt: "%.2f s")
                Stepper("Order: m = \(session.params.burst_order)",
                        value: Binding(get: { Int(session.params.burst_order) }, set: { session.params.burst_order = UInt32($0) }), in: 2...8)
                Note("The viscous multipole burst (a gesture; the Anod strike no longer fires it).")
            }
            Section("Spark") {
                FloatRow(label: "Shear", value: $session.params.spark_shear, range: 0...2, fmt: "%.2f× r")
                FloatRow(label: "Decay", value: $session.params.spark_tau, range: 0.05...2, fmt: "%.2f s")
                Stepper("Octaves: \(session.params.spark_stack)",
                        value: Binding(get: { Int(session.params.spark_stack) }, set: { session.params.spark_stack = UInt32($0) }), in: 1...4)
                Picker("Profile", selection: $session.params.spark_profile) {
                    Text("Triangle").tag(UInt32(0))
                    Text("Noise").tag(UInt32(1))
                }
                .pickerStyle(.segmented)
                ControlRow(session: session, label: "Frequency (k)", ctl: 18)
                Note("The Anod strike's jagged shear episode. The slide (CC 74) drives the frequency under the Anod default.")
            }
            Section("Chirikov") {
                FloatRow(label: "K max", value: $session.params.chirikov_kmax, range: 0...2, fmt: "%.2f")
                Stepper("Periods: \(session.params.chirikov_periods)",
                        value: Binding(get: { Int(session.params.chirikov_periods) }, set: { session.params.chirikov_periods = UInt32($0) }), in: 1...8)
                FloatRow(label: "Drift", value: $session.params.chirikov_eps, range: 0.05...1, fmt: "%.2f")
                ControlRow(session: session, label: "Throw", ctl: 19)
                Note("The standard map: a throw of the mod wheel (the Anod default) or this slider is a kick; Greene's "
                     + "threshold 0.97 separates smooth sheets from filaments and island chains.")
            }
            if [16, 17, 18, 19].contains(where: { session.route(for: $0) == nil }) {
                Section { Note("A slider without a CC route is disabled — route one in the CC map (104–109 by default).") }
            }
        }
        .navigationTitle("Operators")
        .navigationBarTitleDisplayMode(.inline)
    }
}

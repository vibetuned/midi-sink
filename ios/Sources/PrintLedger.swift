// Phase 6 step 44a (QOL §4): THE PRINT LEDGER on the iPad — the desktop's
// (desktop/src/print_ledger.cpp) in Swift. Every paper dip keeps the FIELD it
// printed (sumi_read_field, RGBA16F), the look (params + palette) and, when
// the dip's print lands, a thumbnail; any entry re-exports at Screen / 2K /
// 4K / 8K (Anod optionally over alpha) through sumi_export_begin/poll — the
// same field composited at the target size — to Documents/Prints/<name>.png,
// then the share sheet (Save Image → Photos, Files, AirDrop). The canvas
// ticks it once per frame on the main thread (the render thread on iOS).
import Foundation
import UIKit
import ImageIO
import UniformTypeIdentifiers
import SumiCore

final class PrintLedger: ObservableObject {
    struct Entry: Identifiable {
        let id = UUID()
        var field: [UInt8]
        var fw: UInt32, fh: UInt32
        var params: sumi_params_t
        var palette: sumi_palette_t
        var when: Date
        var print: [UInt8] = []          // RGBA8, the newest entry only
        var pw: UInt32 = 0, ph: UInt32 = 0
        var thumb: UIImage?
        var printSeen = false
        var anod: Bool { params.medium == SUMI_MEDIUM_ANOD.rawValue }
    }

    // An iPad field at sim_scale 1 is ~31 MB (2360 × 1640 × 8): six entries or 256 MB, the oldest first.
    static let maxEntries = 6
    static let maxBytes = 256 << 20

    @Published private(set) var entries: [Entry] = []   // newest last
    @Published private(set) var busy = false
    @Published var status = ""
    /// Set when an export's PNG is written: the Prints page presents the share sheet for it.
    @Published var shareURL: URL?

    private var discardPrints = 0         // prints of "clear" dips still to arrive — read and dropped
    private var exportSize: (UInt32, UInt32) = (0, 0)
    private var exportName = ""

    static var printsDir: URL {
        let d = SessionStore.documentsDir.appendingPathComponent("Prints", isDirectory: true)
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d
    }

    private var bytes: Int { entries.reduce(0) { $0 + $1.field.count + $1.print.count } }
    private func evict() {
        while !entries.isEmpty && (entries.count > Self.maxEntries || bytes > Self.maxBytes) { entries.removeFirst() }
    }

    /// The product's dip: keep the field as it stands and the look, then dip.
    func dip(_ inst: OpaquePointer) {
        var w: UInt32 = 0, h: UInt32 = 0
        guard sumi_read_field(inst, nil, 0, &w, &h), w > 0, h > 0 else {
            sumi_trigger_paper_dip(inst)
            status = "Dipped (a readback was in flight: this sheet is not in the ledger)"
            return
        }
        var field = [UInt8](repeating: 0, count: Int(w) * Int(h) * 8)
        let ok = field.withUnsafeMutableBufferPointer { sumi_read_field(inst, $0.baseAddress, $0.count, &w, &h) }
        guard ok else { sumi_trigger_paper_dip(inst); return }
        var p = sumi_params_t(); sumi_get_params(inst, &p)
        var pal = sumi_palette_t(); sumi_get_palette(inst, &pal)
        for i in entries.indices { entries[i].print = [] }   // only the newest keeps its full print
        entries.append(Entry(field: field, fw: w, fh: h, params: p, palette: pal, when: Date()))
        evict()
        sumi_trigger_paper_dip(inst)
        status = "Dipped: the sheet is kept in the ledger"
    }

    /// Fresh paper, nothing kept: the core still prints, and that print is read and dropped on arrival.
    func clear(_ inst: OpaquePointer) {
        sumi_trigger_paper_dip(inst)
        discardPrints += 1
        status = "Cleared: fresh paper, nothing kept"
    }

    func tick(_ inst: OpaquePointer) {
        var pw: UInt32 = 0, ph: UInt32 = 0
        if (discardPrints > 0 || (entries.last.map { !$0.printSeen } ?? false)),
           sumi_read_print(inst, nil, 0, &pw, &ph), pw > 0, ph > 0 {
            var px = [UInt8](repeating: 0, count: Int(pw) * Int(ph) * 4)
            let ok = px.withUnsafeMutableBufferPointer { sumi_read_print(inst, $0.baseAddress, $0.count, &pw, &ph) }
            if discardPrints > 0 {
                discardPrints -= 1               // a clear's print, in order
            } else if ok, let k = entries.indices.last {
                entries[k].print = px
                entries[k].pw = pw; entries[k].ph = ph
                entries[k].printSeen = true
                entries[k].thumb = Self.thumbnail(px, pw, ph)
                evict()
            }
        }
        guard busy else { return }
        var w: UInt32 = 0, h: UInt32 = 0
        let st0 = sumi_export_poll(inst, nil, 0, &w, &h)
        if st0 == 0 { busy = false; status = "Export failed"; return }
        if st0 != 2 { return }
        var px = [UInt8](repeating: 0, count: Int(exportSize.0) * Int(exportSize.1) * 4)
        let st = px.withUnsafeMutableBufferPointer { sumi_export_poll(inst, $0.baseAddress, $0.count, &w, &h) }
        busy = false
        guard st == 2 else { status = "Export failed"; return }
        let url = Self.printsDir.appendingPathComponent(exportName)
        status = "Writing \(w)×\(h)…"
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            let ok = Self.writePNG(px, w, h, alpha: true, to: url)
            DispatchQueue.main.async {
                self?.status = ok ? "Exported \(w)×\(h) → Prints/\(url.lastPathComponent)" : "Could not write the PNG"
                if ok { self?.shareURL = url }
            }
        }
    }

    /// The export size for a choice: 0 the field's own, else 2K / 4K / 8K wide, the height by aspect (capped).
    static func size(for choice: Int, fw: UInt32, fh: UInt32) -> (UInt32, UInt32) {
        guard choice > 0, fw > 0 else { return (fw, fh) }
        var w: UInt32 = choice == 1 ? 2048 : choice == 2 ? 4096 : 8192
        var h = UInt32((Double(w) * Double(fh) / Double(fw)).rounded())
        if h > SUMI_EXPORT_MAX_DIM { h = SUMI_EXPORT_MAX_DIM; w = UInt32((Double(h) * Double(fw) / Double(fh)).rounded()) }
        return (w, max(h, 1))
    }

    /// Re-export entry `index`: the entry's look round the render (begin renders), the current look after.
    func export(_ inst: OpaquePointer, id: UUID, choice: Int, anodAlpha: Bool,
                current: sumi_params_t, currentPalette: sumi_palette_t) {
        guard !busy, let e = entries.first(where: { $0.id == id }) else { return }
        let (w, h) = Self.size(for: choice, fw: e.fw, fh: e.fh)
        var ep = e.params, epal = e.palette, cp = current, cpal = currentPalette
        sumi_set_params(inst, &ep)
        sumi_set_palette(inst, &epal)
        let flags: UInt32 = anodAlpha && e.anod ? SUMI_EXPORT_ANOD_ALPHA : 0
        let ok = e.field.withUnsafeBufferPointer { sumi_export_begin(inst, $0.baseAddress, e.fw, e.fh, w, h, flags) }
        sumi_set_params(inst, &cp)
        sumi_set_palette(inst, &cpal)
        guard ok else { status = "Export could not start (a readback is in flight, or the size is out of range)"; return }
        let f = DateFormatter(); f.dateFormat = "yyyyMMdd-HHmmss"
        exportName = "print-\(f.string(from: e.when))-\(w)x\(h)\(flags != 0 ? "-alpha" : "").png"
        exportSize = (w, h)
        busy = true
        status = "Exporting \(w)×\(h)…"
    }

    /// The newest dip's own print, straight to Photos (the one-tap save the 1.0 sheet had).
    func saveNewestToPhotos() {
        guard let e = entries.last(where: { $0.printSeen && !$0.print.isEmpty }),
              let img = Self.image(e.print, e.pw, e.ph, alpha: false) else { status = "No print yet — dip first"; return }
        UIImageWriteToSavedPhotosAlbum(UIImage(cgImage: img), nil, nil, nil)
        status = "Saved the print (\(e.pw)×\(e.ph)) to Photos"
    }

    // -- images --------------------------------------------------------------

    static func image(_ px: [UInt8], _ w: UInt32, _ h: UInt32, alpha: Bool) -> CGImage? {
        guard let provider = CGDataProvider(data: Data(px) as CFData) else { return nil }
        let info: CGImageAlphaInfo = alpha ? .last : .noneSkipLast
        return CGImage(width: Int(w), height: Int(h), bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: Int(w) * 4,
                       space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGBitmapInfo(rawValue: info.rawValue),
                       provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent)
    }

    static func writePNG(_ px: [UInt8], _ w: UInt32, _ h: UInt32, alpha: Bool, to url: URL) -> Bool {
        guard let img = image(px, w, h, alpha: alpha),
              let dest = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil) else { return false }
        CGImageDestinationAddImage(dest, img, nil)
        return CGImageDestinationFinalize(dest)
    }

    /// Box-averaged, at most 320 wide / 200 tall.
    static func thumbnail(_ px: [UInt8], _ pw: UInt32, _ ph: UInt32) -> UIImage? {
        let sc = max(1, max((Int(pw) + 319) / 320, (Int(ph) + 199) / 200))
        let tw = Int(pw) / sc, th = Int(ph) / sc
        guard tw > 0, th > 0 else { return nil }
        var t = [UInt8](repeating: 0, count: tw * th * 4)
        px.withUnsafeBufferPointer { src in
            for y in 0..<th {
                for x in 0..<tw {
                    var acc = (0, 0, 0)
                    for yy in 0..<sc {
                        var o = ((y * sc + yy) * Int(pw) + x * sc) * 4
                        for _ in 0..<sc { acc.0 += Int(src[o]); acc.1 += Int(src[o + 1]); acc.2 += Int(src[o + 2]); o += 4 }
                    }
                    let q = (y * tw + x) * 4, n = sc * sc
                    t[q] = UInt8(acc.0 / n); t[q + 1] = UInt8(acc.1 / n); t[q + 2] = UInt8(acc.2 / n); t[q + 3] = 255
                }
            }
        }
        return image(t, UInt32(tw), UInt32(th), alpha: false).map { UIImage(cgImage: $0) }
    }
}

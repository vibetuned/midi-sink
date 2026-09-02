// Play-mode overlay (PHASE4_SPEC.md §6): faint cell lattice + per-touch
// joystick indicators. Step 15 scope — NO MIDI: touches drive only the
// indicator math (soft-knee deadband via HostMPE). Rendered natively over the
// Metal canvas; never inside the core.
//
// Geometry has ONE source of truth: everything drawn here comes from
// sumi_layout_probe — the lattice is built by sweeping the probe over the
// canvas, so no lattice math is ever duplicated in Swift.
import UIKit
import SumiCore
import HostMPE

final class PlayOverlayView: UIView {
    // The canvas view owns the params snapshot (it owns every params write).
    var paramsProvider: (() -> sumi_params_t)?

    private struct Cell {
        let note: UInt8
        let center: CGPoint     // normalized canvas coords
        let radius: Float       // canvas-height units (probe cell_radius)
    }
    private struct ActiveTouch {
        let origin: CGPoint     // view points
        let rMaxCH: Float       // canvas-height units
        let note: UInt8
        var effX: Float = 0     // Δ_eff (direction · g, magnitude ≤ 1)
        var effY: Float = 0
    }

    private var cells: [Cell] = []
    private var latticeLayout: UInt32 = .max
    private var latticeSize = CGSize.zero
    private let latticeLayer = CAShapeLayer()
    private var touches: [ObjectIdentifier: ActiveTouch] = [:]

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        backgroundColor = .clear
        isOpaque = false
        latticeLayer.fillColor = nil
        latticeLayer.strokeColor = UIColor.black.withAlphaComponent(0.10).cgColor
        latticeLayer.lineWidth = 1.0
        layer.addSublayer(latticeLayer)
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    override func layoutSubviews() {
        super.layoutSubviews()
        latticeLayer.frame = bounds
        rebuildLatticeIfNeeded(force: false)
    }

    func layoutParamsChanged() {
        rebuildLatticeIfNeeded(force: true)
        touches.removeAll()
        setNeedsDisplay()
    }

    // -- lattice (probe sweep — no geometry duplication) ----------------------

    private func rebuildLatticeIfNeeded(force: Bool) {
        guard bounds.width > 0, bounds.height > 0,
              let params = paramsProvider?() else { return }
        if !force && params.pitch_layout == latticeLayout && bounds.size == latticeSize {
            return
        }
        latticeLayout = params.pitch_layout
        latticeSize = bounds.size
        cells.removeAll()

        var p = params
        let aspect = Float(bounds.width / bounds.height)
        var info = sumi_cell_info_t()
        // Sweep finely enough that no cell is skipped (Jankó columns are the
        // narrowest feature); dedupe by (note, quantized center).
        var seen = Set<String>()
        let nx = 220, ny = 120
        for iy in 0..<ny {
            for ix in 0..<nx {
                let x = Float(ix) / Float(nx - 1)
                let y = Float(iy) / Float(ny - 1)
                if sumi_layout_probe(p.pitch_layout, &p, aspect, x, y, &info) {
                    let key = "\(info.note):\(Int(info.cell_center_x * 4096)):\(Int(info.cell_center_y * 4096))"
                    if seen.insert(key).inserted {
                        cells.append(Cell(note: info.note,
                                          center: CGPoint(x: CGFloat(info.cell_center_x),
                                                          y: CGFloat(info.cell_center_y)),
                                          radius: info.cell_radius))
                    }
                }
            }
        }
        let path = CGMutablePath()
        for c in cells {
            let r = CGFloat(c.radius) * bounds.height
            path.addEllipse(in: CGRect(x: c.center.x * bounds.width - r,
                                       y: c.center.y * bounds.height - r,
                                       width: 2 * r, height: 2 * r))
        }
        latticeLayer.path = path
    }

    // -- touches: indicator math only (Step 15 — no MIDI) ---------------------

    override func touchesBegan(_ ts: Set<UITouch>, with event: UIEvent?) {
        guard bounds.height > 0, var p = paramsProvider?() else { return }
        let aspect = Float(bounds.width / bounds.height)
        var info = sumi_cell_info_t()
        for t in ts {
            let loc = t.location(in: self)
            let ok = sumi_layout_probe(p.pitch_layout, &p, aspect,
                                       Float(loc.x / bounds.width),
                                       Float(loc.y / bounds.height), &info)
            if ok {
                touches[ObjectIdentifier(t)] = ActiveTouch(origin: loc,
                                                           rMaxCH: info.cell_radius,
                                                           note: info.note)
            }
            // Probe false (dead zone / unplayable layout): the touch is
            // honestly ignored — off the key bed.
        }
        setNeedsDisplay()
    }

    override func touchesMoved(_ ts: Set<UITouch>, with event: UIEvent?) {
        guard bounds.height > 0 else { return }
        for t in ts {
            guard var at = touches[ObjectIdentifier(t)] else { continue }
            let loc = t.location(in: self)
            // Δ in canvas-height units — the probe's metric (§2 units).
            let dx = Float((loc.x - at.origin.x) / bounds.height)
            let dy = Float((loc.y - at.origin.y) / bounds.height)
            var ex: Float = 0, ey: Float = 0
            hostmpe_joystick_eff(dx, dy, at.rMaxCH, &ex, &ey)
            at.effX = ex
            at.effY = ey
            touches[ObjectIdentifier(t)] = at
        }
        setNeedsDisplay()
    }

    override func touchesEnded(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts { touches.removeValue(forKey: ObjectIdentifier(t)) }
        setNeedsDisplay()
    }
    override func touchesCancelled(_ ts: Set<UITouch>, with event: UIEvent?) {
        touchesEnded(ts, with: event)
    }

    // -- indicator drawing -----------------------------------------------------

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext(), !touches.isEmpty else { return }
        let h = bounds.height
        // Jankó echo highlight (§6): all rows of a touched note are the same
        // note — say so.
        let heldNotes = Set(touches.values.map { $0.note })
        if !heldNotes.isEmpty {
            ctx.setFillColor(UIColor.black.withAlphaComponent(0.08).cgColor)
            for c in cells where heldNotes.contains(c.note) {
                let r = CGFloat(c.radius) * h
                ctx.fillEllipse(in: CGRect(x: c.center.x * bounds.width - r,
                                           y: c.center.y * h - r,
                                           width: 2 * r, height: 2 * r))
            }
        }
        for at in touches.values {
            let rPx = CGFloat(at.rMaxCH) * h
            // Hairline circle at the touch origin, radius R_max: the joystick's
            // travel bound, exactly as the math computes it.
            ctx.setStrokeColor(UIColor.black.withAlphaComponent(0.35).cgColor)
            ctx.setLineWidth(1.0)
            ctx.strokeEllipse(in: CGRect(x: at.origin.x - rPx, y: at.origin.y - rPx,
                                         width: 2 * rPx, height: 2 * rPx))
            // Thumb dot at Δ_eff — sits AT the origin inside the deadband.
            let tx = at.origin.x + CGFloat(at.effX) * rPx
            let ty = at.origin.y + CGFloat(at.effY) * rPx
            ctx.setFillColor(UIColor.black.withAlphaComponent(0.55).cgColor)
            ctx.fillEllipse(in: CGRect(x: tx - 5, y: ty - 5, width: 10, height: 10))
        }
    }
}

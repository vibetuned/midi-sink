// Play-mode overlay (PROJECT_SPEC.md §8.6): faint cell lattice + per-touch
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

final class PlayOverlayView: UIView, UIPencilInteractionDelegate {
    // The canvas view owns the params snapshot (it owns every params write),
    // hosts the serial MIDI queue, and is the touch path's MIDI endpoint.
    var paramsProvider: (() -> sumi_params_t)?
    weak var host: SumiCanvasView?

    private struct Cell {
        let note: UInt8
        let center: CGPoint     // normalized canvas coords
        let radius: Float       // canvas-height units (probe cell_radius)
    }
    private struct ActiveTouch {
        let origin: CGPoint     // view points
        let rMaxCH: Float       // canvas-height units
        let note: UInt8
        let voice: Int32        // hostmpe member channel, -1 = saturated
        var effX: Float = 0     // Δ_eff (direction · g, magnitude ≤ 1) — indicator
        var effY: Float = 0
    }
    // §7 (step 21): the pen abandons the joystick — absolute-position play.
    private struct ActivePen {
        let voice: Int32
        let anchor: CGPoint     // strike point, view coords
        let axisX: Float        // anchor cell's semitone axis (ac unit vector)
        let axisY: Float
        let step: Float         // canvas-height units per semitone
        let rMaxCH: Float       // for the CC74 knee
        var lastNote: UInt8     // piano-grid cell tracking
        var lastPoint: CGPoint  // wake segment tail
        var lastEff: Float = 0  // CC74/pinch delta state
        // #40 (corrected): the barrel controls are GESTURES, not knobs —
        // deltas feed a value that DECAYS (τ = 0.4 s), so the effect lives
        // while the hand moves and dies when it stops; any tilt leak
        // self-heals to zero.
        var lastAzimuth: Float  // rad
        var bendBoost: Float = 0         // vibrato booster: bend ×(1 + boost), 0..2
        var lastRoll: Float = 0          // Pencil Pro rollAngle (#40: the booster's
                                         // primary feed — reborn from the vortex days)
        var lastAltitude: Float = 0      // posture gate
        var lastOffset: Float = 0        // for decay re-emission of the bend
        var lastVel: UInt8 = 96
    }
    // How long a delta lives as a multiplier: exponential, τ = 0.4 s.
    private let GESTURE_TAU: Double = 0.4
    private var pens: [ObjectIdentifier: ActivePen] = [:]
    private var hoverPoint: CGPoint? = nil
    // #61 pen sustain (squeeze / double-tap): held state. Only TRANSITIONS
    // reach the engine, so a squeeze's began/changed stream cannot double-fire
    // one press (Android's `setPenButton` contract, verbatim).
    private var penSustainDown = false
    private var saturationBlinkUntil: CFTimeInterval = 0

    private var cells: [Cell] = []
    private var latticeLayout: UInt32 = .max
    private var latticeSize = CGSize.zero
    // #41 two-tone lattice: a paper-cream halo UNDER each dark ring keeps the
    // cells legible over dense ink; accidental rings draw LAST (on top), like
    // black keys sitting on the keybed.
    private let latticeLightN = CAShapeLayer()   // naturals, light halo
    private let latticeDarkN = CAShapeLayer()    // naturals, dark ring
    private let latticeLightA = CAShapeLayer()   // accidentals, light halo
    private let latticeDarkA = CAShapeLayer()    // accidentals, dark ring
    private var touches: [ObjectIdentifier: ActiveTouch] = [:]

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        backgroundColor = .clear
        isOpaque = false
        // §7 hover ghost (Pencil hover, M2 iPads; inert elsewhere).
        let hover = UIHoverGestureRecognizer(target: self, action: #selector(onHover))
        addGestureRecognizer(hover)
        // #61: the Pencil Pro's SQUEEZE is the S-Pen barrel button's twin
        // (Android #58) — it drives the SAME strip sustain engine, so one
        // pedal serves pen and panel on both platforms.
        let pencil = UIPencilInteraction()
        pencil.delegate = self
        addInteraction(pencil)
        let paper = UIColor(red: 0.96, green: 0.94, blue: 0.89, alpha: 0.55)
        for (l, color, width) in [(latticeLightN, paper, 3.0),
                                  (latticeDarkN, UIColor.black.withAlphaComponent(0.18), 1.0),
                                  (latticeLightA, paper, 3.0),
                                  (latticeDarkA, UIColor.black.withAlphaComponent(0.30), 1.2)]
        as [(CAShapeLayer, UIColor, CGFloat)] {
            l.fillColor = nil
            l.strokeColor = color.cgColor
            l.lineWidth = width
            layer.addSublayer(l)   // add order = z order: accidentals on top
        }
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    override func layoutSubviews() {
        super.layoutSubviews()
        for l in [latticeLightN, latticeDarkN, latticeLightA, latticeDarkA] {
            l.frame = bounds
        }
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
        let naturals = CGMutablePath()
        let accidentals = CGMutablePath()
        for c in cells {
            let r = CGFloat(c.radius) * bounds.height
            let rect = CGRect(x: c.center.x * bounds.width - r,
                              y: c.center.y * bounds.height - r,
                              width: 2 * r, height: 2 * r)
            let pc = Int(c.note) % 12
            let black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10
            (black ? accidentals : naturals).addEllipse(in: rect)
        }
        latticeLightN.path = naturals
        latticeDarkN.path = naturals
        latticeLightA.path = accidentals
        latticeDarkA.path = accidentals
    }

    // -- touches: indicator math only (Step 15 — no MIDI) ---------------------

    // §4 truth table, iOS finger: no force API — synthesized velocity
    // (default 96, majorRadius modulation behind a setting). Continuous
    // pressure comes from the UPWARD joystick axis (§3.3 rev, DECISIONS_3
    // #19), computed inside hostmpe from the raw delta — nothing here.
    private func synthVelocity(_ t: UITouch) -> UInt8 {
        guard host?.velocityFromTouchSize == true else { return 96 }
        let mr = Float(t.majorRadius)
        let v = 48.0 + (mr - 8.0) / 18.0 * 70.0
        return UInt8(min(max(v, 40.0), 120.0))
    }

    override func touchesBegan(_ ts: Set<UITouch>, with event: UIEvent?) {
        guard bounds.height > 0, var p = paramsProvider?() else { return }
        let aspect = Float(bounds.width / bounds.height)
        var info = sumi_cell_info_t()
        for t in ts {
            let loc = t.location(in: self)
            let ok = sumi_layout_probe(p.pitch_layout, &p, aspect,
                                       Float(loc.x / bounds.width),
                                       Float(loc.y / bounds.height), &info)
            guard ok else { continue }   // dead zone: off the key bed
            if t.type == .pencil {
                // §7: absolute-position play — the strike anchors the note.
                // Velocity from REAL tip force in UIKit's native units
                // (1.0 = an average finger touch): a baseline tap plays at
                // the finger default (96), force 3 is maximally loud (127).
                // Touch-down force reads low before contact force builds
                // (#38 corrected), so sub-baseline forces clamp UP to the
                // baseline — the pen never whispers by accident; pressing in
                // only adds.
                let raw = Float(t.force)
                let f = max(raw, 1.0)
                let vel = UInt8(min(96.0 + (f - 1.0) * 15.5, 127.0))
                let voice = host?.penBegin(note: info.note, velocity: vel) ?? -1
                if voice < 0 {
                    saturationBlinkUntil = CACurrentMediaTime() + 0.35
                    setNeedsDisplay()
                    continue
                }
                var newPen = ActivePen(
                    voice: voice, anchor: loc,
                    axisX: info.semitone_dx, axisY: info.semitone_dy,
                    step: info.semitone_step, rMaxCH: info.cell_radius,
                    lastNote: info.note, lastPoint: loc,
                    lastAzimuth: Float(t.azimuthAngle(in: self)))
                newPen.lastAltitude = Float(t.altitudeAngle)
                if #available(iOS 17.5, *) { newPen.lastRoll = Float(t.rollAngle) }
                pens[ObjectIdentifier(t)] = newPen
                continue
            }
            // hostmpe: center bend + Note On on the allocated channel, into
            // the loopback via the serial producer queue. The pitch gradient
            // is the probe's #7 axis (DECISIONS_3 #18: horizontal on both
            // playable layouts; vertical is timbre's alone).
            let voice = host?.playTouchBegin(note: info.note,
                                             velocity: synthVelocity(t),
                                             rMax: info.cell_radius,
                                             gradX: info.semitone_dx / info.semitone_step,
                                             gradY: info.semitone_dy / info.semitone_step) ?? -1
            if voice < 0 {
                // Saturation (§5.1): silent drop + HUD blink — never steal.
                saturationBlinkUntil = CACurrentMediaTime() + 0.35
                setNeedsDisplay()
                continue
            }
            touches[ObjectIdentifier(t)] = ActiveTouch(origin: loc,
                                                       rMaxCH: info.cell_radius,
                                                       note: info.note,
                                                       voice: voice)
        }
        setNeedsDisplay()
    }

    override func touchesMoved(_ ts: Set<UITouch>, with event: UIEvent?) {
        guard bounds.height > 0 else { return }
        for t in ts {
            if t.type == .pencil, var pen = pens[ObjectIdentifier(t)] {
                movePen(t, &pen)
                pens[ObjectIdentifier(t)] = pen
                continue
            }
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
            // hostmpe takes the RAW screen delta; deadband/knee/pressure-from-
            // upward-Y are its job (one implementation, zero drift).
            host?.playTouchUpdate(voice: at.voice, dx: dx, dy: dy)
        }
        setNeedsDisplay()
    }

    override func touchesEnded(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            if let pen = pens.removeValue(forKey: ObjectIdentifier(t)) {
                host?.playTouchEnd(voice: pen.voice, lift: 64, isPen: true)
                continue
            }
            if let at = touches.removeValue(forKey: ObjectIdentifier(t)) {
                host?.playTouchEnd(voice: at.voice, lift: 64)
            }
        }
        setNeedsDisplay()
    }

    /// §7 pen move: wake on every stroke segment (both would-be modes — it is
    /// physical, never MIDI), then per-layout legato, CC74/pinch, pressure.
    private func movePen(_ t: UITouch, _ pen: inout ActivePen) {
        var p = paramsProvider?() ?? sumi_params_t()
        let loc = t.location(in: self)
        let h = bounds.height
        // Tip travel this event — the gesture discriminator (#40 corrected):
        // azimuth swings NATURALLY while drawing, so lean deltas count as a
        // gesture only while the tip is planted.
        let tipMove = hypot(loc.x - pen.lastPoint.x, loc.y - pen.lastPoint.y)
        // Dipolar wake rides the stroke: tip radius from real force (§4.3(4)).
        let force = t.maximumPossibleForce > 0
            ? Float(t.force / t.maximumPossibleForce) : 0.3
        host?.addWake(x0: Float(pen.lastPoint.x / bounds.width),
                      y0: Float(pen.lastPoint.y / h),
                      x1: Float(loc.x / bounds.width),
                      y1: Float(loc.y / h),
                      tip: 0.006 + 0.030 * force)
        pen.lastPoint = loc

        let aspect = Float(bounds.width / h)
        // Legato glissando (#39, all playable layouts): probe the cell UNDER
        // the pen; crossing into a new cell is a same-channel legato
        // retrigger at the CURRENT force's velocity; inside the cell the
        // offset from its center (along its own semitone axis) is the bend —
        // vibrato without retriggering. Dead zones (piano-grid gaps, off the
        // lattice): no call — the last pitch sustains.
        var info = sumi_cell_info_t()
        if sumi_layout_probe(p.pitch_layout, &p, aspect,
                             Float(loc.x / bounds.width),
                             Float(loc.y / h), &info) {
            let dxAC = Float(loc.x / h) - info.cell_center_x * aspect
            let dyAC = Float(loc.y / h) - info.cell_center_y
            let offset = (dxAC * info.semitone_dx + dyAC * info.semitone_dy)
                         / info.semitone_step
            let vel = UInt8(min(96.0 + (max(Float(t.force), 1.0) - 1.0) * 15.5, 127.0))
            pen.lastNote = info.note
            pen.lastOffset = offset
            pen.lastVel = vel
            host?.penGlide(voice: pen.voice, note: info.note, offset: offset,
                           scale: 1.0 + min(pen.bendBoost, 2.0), velocity: vel)
        }

        // #40 posture gate (corrected): tilting cross-talks into the reported
        // roll and azimuth — a posture change is NOT a dial gesture. While
        // altitude is moving, both dials freeze; azimuth is additionally
        // ignored near vertical, where UIKit's estimate swings wildly.
        let alt = Float(t.altitudeAngle)
        let posture = abs(alt - pen.lastAltitude) > 0.02
        pen.lastAltitude = alt

        // #40 azimuth GESTURE (final): the in-cell TIP vibrato is always
        // live at ×1 — the baseline is ONE, never zero. Stirring the tail
        // with the tip planted (< 3 pt travel this event; a moving tip is
        // handwriting, never a gesture) multiplies it momentarily, ×1..×3,
        // decaying back to ×1 (τ = 0.4 s). Per-event floor 0.02 rad keeps
        // lean jitter silent.
        let az = Float(t.azimuthAngle(in: self))
        let dAz = atan2f(sinf(az - pen.lastAzimuth), cosf(az - pen.lastAzimuth))
        pen.lastAzimuth = az
        if !posture && alt < 1.2 && tipMove < 3.0 && abs(dAz) > 0.02 {
            let step = min(abs(dAz), 0.2)
            pen.bendBoost = min(pen.bendBoost + step * 2.0, 2.0)
        }

        // #40 (final form): the UNBOUND BARREL ROLL is the booster's primary
        // feed — the same hair-trigger responsiveness that made it a leaky
        // vortex trigger makes it a great vibrato throttle, and unlike
        // azimuth it has no stroke cross-talk, so it works WHILE the tip
        // wiggles (exactly when depth is wanted). A quick twist saturates
        // the boost (×3); leaks only feed a decaying value that multiplies
        // an existing bend — self-healing, low stakes.
        if #available(iOS 17.5, *) {
            let roll = Float(t.rollAngle)
            let dRoll = atan2f(sinf(roll - pen.lastRoll), cosf(roll - pen.lastRoll))
            pen.lastRoll = roll
            if !posture && abs(dRoll) > 0.008 {
                let step = min(abs(dRoll), 0.4)
                pen.bendBoost = min(pen.bendBoost + step * 4.0, 2.0)
            }
        }

        // (#40 final: the barrel-roll -> mod/vortex gesture is REMOVED — tilt
        // cross-talk kept leaking a phantom vortex through every gate; the
        // mod belongs to the strip's Mod wheel. The pen keeps: legato,
        // bend + azimuth boost, CC74, pressure, wake, pinch.)

        // Y (relative to the strike) is the stylus timbre axis (§3.3):
        // knee-shaped, center 64, up = brighter. With slide_mode = 1 the
        // smoothed deltas drive the PINCH at the pen position, fold axis from
        // azimuth (§7) — CC74 then goes OUTBOUND ONLY (DECISIONS_3 #38).
        var ex: Float = 0, ey: Float = 0
        hostmpe_joystick_eff(0, dyCH_forSlide(loc, pen), pen.rMaxCH, &ex, &ey)
        let eff = -ey
        if p.slide_mode == 1 {
            let dk = (eff - pen.lastEff) * 0.6
            if abs(dk) > 0.002 {
                let az = Float(t.azimuthAngle(in: self))
                host?.penPinch(x: Float(loc.x / bounds.width),
                               y: Float(loc.y / h), k: dk, angle: az)
            }
            host?.penSlide(voice: pen.voice, eff: eff, outboundOnly: true)
        } else {
            host?.penSlide(voice: pen.voice, eff: eff, outboundOnly: false)
        }
        pen.lastEff = eff

        host?.penPressure(voice: pen.voice, force: force)
        // (Tilt -> CC 1 removed — #40: altitude was a nuisance; the mod now
        // belongs to the barrel-roll dial above.)
    }

    private func dyCH_forSlide(_ loc: CGPoint, _ pen: ActivePen) -> Float {
        return Float((loc.y - pen.anchor.y) / max(bounds.height, 1))
    }

    /// #40 (corrected): the gesture decay — called each frame by the host.
    /// A delta's life as a multiplier: value ×= exp(−dt/0.4 s). The bend is
    /// re-emitted at the decaying scale (change-only downstream); the mod
    /// falls to an explicit 0 so the vortex always stills.
    func penGestureTick(dt: Double) {
        guard !pens.isEmpty, dt > 0 else { return }
        let k = Float(exp(-dt / GESTURE_TAU))
        for (id, var pen) in pens {
            var touched = false
            if pen.bendBoost > 0 {
                pen.bendBoost *= k
                if pen.bendBoost < 0.01 { pen.bendBoost = 0 }
                host?.penGlide(voice: pen.voice, note: pen.lastNote,
                               offset: pen.lastOffset,
                               scale: 1.0 + min(pen.bendBoost, 2.0),   // settles to ×1
                               velocity: pen.lastVel)
                touched = true
            }
            if touched { pens[id] = pen }
        }
    }

    /// #61: drive the strip's sustain engine from the pen, then refresh the
    /// palette's pad so panel and pen never disagree.
    private func setPenSustain(_ down: Bool) {
        guard !isHidden else { return }          // Play mode only
        guard down != penSustainDown else { return }
        penSustainDown = down
        if down { host?.stripSustainDown() } else { host?.stripSustainUp() }
        host?.syncStripMirrors()
        NSLog("[pen] squeeze %@ -> sustain %@",
              down ? "pressed" : "released", down ? "on" : "off")
    }

    /// Pencil Pro squeeze (iOS 17.5+): began = pedal down, ended/cancelled =
    /// up; `.changed` is the hold and needs no message (change-only).
    @available(iOS 17.5, *)
    func pencilInteraction(_ interaction: UIPencilInteraction,
                           didReceiveSqueeze squeeze: UIPencilInteraction.Squeeze) {
        switch squeeze.phase {
        case .began:            setPenSustain(true)
        case .ended, .cancelled: setPenSustain(false)
        default:                break
        }
    }

    /// Pencil 2 fallback: a double-tap has no hold, so it LATCHES the pedal
    /// (tap on, tap off) — the same engine, the same pad.
    func pencilInteractionDidTap(_ interaction: UIPencilInteraction) {
        setPenSustain(!penSustainDown)
    }

    @objc private func onHover(_ g: UIHoverGestureRecognizer) {
        switch g.state {
        case .began, .changed:
            hoverPoint = g.location(in: self)
        default:
            hoverPoint = nil
        }
        setNeedsDisplay()
    }
    override func touchesCancelled(_ ts: Set<UITouch>, with event: UIEvent?) {
        touchesEnded(ts, with: event)
    }

    /// Leaving Play mode (or a layout change): end every held voice cleanly —
    /// pressure 0 + Note Off through the same path, no stuck notes.
    func releaseAllTouches() {
        for (_, at) in touches { host?.playTouchEnd(voice: at.voice, lift: 64) }
        for (_, pen) in pens { host?.playTouchEnd(voice: pen.voice, lift: 64, isPen: true) }
        touches.removeAll()
        pens.removeAll()
        setNeedsDisplay()
    }

    // -- indicator drawing -----------------------------------------------------

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext() else { return }
        // Saturation HUD blink (§5.1): a brief border flash, no note stolen.
        if CACurrentMediaTime() < saturationBlinkUntil {
            ctx.setStrokeColor(UIColor.systemRed.withAlphaComponent(0.6).cgColor)
            ctx.setLineWidth(6)
            ctx.stroke(bounds.insetBy(dx: 3, dy: 3))
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.4) { [weak self] in
                self?.setNeedsDisplay()
            }
        }
        // §7 hover ghost cursor (Pencil hover, M2 iPads).
        if let hp = hoverPoint {
            ctx.setStrokeColor(UIColor.black.withAlphaComponent(0.25).cgColor)
            ctx.setLineWidth(1.0)
            ctx.strokeEllipse(in: CGRect(x: hp.x - 9, y: hp.y - 9, width: 18, height: 18))
            ctx.setFillColor(UIColor.black.withAlphaComponent(0.3).cgColor)
            ctx.fillEllipse(in: CGRect(x: hp.x - 2, y: hp.y - 2, width: 4, height: 4))
        }
        guard !touches.isEmpty else { return }
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

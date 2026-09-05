// Performance control strip (PROJECT_SPEC.md §8.8, Step 18): a dockable bar of
// wheels and buttons built from the existing joystick primitive — a touch
// anchors its origin and the §3.2 soft knee shapes Δy (via hostmpe, one
// implementation). All VALUE state lives in hostmpe_strip_t on the host's
// MIDI queue; this view keeps display mirrors only. Every message the strip
// causes goes out on the master channel — the widget engines enforce that,
// unit-tested headlessly.
import UIKit
import HostMPE

final class ControlStripView: UIView, UIGestureRecognizerDelegate {
    weak var host: SumiCanvasView?

    // Widget slots, left to right. rawValue orders the layout.
    enum Widget: Int, CaseIterable {
        case pitch = 0, mod, assignA, assignB, sustain
    }

    // Travel bound for the wheel joysticks, in points: the §3.2 knee needs Δ
    // and r_max in ONE metric; points are that metric here (the strip is a
    // fixed-height bar, not the lattice — canvas-height units would tie the
    // feel to the canvas size).
    private let wheelTravelPt: Float = 60
    // Full-travel latch sweep in CC units per grab (slow drags accumulate).
    private let latchSweep: Float = 64

    private struct Grab {
        let widget: Widget
        let originY: CGFloat
        var lastShaped: Float   // clamped joystick position at the last event
    }
    private var grabs: [ObjectIdentifier: Grab] = [:]

    // Display mirrors (the engine on the MIDI queue is the truth).
    private var mirrorPitch: Float = 0          // [-1, 1]
    private var mirrorLatch: [Float] = [0, 0, 0]
    private var mirrorSustain = false
    private var mirrorToggleMode = false
    private var mirrorCCs: [UInt8] = [1, 23, 24]
    private var ramping = false

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        isOpaque = false
        // Compact floating palette (§8 rev, DECISIONS_3 #31): translucent
        // enough that the marbling reads through, opaque enough to find.
        backgroundColor = UIColor.white.withAlphaComponent(0.42)
        layer.borderWidth = 0.5
        layer.borderColor = UIColor.black.withAlphaComponent(0.15).cgColor
        layer.cornerRadius = 12
        let lp = UILongPressGestureRecognizer(target: self, action: #selector(onLongPress))
        lp.minimumPressDuration = 0.5
        // The editor gesture may only ever see ASSIGNABLE-wheel touches: a
        // recognizer that fires cancels the view's touches (UIKit default),
        // which was releasing a HELD sustain pedal after 0.5 s — the
        // "unpresses while I'm pressing it" device bug (DECISIONS_3 #31).
        lp.delegate = self
        addGestureRecognizer(lp)
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    func gestureRecognizer(_ g: UIGestureRecognizer,
                           shouldReceive touch: UITouch) -> Bool {
        let w = widget(at: touch.location(in: self))
        return w == .assignA || w == .assignB
    }

    // Called by the host whenever engine state changed outside a local touch
    // (mode switches, assignment results, the return ramp finishing).
    func syncMirrors(pitch: Float, latch: [Float], sustain: Bool,
                     toggleMode: Bool, ccs: [UInt8]) {
        mirrorPitch = pitch
        mirrorLatch = latch
        mirrorSustain = sustain
        mirrorToggleMode = toggleMode
        mirrorCCs = ccs
        setNeedsDisplay()
    }

    // -- geometry --------------------------------------------------------------

    private func slotRect(_ w: Widget) -> CGRect {
        let n = CGFloat(Widget.allCases.count)
        let sw = bounds.width / n
        return CGRect(x: CGFloat(w.rawValue) * sw, y: 0, width: sw,
                      height: bounds.height).insetBy(dx: 6, dy: 6)
    }

    private func widget(at p: CGPoint) -> Widget? {
        for w in Widget.allCases where slotRect(w).insetBy(dx: -6, dy: -6).contains(p) {
            return w
        }
        return nil
    }

    // -- touches: the joystick primitive per widget ----------------------------

    // Clamped, knee-shaped vertical deflection for a wheel grab (up = +1).
    private func shaped(_ dyPt: CGFloat) -> Float {
        var ex: Float = 0, ey: Float = 0
        hostmpe_joystick_eff(0, Float(dyPt), wheelTravelPt, &ex, &ey)
        return -ey   // screen y grows down; up is positive
    }

    override func touchesBegan(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            let p = t.location(in: self)
            guard let w = widget(at: p) else { continue }
            // Every widget touch is TRACKED (sustain included): the release
            // must find its widget by the grab record, never by where the
            // finger happens to be when it lifts.
            grabs[ObjectIdentifier(t)] = Grab(widget: w, originY: p.y, lastShaped: 0)
            switch w {
            case .sustain:
                mirrorSustain = mirrorToggleMode ? !mirrorSustain : true
                host?.stripSustainDown()
            case .pitch:
                ramping = false
            case .mod, .assignA, .assignB:
                break
            }
        }
        setNeedsDisplay()
    }

    override func touchesMoved(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            guard var g = grabs[ObjectIdentifier(t)] else { continue }
            let s = shaped(t.location(in: self).y - g.originY)
            switch g.widget {
            case .pitch:
                mirrorPitch = s
                host?.stripPitchMove(s)
            case .mod, .assignA, .assignB:
                // Relative accumulation (§8): only the CHANGE in shaped
                // position moves the value — a regrasp contributes zero.
                let wheel = Int32(g.widget.rawValue - Widget.mod.rawValue)
                let delta = (s - g.lastShaped) * latchSweep
                let idx = Int(wheel)
                mirrorLatch[idx] = min(max(mirrorLatch[idx] + delta, 0), 127)
                host?.stripLatchMove(wheel: wheel, delta: delta)
            case .sustain:
                break
            }
            g.lastShaped = s
            grabs[ObjectIdentifier(t)] = g
        }
        setNeedsDisplay()
    }

    override func touchesEnded(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            guard let g = grabs.removeValue(forKey: ObjectIdentifier(t)) else { continue }
            switch g.widget {
            case .pitch:
                host?.stripPitchRelease()
                animateSpringReturn()
            case .sustain:
                if !mirrorToggleMode { mirrorSustain = false }
                host?.stripSustainUp()   // toggle mode: engine-side no-op
            case .mod, .assignA, .assignB:
                break
            }
        }
        setNeedsDisplay()
    }
    override func touchesCancelled(_ ts: Set<UITouch>, with event: UIEvent?) {
        touchesEnded(ts, with: event)
    }

    // Visual mirror of the engine's 50 ms return ramp (the MIDI ramp itself
    // is emitted by hostmpe_strip_tick on the host's frame drain).
    private func animateSpringReturn() {
        ramping = true
        let v0 = mirrorPitch
        let t0 = CACurrentMediaTime()
        func step() {
            guard ramping else { return }
            let f = Float((CACurrentMediaTime() - t0) / 0.050)
            if f >= 1 {
                mirrorPitch = 0
                ramping = false
            } else {
                mirrorPitch = v0 * (1 - f)
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.016) { step() }
            }
            setNeedsDisplay()
        }
        step()
    }

    // -- long-press: the assignable-wheel CC editor (§8) -----------------------

    @objc private func onLongPress(_ g: UILongPressGestureRecognizer) {
        guard g.state == .began, let w = widget(at: g.location(in: self)),
              w == .assignA || w == .assignB else { return }
        let wheel = Int32(w.rawValue - Widget.mod.rawValue)
        let current = mirrorCCs[Int(wheel)]
        let alert = UIAlertController(
            title: "Assign wheel \(w == .assignA ? "A" : "B")",
            message: "7-bit CC number (0–127). Currently CC \(current). "
                + "Protocol CCs (1, 6, 38, 64, 98–101, 120–127) are refused.",
            preferredStyle: .alert)
        alert.addTextField { tf in
            tf.keyboardType = .numberPad
            tf.text = "\(current)"
        }
        alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
        alert.addAction(UIAlertAction(title: "Assign", style: .default) { [weak self] _ in
            guard let self, let text = alert.textFields?.first?.text,
                  let cc = UInt8(text) else { return }
            self.host?.stripAssign(wheel: wheel, cc: cc) { ok, assigned in
                DispatchQueue.main.async {
                    if ok { self.mirrorCCs[Int(wheel)] = assigned }
                    self.setNeedsDisplay()
                }
            }
        })
        window?.rootViewController?.present(alert, animated: true)
    }

    // -- drawing ---------------------------------------------------------------

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext() else { return }
        let ink = UIColor.black
        for w in Widget.allCases {
            let r = slotRect(w)
            let path = UIBezierPath(roundedRect: r, cornerRadius: 8)
            ctx.setStrokeColor(ink.withAlphaComponent(0.25).cgColor)
            ctx.setLineWidth(1)
            ctx.addPath(path.cgPath)
            ctx.strokePath()

            let label: String
            switch w {
            case .pitch:   label = "Pitch"
            case .mod:     label = "Mod"
            case .assignA: label = "CC \(mirrorCCs[1])"
            case .assignB: label = "CC \(mirrorCCs[2])"
            case .sustain: label = mirrorToggleMode ? "Sus ⇥" : "Sus"
            }
            draw(text: label, in: CGRect(x: r.minX, y: r.maxY - 16,
                                         width: r.width, height: 14), ink: ink)

            if w == .sustain {
                let pad = r.insetBy(dx: r.width * 0.22, dy: r.height * 0.24)
                    .offsetBy(dx: 0, dy: -6)
                let bp = UIBezierPath(roundedRect: pad, cornerRadius: 6)
                if mirrorSustain {
                    ctx.setFillColor(ink.withAlphaComponent(0.45).cgColor)
                    ctx.addPath(bp.cgPath)
                    ctx.fillPath()
                } else {
                    ctx.setStrokeColor(ink.withAlphaComponent(0.4).cgColor)
                    ctx.addPath(bp.cgPath)
                    ctx.strokePath()
                }
                continue
            }

            // Wheel track + thumb.
            let track = CGRect(x: r.midX - 1.5, y: r.minY + 8,
                               width: 3, height: r.height - 32)
            ctx.setFillColor(ink.withAlphaComponent(0.12).cgColor)
            ctx.fill(track)
            let frac: CGFloat   // 0 bottom .. 1 top of the track
            if w == .pitch {
                frac = CGFloat(0.5 + 0.5 * mirrorPitch)
                // center notch
                ctx.setFillColor(ink.withAlphaComponent(0.3).cgColor)
                ctx.fill(CGRect(x: r.midX - 8, y: track.midY - 0.5, width: 16, height: 1))
            } else {
                frac = CGFloat(mirrorLatch[w.rawValue - Widget.mod.rawValue] / 127.0)
            }
            let ty = track.maxY - frac * track.height
            ctx.setFillColor(ink.withAlphaComponent(0.55).cgColor)
            ctx.fillEllipse(in: CGRect(x: r.midX - 7, y: ty - 7, width: 14, height: 14))
        }
    }

    private func draw(text: String, in rect: CGRect, ink: UIColor) {
        let style = NSMutableParagraphStyle()
        style.alignment = .center
        (text as NSString).draw(
            in: rect,
            withAttributes: [.font: UIFont.systemFont(ofSize: 11, weight: .medium),
                             .foregroundColor: ink.withAlphaComponent(0.6),
                             .paragraphStyle: style])
    }
}

// The fingering panel (Phase 9 step 63, the author's fixes — DECISIONS_8 #14, #15): the trumpet's three
// valves and the trombone's slide as LARGE controls, in one of two forms — a tall panel at the side of
// the sheet at mid-height, under the hand that does not play the partials, or a wide one along the
// bottom edge. The strip keeps the wheels, the pedal, Next and Panic at the opposite corner. Vertical,
// the valves stack top to bottom (1, 2, 3: a hand reaching from the side rests its three fingers on
// them) and the slide is a vertical track, the 1st position at the top, the 7th at the bottom — down is
// out, lower. Horizontal, the pads lie side by side with 1 under the index finger and the slide's 1st
// position is at the hand's near side. The panel's own rotate button (the corner towards the sheet)
// flips the form and persists it — the settings' toggle is the same switch. All VALUE state lives in
// hostmpe_strip_t on the host's MIDI queue (the valve and slide engines); this view keeps display
// mirrors only. A valve stays down until its finger lifts, wherever the finger wanders — real valves.
// Every message rides the master channel.
import UIKit
import HostMPE

final class FingeringPanelView: UIView {
    weak var host: SumiCanvasView?

    enum Mode { case none, valves, slide }
    enum Orientation { case vertical, horizontal }   // the author's ask: both forms
    private(set) var mode: Mode = .none
    private(set) var orientation: Orientation = .vertical
    private var mirrored = false   // left-handed: the index finger's pad and the 1st position at the hand's near side
    private var mirrorValves: UInt32 = 0
    private var mirrorSlide: Float = 0
    private var grabs: [ObjectIdentifier: Int] = [:]   // touch -> the valve it holds (0..2), or -1 for the slide
    private(set) var darkTheme = false

    private let gap: CGFloat = 12
    private let headerH: CGFloat = 30   // the band above the pads that holds the rotate button

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        isOpaque = false
        layer.borderWidth = 0.5
        layer.cornerRadius = 18
        applyTheme()
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    /// The panel's natural size (the host clamps it to the sheet): tall at the side, wide at the bottom.
    var preferredSize: CGSize {
        switch (mode, orientation) {
        case (.slide, .vertical):   return CGSize(width: 104, height: 460)
        case (.slide, .horizontal): return CGSize(width: 460, height: 104)
        case (_, .vertical):        return CGSize(width: 140, height: 420)
        case (_, .horizontal):      return CGSize(width: 400, height: 150)
        }
    }

    func setMode(_ m: Mode) {
        guard m != mode else { return }
        mode = m
        grabs.removeAll()
        setNeedsDisplay()
    }
    func setOrientation(_ o: Orientation) {
        guard o != orientation else { return }
        orientation = o
        grabs.removeAll()
        setNeedsDisplay()
    }
    func setMirror(_ on: Bool) {
        guard on != mirrored else { return }
        mirrored = on
        setNeedsDisplay()
    }
    func syncFingering(valves: UInt32, slide: Float) {
        mirrorValves = valves
        mirrorSlide = slide
        setNeedsDisplay()
    }
    func setDarkTheme(_ dark: Bool) {
        guard dark != darkTheme else { return }
        darkTheme = dark
        applyTheme()
        setNeedsDisplay()
    }
    private func applyTheme() {
        backgroundColor = darkTheme ? UIColor.black.withAlphaComponent(0.45) : UIColor.white.withAlphaComponent(0.42)
        layer.borderColor = (darkTheme ? UIColor.white.withAlphaComponent(0.22) : UIColor.black.withAlphaComponent(0.15)).cgColor
    }

    // -- geometry --------------------------------------------------------------

    // the rotate button: the top corner towards the sheet (the right; the left when mirrored)
    private var rotateRect: CGRect {
        CGRect(x: mirrored ? 6 : bounds.width - 32, y: 4, width: 26, height: 26)
    }
    // valve k's pad: stacked top to bottom under the header band, or side by side — 1 under the index
    // finger, so right to left when the hand comes from the right (mirrored)
    private func padRect(_ k: Int) -> CGRect {
        if orientation == .vertical {
            let h = (bounds.height - headerH - 3 * gap) / 3
            return CGRect(x: gap, y: headerH + CGFloat(k) * (h + gap), width: bounds.width - 2 * gap, height: h)
        }
        let w = (bounds.width - 4 * gap) / 3
        let i = mirrored ? 2 - k : k
        return CGRect(x: gap + CGFloat(i) * (w + gap), y: headerH, width: w, height: bounds.height - headerH - gap)
    }
    private var trackRect: CGRect {
        orientation == .vertical ? bounds.insetBy(dx: bounds.width * 0.3, dy: 40) : bounds.insetBy(dx: 34, dy: bounds.height * 0.3)
    }
    // the slide's position 0..1 along the track: the 1st position at the top, or at the hand's near side
    // (the left; the right when mirrored), the 7th away from it
    private func slidePosition(_ p: CGPoint) -> Float {
        let t = trackRect
        if orientation == .vertical {
            guard t.height > 0 else { return 0 }
            return Float(min(max((p.y - t.minY) / t.height, 0), 1))
        }
        guard t.width > 0 else { return 0 }
        let f = Float(min(max((p.x - t.minX) / t.width, 0), 1))
        return mirrored ? 1 - f : f
    }
    private func trackPoint(_ pos: Float) -> CGPoint {   // where a position sits on the track
        let t = trackRect
        if orientation == .vertical { return CGPoint(x: t.midX, y: t.minY + t.height * CGFloat(pos)) }
        let f = mirrored ? 1 - pos : pos
        return CGPoint(x: t.minX + t.width * CGFloat(f), y: t.midY)
    }

    // -- touches ---------------------------------------------------------------

    override func touchesBegan(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            let p = t.location(in: self)
            if mode != .none && rotateRect.insetBy(dx: -6, dy: -6).contains(p) {   // the rotate button: no grab
                host?.toggleFingeringOrientation()
                continue
            }
            switch mode {
            case .valves:
                for k in 0..<3 where padRect(k).insetBy(dx: -gap / 2, dy: -gap / 2).contains(p) {
                    grabs[ObjectIdentifier(t)] = k
                    mirrorValves |= 1 << UInt32(k)
                    host?.stripValveDown(Int32(k))
                    break
                }
            case .slide:
                grabs[ObjectIdentifier(t)] = -1
                let pos = slidePosition(p)
                mirrorSlide = pos
                host?.stripSlideSet(pos)
            case .none:
                break
            }
        }
        setNeedsDisplay()
    }

    override func touchesMoved(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            guard let g = grabs[ObjectIdentifier(t)], g < 0 else { continue }   // a valve stays where its finger first landed
            let pos = slidePosition(t.location(in: self))
            mirrorSlide = pos
            host?.stripSlideSet(pos)
        }
        setNeedsDisplay()
    }

    override func touchesEnded(_ ts: Set<UITouch>, with event: UIEvent?) {
        for t in ts {
            guard let g = grabs.removeValue(forKey: ObjectIdentifier(t)) else { continue }
            if g >= 0 {
                mirrorValves &= ~(1 << UInt32(g))
                host?.stripValveUp(Int32(g))
            }
        }
        setNeedsDisplay()
    }
    override func touchesCancelled(_ ts: Set<UITouch>, with event: UIEvent?) {
        touchesEnded(ts, with: event)
    }

    // -- drawing ---------------------------------------------------------------

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext() else { return }
        let ink: UIColor = darkTheme ? .white : .black
        switch mode {
        case .valves:
            for k in 0..<3 {
                let r = padRect(k)
                let path = UIBezierPath(roundedRect: r, cornerRadius: 14)
                let down = (mirrorValves & (1 << UInt32(k))) != 0
                if down {
                    ctx.setFillColor(ink.withAlphaComponent(0.45).cgColor)
                    ctx.addPath(path.cgPath)
                    ctx.fillPath()
                } else {
                    ctx.setStrokeColor(ink.withAlphaComponent(0.4).cgColor)
                    ctx.setLineWidth(1.5)
                    ctx.addPath(path.cgPath)
                    ctx.strokePath()
                }
                draw(text: "\(k + 1)", in: CGRect(x: r.minX, y: r.midY - 16, width: r.width, height: 32),
                     ink: down ? (darkTheme ? .black : .white) : ink, size: 26)
            }
        case .slide:
            let t = trackRect
            let vertical = orientation == .vertical
            ctx.setFillColor(ink.withAlphaComponent(0.12).cgColor)
            if vertical { ctx.fill(CGRect(x: t.midX - 2, y: t.minY, width: 4, height: t.height)) }
            else { ctx.fill(CGRect(x: t.minX, y: t.midY - 2, width: t.width, height: 4)) }
            for k in 0..<HOSTMPE_SLIDE_POSITIONS {
                let p = trackPoint(Float(k) / Float(HOSTMPE_SLIDE_POSITIONS - 1))
                ctx.setFillColor(ink.withAlphaComponent(0.3).cgColor)
                if vertical {
                    ctx.fill(CGRect(x: t.minX, y: p.y - 0.5, width: t.width, height: 1))
                    draw(text: "\(k + 1)", in: CGRect(x: 2, y: p.y - 8, width: t.minX - 4, height: 16), ink: ink, size: 11)
                } else {
                    ctx.fill(CGRect(x: p.x - 0.5, y: t.minY, width: 1, height: t.height))
                    draw(text: "\(k + 1)", in: CGRect(x: p.x - 12, y: t.maxY + 4, width: 24, height: 16), ink: ink, size: 11)
                }
            }
            let p = trackPoint(mirrorSlide)
            let thumb = vertical ? CGRect(x: t.minX - 6, y: p.y - 11, width: t.width + 12, height: 22)
                                 : CGRect(x: p.x - 11, y: t.minY - 6, width: 22, height: t.height + 12)
            ctx.setFillColor(ink.withAlphaComponent(0.55).cgColor)
            ctx.addPath(UIBezierPath(roundedRect: thumb, cornerRadius: 8).cgPath)
            ctx.fillPath()
            if vertical { draw(text: "Slide", in: CGRect(x: 0, y: 10, width: bounds.width, height: 16), ink: ink, size: 11) }
            else { draw(text: "Slide", in: CGRect(x: 0, y: 6, width: bounds.width, height: 16), ink: ink, size: 11) }
        case .none:
            return
        }
        // the rotate button, the same corner in every form
        let cfg = UIImage.SymbolConfiguration(pointSize: 17, weight: .medium)
        if let glyph = UIImage(systemName: "rotate.right", withConfiguration: cfg)?
            .withTintColor(ink.withAlphaComponent(0.6), renderingMode: .alwaysOriginal) {
            let r = rotateRect
            let s = glyph.size
            glyph.draw(in: CGRect(x: r.midX - s.width / 2, y: r.midY - s.height / 2, width: s.width, height: s.height))
        }
    }

    private func draw(text: String, in rect: CGRect, ink: UIColor, size: CGFloat) {
        let style = NSMutableParagraphStyle()
        style.alignment = .center
        (text as NSString).draw(
            in: rect,
            withAttributes: [.font: UIFont.systemFont(ofSize: size, weight: .medium),
                             .foregroundColor: ink.withAlphaComponent(0.75),
                             .paragraphStyle: style])
    }
}

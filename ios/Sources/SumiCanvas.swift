// The §5.4 shell proper: a UIViewRepresentable whose backing UIView overrides
// layerClass to CAMetalLayer, hands the layer to sumi_create (backend METAL),
// drives sumi_update/sumi_render from a CADisplayLink, and forwards resizes
// from layoutSubviews with contentScaleFactor as pixel_ratio. Touch gestures
// reuse the ABI gesture calls with the desktop harness's constants.
import SwiftUI
import UIKit
import Metal
import QuartzCore
import os.signpost
import SumiCore
import HostMPE

struct SumiCanvas: UIViewRepresentable {
    var simScale: Float
    var layout: UInt32
    var playMode: Bool
    var velocityFromTouchSize: Bool

    func makeUIView(context: Context) -> SumiCanvasView { SumiCanvasView() }
    func updateUIView(_ view: SumiCanvasView, context: Context) {
        view.setSimScale(simScale)
        view.setLayout(layout)
        view.velocityFromTouchSize = velocityFromTouchSize
        view.setPlayMode(playMode)
    }
}

private func sumiLog(_ level: Int32, _ msg: UnsafePointer<CChar>?, _ user: UnsafeMutableRawPointer?) {
    if let msg { NSLog("[sumi %d] %@", level, String(cString: msg)) }
}

final class SumiCanvasView: UIView, UIGestureRecognizerDelegate {
    // Single canvas per app; statically reachable for scene-phase forwarding
    // and the settings status line.
    static weak var shared: SumiCanvasView?

    override class var layerClass: AnyClass { CAMetalLayer.self }

    // Host-owned sim_scale default (§ params comment): "iPad-class GPU" is
    // read as Apple GPU family 7+ (A14/M1 and newer) — 1.0 there, 0.75 below.
    static let defaultsToFullResolution: Bool = {
        guard let dev = MTLCreateSystemDefaultDevice() else { return false }
        return dev.supportsFamily(.apple7)
    }()

    private var inst: OpaquePointer?
    private var link: CADisplayLink?
    private var midi: MidiSource?
    private var lastFrameTime: CFTimeInterval = 0
    private var pendingSimScale: Float = SumiCanvasView.defaultsToFullResolution ? 1.0 : 0.75
    private var sceneActive = true
    private var resizeOnActivate = false

    // Screen sleep: a performance must not be interrupted by the idle timer.
    // MIDI (any thread) and touches mark activity; the display stays awake
    // for IDLE_KEEPAWAKE_S after the last event, then may sleep normally.
    private let IDLE_KEEPAWAKE_S: CFTimeInterval = 180
    private let activityLock = NSLock()
    private var lastActivity: CFTimeInterval = 0
    private var idleTimerDisabled = false

    func markActivity() {
        activityLock.lock()
        lastActivity = CACurrentMediaTime()
        activityLock.unlock()
    }

    // Desktop harness gesture constants (desktop/src/main.cpp), verbatim.
    private let DROP_RADIUS: Float = 0.06
    private let TINE_ALPHA: Float = 0.035
    private let VORTEX_RADIUS: Float = 0.18
    private let VORTEX_STRENGTH: Float = 4.0
    private let DRAG_THRESHOLD_PT: CGFloat = 5.0
    private var panLast = CGPoint.zero
    private var rotLast: CGFloat = 0
    private var twist: UIRotationGestureRecognizer?
    private var pendingLayout: UInt32 = 0
    private var marbleRecognizers: [UIGestureRecognizer] = []
    private let overlay = PlayOverlayView()
    private var playModeRequested = false
    private var playEffective = false
    private(set) var paramsSnapshot = sumi_params_t()

    // -- Phase 4 §5.2: the serial MIDI queue is the SOLE producer -----------
    // Every byte — CoreMIDI devices AND touch-generated — crosses
    // sumi_push_midi only from this queue; hostmpe state lives on it too.
    private let midiQueue = DispatchQueue(label: "com.vibetuned.midi-sink.midi")
    private var mpe: OpaquePointer?   // hostmpe_t*, touched only on midiQueue
    var velocityFromTouchSize = false

    // Byte log at the merge point (Step 16 evidence: emit-order and
    // channel-steal asserts). Appended on midiQueue only; flushed with the
    // session log. src: 0 = external device, 1 = touch, 2 = session config.
    private var byteLog: [(t: Double, s: UInt8, d1: UInt8, d2: UInt8, src: UInt8)] = []
    private let byteLogCap = 300_000

    // Touch-down -> visible drop latency (DONE: ≤ 2 frames). Marked at
    // touch-down on the main thread, resolved after the next sumi_render.
    private var latencyMarks: [CFTimeInterval] = []
    private var latencySamples: [Double] = []
    private let signposter = OSSignposter(subsystem: "com.vibetuned.midi-sink",
                                          category: "play")

    // Session evidence log (DONE: 10-minute 60 fps session, thermal trace).
    private var sessionStart: CFTimeInterval = 0
    private var secondStart: CFTimeInterval = 0
    private var framesThisSecond = 0
    private var worstFrameMs: Double = 0
    private var logLines: [String] = []
    private(set) var statusLine = ""

    override init(frame: CGRect) {
        super.init(frame: frame)
        SumiCanvasView.shared = self
        isMultipleTouchEnabled = true
        // The recognizers must run SIMULTANEOUSLY (delegate below): with the
        // default mutual exclusion, the one-finger pan claims the first touch
        // and the two-finger twist can never begin.
        let tap = UITapGestureRecognizer(target: self, action: #selector(onTap))
        let pan = UIPanGestureRecognizer(target: self, action: #selector(onPan))
        pan.maximumNumberOfTouches = 1
        let rot = UIRotationGestureRecognizer(target: self, action: #selector(onTwist))
        for g in [tap, pan, rot] as [UIGestureRecognizer] {
            g.delegate = self
            addGestureRecognizer(g)
        }
        twist = rot
        marbleRecognizers = [tap, pan, rot]
        // Play-mode overlay (Phase 4 §6): hidden and interaction-inert in
        // Marble mode, so the marble gesture path stays bit-identical.
        overlay.paramsProvider = { [weak self] in self?.paramsSnapshot ?? sumi_params_t() }
        overlay.host = self
        overlay.isHidden = true
        overlay.isUserInteractionEnabled = false
        addSubview(overlay)
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    // -- lifecycle -----------------------------------------------------------

    override func didMoveToWindow() {
        super.didMoveToWindow()
        if let window {
            contentScaleFactor = window.screen.scale
            setNeedsLayout()
        } else {
            teardown()
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        // While backgrounded, iOS drives snapshot layout passes in BOTH
        // orientations for the app switcher — resizing the core for those
        // would churn the field twice per backgrounding. Defer to reactivation.
        guard sceneActive else { resizeOnActivate = true; return }
        let w = UInt32(bounds.width * contentScaleFactor)
        let h = UInt32(bounds.height * contentScaleFactor)
        guard w > 0, h > 0, window != nil else { return }
        overlay.frame = bounds
        if inst == nil {
            start(width: w, height: h)
        } else {
            sumi_resize(inst, w, h, Float(contentScaleFactor))
        }
    }

    private func start(width: UInt32, height: UInt32) {
        var config = sumi_config_t(
            native_surface_handle: Unmanaged.passUnretained(layer).toOpaque(),
            backend: SUMI_BACKEND_METAL,
            width: width, height: height,
            pixel_ratio: Float(contentScaleFactor),
            log_cb: sumiLog, log_user: nil)
        guard let created = sumi_create(&config) else {
            NSLog("[shell] sumi_create failed")
            return
        }
        inst = created
        midiQueue.sync { mpe = hostmpe_create() }
        applyParams()
        midi = MidiSource { [weak self] status, d1, d2 in
            // CoreMIDI thread -> hop to the serial MIDI queue: the SOLE
            // producer (§5.2). The merge point also feeds hostmpe's
            // external-occupancy mask (§5.1) and the byte log.
            guard let self else { return }
            self.midiQueue.async {
                guard let inst = self.inst else { return }
                if let mpe = self.mpe {
                    hostmpe_observe_external(mpe, CACurrentMediaTime(), status, d1, d2)
                }
                self.logByte(status, d1, d2, src: 0)
                sumi_push_midi(inst, status, d1, d2)
            }
            self.markActivity()
        }
        midi?.onSourcesRemoved = { [weak self] in
            guard let self else { return }
            self.midiQueue.async {
                if let mpe = self.mpe { hostmpe_external_clear(mpe) }
            }
        }
        sessionStart = CACurrentMediaTime()
        secondStart = sessionStart
        logLines = ["t_s,fps,worst_frame_ms,thermal"]
        let l = CADisplayLink(target: self, selector: #selector(tick))
        l.add(to: .main, forMode: .common)
        link = l
        NSLog("[shell] sumi %d.%d.%d ready, %ux%u @%.1fx",
              sumi_version() >> 16, (sumi_version() >> 8) & 0xFF, sumi_version() & 0xFF,
              width, height, contentScaleFactor)
    }

    private func teardown() {
        link?.invalidate(); link = nil
        midi?.stop(); midi = nil
        flushSessionLog()
        midiQueue.sync {
            if let mpe { hostmpe_destroy(mpe) }
            mpe = nil
        }
        if let inst { sumi_destroy(inst) }
        inst = nil
    }

    func setScenePhaseActive(_ active: Bool) {
        sceneActive = active
        link?.isPaused = !active
        if active {
            lastFrameTime = 0   // don't integrate the paused gap as dt
            if resizeOnActivate {
                resizeOnActivate = false
                setNeedsLayout()
            }
        } else {
            flushSessionLog()
            if idleTimerDisabled {
                idleTimerDisabled = false
                UIApplication.shared.isIdleTimerDisabled = false
            }
        }
    }

    // -- frame loop ----------------------------------------------------------

    @objc private func tick(_ link: CADisplayLink) {
        guard let inst, sceneActive else { return }
        let now = link.timestamp
        let dt = lastFrameTime > 0 ? now - lastFrameTime : link.duration
        lastFrameTime = now

        let t0 = CACurrentMediaTime()
        sumi_update(inst, dt)
        sumi_render(inst)
        let frameMs = (CACurrentMediaTime() - t0) * 1000.0

        // Touch-down -> this render is the first that can show the drop
        // (loopback bytes enqueued before this frame's update drained them).
        if !latencyMarks.isEmpty {
            let t = CACurrentMediaTime()
            for mark in latencyMarks { latencySamples.append((t - mark) * 1000.0) }
            latencyMarks.removeAll()
        }

        framesThisSecond += 1
        worstFrameMs = max(worstFrameMs, frameMs)
        if now - secondStart >= 1.0 {
            // Idle-timer control, re-evaluated once per second on the main
            // thread (setting the flag is idempotent).
            activityLock.lock()
            let sinceActivity = CACurrentMediaTime() - lastActivity
            activityLock.unlock()
            let keepAwake = sinceActivity < IDLE_KEEPAWAKE_S
            if keepAwake != idleTimerDisabled {
                idleTimerDisabled = keepAwake
                UIApplication.shared.isIdleTimerDisabled = keepAwake
            }
            let fps = Double(framesThisSecond) / (now - secondStart)
            let thermal = Self.thermalName(ProcessInfo.processInfo.thermalState)
            let dropped = sumi_dropped_midi_count(inst)
            let lat = latencySamples.last.map { String(format: "  touch %.1f ms", $0) } ?? ""
            statusLine = String(format: "t=%.0fs  %.1f fps  worst %.2f ms  thermal %@  dropped %u%@",
                                now - sessionStart, fps, worstFrameMs, thermal, dropped, lat)
            logLines.append(String(format: "%.0f,%.1f,%.2f,%@",
                                   now - sessionStart, fps, worstFrameMs, thermal))
            framesThisSecond = 0
            worstFrameMs = 0
            secondStart = now
            if logLines.count % 30 == 0 { flushSessionLog() }
        }
    }

    private static func thermalName(_ s: ProcessInfo.ThermalState) -> String {
        switch s {
        case .nominal: return "nominal"
        case .fair: return "fair"
        case .serious: return "serious"
        case .critical: return "critical"
        @unknown default: return "unknown"
        }
    }

    private func flushSessionLog() {
        guard let dir = FileManager.default.urls(for: .documentDirectory,
                                                 in: .userDomainMask).first else { return }
        if logLines.count > 1 {
            try? logLines.joined(separator: "\n").appending("\n")
                .write(to: dir.appendingPathComponent("session_log.csv"),
                       atomically: true, encoding: .utf8)
        }
        if !latencySamples.isEmpty {
            let body = "touch_to_render_ms\n"
                + latencySamples.map { String(format: "%.2f", $0) }.joined(separator: "\n")
            try? body.appending("\n")
                .write(to: dir.appendingPathComponent("latency_log.csv"),
                       atomically: true, encoding: .utf8)
        }
        flushByteLog()
    }

    // -- params --------------------------------------------------------------

    func setSimScale(_ s: Float) {
        pendingSimScale = s
        applyParams()
    }

    func setLayout(_ l: UInt32) {
        pendingLayout = l
        applyParams()
    }

    /// Play mode (Phase 4): effective only on the playable layouts (grid,
    /// Jankó — the probe refuses everything else anyway); Marble mode leaves
    /// the recognizers exactly as Step 13 shipped them.
    func setPlayMode(_ play: Bool) {
        playModeRequested = play
        applyMode()
    }

    private func applyMode() {
        let playable = pendingLayout == 1 || pendingLayout == 2
        let effective = playModeRequested && playable
        for g in marbleRecognizers { g.isEnabled = !effective }
        overlay.isHidden = !effective
        overlay.isUserInteractionEnabled = effective
        if effective != playEffective {
            playEffective = effective
            if effective {
                // Working rule: entering Play mode pushes MCM/RPN0 into the
                // LOOPBACK before any notes — the normalizer's MPE mode and
                // ±48 range become deterministic, never heuristic.
                sendSessionConfig()
            } else {
                overlay.releaseAllTouches()   // ends any held voices cleanly
            }
        }
    }

    private func sendSessionConfig() {
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var cfg = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 128)
            let n = hostmpe_session_config(mpe, &cfg, 128)
            for i in 0..<Int(n) {
                self.logByte(cfg[i].status, cfg[i].data1, cfg[i].data2, src: 2)
                sumi_push_midi(inst, cfg[i].status, cfg[i].data1, cfg[i].data2)
            }
        }
    }

    // -- Play-mode touch path (overlay -> hostmpe -> loopback) ---------------

    /// Returns the allocated voice (member channel) or -1 on saturation.
    /// Synchronous hop onto the MIDI queue: allocation must answer before the
    /// overlay can track the touch, and the calls are microseconds.
    func playTouchBegin(note: UInt8, velocity: UInt8, rMax: Float,
                        gradX: Float, gradY: Float) -> Int32 {
        markActivity()
        latencyMarks.append(CACurrentMediaTime())
        let state = signposter.beginInterval("touch-to-render")
        defer { signposter.endInterval("touch-to-render", state) }
        var voice: Int32 = -1
        midiQueue.sync { [self] in
            guard let inst, let mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            var n: UInt32 = 0
            voice = hostmpe_touch_begin(mpe, CACurrentMediaTime(), note, velocity,
                                        rMax, gradX, gradY, &m, 4, &n)
            for i in 0..<Int(n) {
                logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
            }
        }
        return voice
    }

    func playTouchUpdate(voice: Int32, dx: Float, dy: Float) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            let n = hostmpe_touch_update(mpe, voice, dx, dy, &m, 4)
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
            }
        }
    }

    func playTouchEnd(voice: Int32, lift: UInt8) {
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            let n = hostmpe_touch_end(mpe, voice, CACurrentMediaTime(), lift, &m, 4)
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
            }
        }
    }

    // midiQueue only.
    private func logByte(_ s: UInt8, _ d1: UInt8, _ d2: UInt8, src: UInt8) {
        if byteLog.count < byteLogCap {
            byteLog.append((CACurrentMediaTime(), s, d1, d2, src))
        }
    }

    private func flushByteLog() {
        midiQueue.async { [weak self] in
            guard let self, !self.byteLog.isEmpty,
                  let dir = FileManager.default.urls(for: .documentDirectory,
                                                     in: .userDomainMask).first else { return }
            var out = "t,status,d1,d2,src\n"
            for e in self.byteLog {
                out += String(format: "%.4f,%d,%d,%d,%d\n", e.t, e.s, e.d1, e.d2, e.src)
            }
            try? out.write(to: dir.appendingPathComponent("midi_log.csv"),
                           atomically: true, encoding: .utf8)
        }
    }

    private func applyParams() {
        guard let inst else { return }
        var p = sumi_params_t()
        sumi_get_params(inst, &p)
        if p.sim_scale != pendingSimScale || p.pitch_layout != pendingLayout {
            p.sim_scale = pendingSimScale
            p.pitch_layout = pendingLayout
            sumi_set_params(inst, &p)
        }
        // The shells own every params write, so this snapshot is the probe's
        // ground truth (PHASE4 §2: instance-free probing off the UI state).
        paramsSnapshot = p
        overlay.layoutParamsChanged()
        applyMode()
    }

    // -- touch gestures (tap = drop, pan = tine, two-finger twist = vortex) --

    private func norm(_ p: CGPoint) -> (Float, Float) {
        (Float(p.x / max(bounds.width, 1)), Float(p.y / max(bounds.height, 1)))
    }
    /// Aspect-corrected length in canvas-height units (desktop segment_len_ac).
    private func lengthAC(_ a: CGPoint, _ b: CGPoint) -> Float {
        let aspect = Float(bounds.width / max(bounds.height, 1))
        let (ax, ay) = norm(a), (bx, by) = norm(b)
        let dx = (bx - ax) * aspect, dy = by - ay
        return (dx * dx + dy * dy).squareRoot()
    }

    @objc private func onTap(_ g: UITapGestureRecognizer) {
        guard let inst, g.state == .ended else { return }
        markActivity()
        let (x, y) = norm(g.location(in: self))
        sumi_add_drop(inst, x, y, DROP_RADIUS, 0)
    }

    @objc private func onPan(_ g: UIPanGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        let p = g.location(in: self)
        switch g.state {
        case .began:
            panLast = p
        case .changed:
            // A two-finger twist in progress owns the touches — no tines.
            if let twist, twist.state == .began || twist.state == .changed { return }
            let dx = p.x - panLast.x, dy = p.y - panLast.y
            if dx * dx + dy * dy >= DRAG_THRESHOLD_PT * DRAG_THRESHOLD_PT {
                let (x0, y0) = norm(panLast)
                let (x1, y1) = norm(p)
                sumi_add_tine(inst, x0, y0, x1, y1, TINE_ALPHA, lengthAC(panLast, p))
                panLast = p
            }
        default:
            break
        }
    }

    func gestureRecognizer(_ g: UIGestureRecognizer,
                           shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer) -> Bool {
        true
    }

    @objc private func onTwist(_ g: UIRotationGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        switch g.state {
        case .began:
            rotLast = g.rotation
        case .changed:
            // Desktop right-drag: strength = aspect-corrected drag speed ×
            // VORTEX_STRENGTH. The twist analog: the rotation delta itself is
            // already radians of intent — scale to the same feel and clamp.
            var strength = Float(g.rotation - rotLast) * (VORTEX_STRENGTH / 4.0)
            strength = max(-0.5, min(0.5, strength))
            rotLast = g.rotation
            let (x, y) = norm(g.location(in: self))
            sumi_add_vortex(inst, x, y, strength, VORTEX_RADIUS)
        default:
            break
        }
    }
}

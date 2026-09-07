// The §5.4 shell proper: a UIViewRepresentable whose backing UIView overrides
// layerClass to CAMetalLayer, hands the layer to sumi_create (backend METAL),
// drives sumi_update/sumi_render from a CADisplayLink, and forwards resizes
// from layoutSubviews with contentScaleFactor as pixel_ratio. Touch gestures
// reuse the ABI gesture calls with the desktop harness's constants.
import SwiftUI
import UIKit
import Metal
import QuartzCore
import CoreMIDI
import os.signpost
import SumiCore
import HostMPE

struct SumiCanvas: UIViewRepresentable {
    var simScale: Float
    var layout: UInt32
    var playMode: Bool
    var velocityFromTouchSize: Bool
    var outVirtual: Bool
    var outNetwork: Bool
    var outBLE: Bool
    var sustainToggle: Bool
    var slidePinch: Bool
    var pinchCrossed: Bool
    var bendRipple: Bool
    var pressSwirl: Bool
    var wakeViscous: Bool
    var wakeSpread: Double
    // Step 33 (#56): the desktop settings window's rows the tablets lacked.
    var palette: Int
    var viscosity: Double
    var inkFeed: Double
    var roughness: Double
    var bpm: Double
    var rollSpeed: Double
    var vortexRankine: Bool
    var rippleAmount: Int
    var rippleWavelength: Int
    var rippleAngle: Double
    var ccMap: String

    func makeUIView(context: Context) -> SumiCanvasView { SumiCanvasView() }
    func updateUIView(_ view: SumiCanvasView, context: Context) {
        view.setSimScale(simScale)
        view.setLayout(layout)
        view.setLook(palette: palette, viscosity: viscosity, inkFeed: inkFeed,
                     roughness: roughness, bpm: bpm, rollSpeed: rollSpeed)
        view.setVortexProfile(rankine: vortexRankine)
        view.setCcMap(CcMap.decode(ccMap))
        view.setRipple(amount: rippleAmount, wavelength: rippleWavelength, angle: rippleAngle)
        view.velocityFromTouchSize = velocityFromTouchSize
        view.setPlayMode(playMode)
        view.setTransports(virtualSrc: outVirtual, network: outNetwork, ble: outBLE)
        view.setSustainToggleMode(sustainToggle)
        view.setSlidePinch(slidePinch, crossed: pinchCrossed)
        view.setBendMode(ripple: bendRipple)
        view.setPressMode(swirl: pressSwirl)
        view.setWakeProfile(viscous: wakeViscous, spread: wakeSpread)
    }
}

/// The CC map as the settings own it (#56) — the desktop's CcRoute mirror:
/// channel 0xFF = any, cc 0..127, target = sumi_ctl_t. Persisted as
/// "ch:cc:target;..." (an empty string means the default map).
struct CcRoute: Hashable, Identifiable {
    var channel: UInt8
    var cc: UInt8
    var target: UInt32
    var id: String { "\(channel):\(cc):\(target)" }
}

enum CcMap {
    static let ctlNames: [(UInt32, String)] = [
        (0, "Vortex strength"), (1, "Vortex center X"), (2, "Vortex center Y"),
        (3, "Viscosity"), (4, "Paper roughness"), (5, "Palette morph"),
        (6, "Ink flow (breath)"), (7, "Ripple amount"), (8, "Ripple wavelength"),
    ]
    static func ctlName(_ t: UInt32) -> String { ctlNames.first { $0.0 == t }?.1 ?? "?" }

    /// desktop/src/app_settings.cpp app_settings_default_routes, verbatim: the
    /// core's install_default_cc_map + the shells' ripple handles 102/103.
    static let defaults: [CcRoute] = [
        CcRoute(channel: 0xFF, cc: 1,  target: 0),   // mod wheel
        CcRoute(channel: 0xFF, cc: 2,  target: 6),   // breath
        CcRoute(channel: 0xFF, cc: 7,  target: 6),   // volume = breath alias
        CcRoute(channel: 0xFF, cc: 11, target: 6),   // expression = breath alias
        CcRoute(channel: 0xFF, cc: 26, target: 0),   // Airwave Raise L (#50)
        CcRoute(channel: 0xFF, cc: 24, target: 1),   // Glide L
        CcRoute(channel: 0xFF, cc: 22, target: 2),   // Slide L
        CcRoute(channel: 0xFF, cc: 29, target: 3),   // Tilt R
        CcRoute(channel: 0xFF, cc: 30, target: 4),   // Flex L
        CcRoute(channel: 0xFF, cc: 31, target: 5),   // Flex R
        CcRoute(channel: 0xFF, cc: 27, target: 7),   // Raise R
        CcRoute(channel: 0xFF, cc: 28, target: 8),   // Tilt L
        CcRoute(channel: 0xFF, cc: 102, target: 7),  // the ripple handles
        CcRoute(channel: 0xFF, cc: 103, target: 8),
    ]

    static func encode(_ routes: [CcRoute]) -> String {
        routes.map { "\($0.channel):\($0.cc):\($0.target)" }.joined(separator: ";")
    }
    static func decode(_ s: String) -> [CcRoute] {
        if s.isEmpty { return defaults }
        var out: [CcRoute] = []
        for part in s.split(separator: ";") {
            let f = part.split(separator: ":")
            guard f.count == 3, let ch = UInt32(f[0]), let cc = UInt8(f[1]), let t = UInt32(f[2]),
                  cc < 128, t < 9, ch == 0xFF || ch < 16 else { continue }
            out.append(CcRoute(channel: UInt8(ch), cc: cc, target: t))
        }
        return out
    }
    static func route(_ routes: [CcRoute], for target: UInt32) -> UInt8? {
        routes.first { $0.target == target }?.cc
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
    // v0.6 pressure gesture (#49) — the same constants on every shell.
    private let PRESS_TRAVEL: Float = 0.15
    private let PRESS_FEED_RATE: Float = 0.12
    private let PRESS_FEED_IDLE: Float = 0.35
    private let PRESS_SWIRL_OMEGA: Float = 3.0
    private struct PressState { var x: Float; var y: Float; var R: Float; var cy0: CGFloat; var cy: CGFloat }
    private var press: PressState?
    private let VORTEX_STRENGTH: Float = 4.0
    private let DRAG_THRESHOLD_PT: CGFloat = 5.0
    private var panLast = CGPoint.zero
    private var rotLast: CGFloat = 0
    private var twist: UIRotationGestureRecognizer?
    private var pendingLayout: UInt32 = 0
    private var pendingSlideMode: UInt32 = 0     // v0.4: 0 hue/aux, 1 pinch
    private var pendingPinchVariant: UInt32 = 0  // v0.4: 0 saddle, 1 crossed
    private var pendingBendMode: UInt32 = 0      // v0.4: 0 glide, 1 ripple
    private var pendingRippleBake: UInt32 = 0    // rides bend_mode (#36)
    private var pendingPressMode: UInt32 = 0     // v0.4 step 20: 0 feed, 1 swirl
    private var pendingWakeProfile: UInt32 = 0   // v0.7 (#53): 0 doublet, 1 viscous Stokeslet
    private var pendingWakeSpread: Float = 3.0   // v0.7: l/a
    // #56: the look and the remaining routing rows (core defaults, engine.cpp).
    private var pendingPalette: UInt32 = 0
    private var pendingViscosity: Float = 0.5
    private var pendingInkFeed: Float = 1.0
    private var pendingRoughness: Float = 0.5
    private var pendingBpm: Float = 120.0
    private var pendingRollSpeed: Float = 0.0625
    private var pendingVortexProfile: UInt32 = 0  // 0 exponential, 1 Rankine
    private var pendingRippleAngle: Float = 0.0
    private var ccRoutes: [CcRoute] = CcMap.defaults
    private var ccRoutesApplied: [CcRoute]? = nil
    private var rippleSent: (Int, Int)? = nil      // last (amount, wavelength) pushed
    private var pendingRipple: (Int, Int) = (0, 32)
    private var marbleRecognizers: [UIGestureRecognizer] = []
    private let overlay = PlayOverlayView()
    private var playModeRequested = false
    private var playEffective = false
    private(set) var paramsSnapshot = sumi_params_t()

    // Step 18 (§8 rev, DECISIONS_3 #31): the performance control strip — a
    // compact floating palette over the full-canvas lattice. The widget VALUE
    // engine (hostmpe_strip_t) lives on the midiQueue and survives mode and
    // layout switches — values persist by construction.
    private let strip = ControlStripView()
    private var stripEngine: OpaquePointer?   // hostmpe_strip_t*, midiQueue only
    private var sustainToggleMode = false

    // -- Phase 4 §5.2: the serial MIDI queue is the SOLE producer -----------
    // Every byte — CoreMIDI devices AND touch-generated — crosses
    // sumi_push_midi only from this queue; hostmpe state lives on it too.
    private let midiQueue = DispatchQueue(label: "com.vibetuned.midi-sink.midi")
    private var mpe: OpaquePointer?   // hostmpe_t*, touched only on midiQueue
    private var outputs: MidiOutputs? // Step 17 transports, touched only on midiQueue
    var velocityFromTouchSize = false

    // Storm test (Step 17 BLE saturation DONE): 10 synthetic voices, 60 s.
    private var stormTimer: DispatchSourceTimer?
    private(set) var stormRunning = false
    private var pendingTransports: (Bool, Bool, Bool) = (true, false, false)
    private var lastAutoResync: CFTimeInterval = 0

    // Byte log at the merge point (Step 16 evidence: emit-order and
    // channel-steal asserts). Appended on midiQueue only; flushed with the
    // session log. Taxonomy shared with Android and tools/midi_asserts.py +
    // tools/pen_trace.py: 0 = external device, 1 = finger, 2 = session
    // config, 3 = control strip, 4 = stylus (#61).
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
    /// Step 33 (#55): the settings' "MIDI inputs" section — names of the
    /// connected CoreMIDI sources and the live receive counters.
    func midiInputs() -> MidiSource.Snapshot? { midi?.snapshot() }
    func midiRescanNow() { midi?.rescanNow() }
    private var echoDroppedSnapshot: UInt32 = 0   // #66 diagnostics

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
        // #41: the v0.4 pinch finally gets its Marble gesture on iOS — a
        // literal two-finger pinch: the fold axis IS the line between the
        // fingers, the squeeze is the (delta-driven) strength.
        let pinch = UIPinchGestureRecognizer(target: self, action: #selector(onPinch))
        // v0.6 (#49): the pressure gesture — a long press lays a drop and
        // becomes Play mode's bipolar Y: hold / push up = feed, pull back = swirl.
        let press = UILongPressGestureRecognizer(target: self, action: #selector(onPress))
        press.minimumPressDuration = 0.25
        press.allowableMovement = 8
        for g in [tap, pan, rot, pinch, press] as [UIGestureRecognizer] {
            g.delegate = self
            // Step 33 (#54): fingers only — the Pencil in Marble mode draws its
            // wake through touchesBegan/Moved below (spec §8.7: both modes),
            // never a tine, a drop or a pressure long press.
            g.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.direct.rawValue)]
            addGestureRecognizer(g)
        }
        twist = rot
        marbleRecognizers = [tap, pan, rot, pinch, press]
        // Play-mode overlay (Phase 4 §6): hidden and interaction-inert in
        // Marble mode, so the marble gesture path stays bit-identical.
        overlay.paramsProvider = { [weak self] in self?.paramsSnapshot ?? sumi_params_t() }
        overlay.host = self
        overlay.isHidden = true
        overlay.isUserInteractionEnabled = false
        addSubview(overlay)
        strip.host = self
        strip.isHidden = true
        addSubview(strip)
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
        layoutPlaySurface()
        if inst == nil {
            start(width: w, height: h)
        } else {
            sumi_resize(inst, w, h, Float(contentScaleFactor))
        }
    }

    /// §8 rev (DECISIONS_3 #31): the strip is a COMPACT FLOATING PALETTE at
    /// the top-left, over the full-canvas lattice — the overlay always keeps
    /// the full bounds, so a touched cell and its loopback drop stay exactly
    /// aligned (the displacement variant broke drop-under-finger on device).
    /// The palette consumes its own touches; it hides only the corner cells
    /// beneath it.
    private func layoutPlaySurface() {
        overlay.frame = bounds
        let w: CGFloat = min(300, bounds.width * 0.4)
        strip.frame = CGRect(x: 10, y: safeAreaInsets.top + 10, width: w, height: 86)
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
        var excluded = Set<MIDIUniqueID>()
        midiQueue.sync {
            mpe = hostmpe_create()
            stripEngine = hostmpe_strip_create()
            let o = MidiOutputs()
            o.primeSinkSignature()
            // §5.4: a sink coming up mid-session (USB/IDAM enabled in Audio
            // MIDI Setup, a BLE central connecting) must get the MCM/RPN0
            // handshake it missed. Debounced — one setup change can fan out
            // several notifications.
            o.onSinkAppeared = { [weak self] in
                guard let self, self.playEffective else { return }
                let now = CACurrentMediaTime()
                guard now - self.lastAutoResync > 2.0 else { return }
                self.lastAutoResync = now
                self.sendSessionConfig()
            }
            o.onDelivered = { [weak self] msg in
                // #66: on the MIDI queue already (emit is called from it).
                guard let mpe = self?.mpe else { return }
                hostmpe_echo_record(mpe, CACurrentMediaTime(),
                                    msg.status, msg.data1, msg.data2)
            }
            outputs = o
            excluded = o.ownUniqueIDs
        }
        applyParams()
        // v0.4 ripple ctls (#32/#35): CC 102/103 are the shell's local
        // handles for amplitude/wavelength (unused by anything else; the
        // default map ships the dims unmapped). The settings slider and the
        // strip's assignable wheels ride them.
        // #56: the settings' CC map (defaults = the core's map + 102/103).
        applyCcMap()
        midi = MidiSource { [weak self] status, d1, d2 in
            // CoreMIDI thread -> hop to the serial MIDI queue: the SOLE
            // producer (§5.2). The merge point also feeds hostmpe's
            // external-occupancy mask (§5.1) and the byte log.
            guard let self else { return }
            self.midiQueue.async {
                guard let inst = self.inst else { return }
                // #66: a transport mirroring our own output back must not be
                // treated as a device — it would mark OUR channels externally
                // held (starving the allocator) and paint every note twice.
                if let mpe = self.mpe,
                   hostmpe_echo_is_ours(mpe, CACurrentMediaTime(), status, d1, d2) {
                    return
                }
                if let mpe = self.mpe {
                    hostmpe_observe_external(mpe, CACurrentMediaTime(), status, d1, d2)
                }
                self.logByte(status, d1, d2, src: 0)
                sumi_push_midi(inst, status, d1, d2)
            }
            self.markActivity()
        }
        applyTransports()   // sinks now exist: apply whatever SwiftUI set
        midi?.excludedUniqueIDs = excluded
        midi?.onSourcesRemoved = { [weak self] in
            guard let self else { return }
            self.midiQueue.async {
                if let mpe = self.mpe { hostmpe_external_clear(mpe) }
            }
        }
        rippleSent = nil
        sendRipple()   // the persisted sliders, through the routed CCs (#56)
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
        stormTimer?.cancel()
        stormTimer = nil
        midiQueue.sync {
            if let mpe { hostmpe_destroy(mpe) }
            mpe = nil
            if let stripEngine { hostmpe_strip_destroy(stripEngine) }
            stripEngine = nil
            outputs = nil
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
        overlay.penGestureTick(dt: dt)   // #40: barrel gestures decay (τ 0.4 s)
        pressureTick(dt: dt)             // #49: the Marble-mode long press
        sumi_update(inst, dt)
        sumi_render(inst)
        servicePrintSave()               // #51: a pending "save the print"
        let frameMs = (CACurrentMediaTime() - t0) * 1000.0

        // Step 17: surface limiter-held outbound messages once per frame.
        // Step 18: the same cadence drives the strip's spring return ramp.
        midiQueue.async { [weak self] in
            guard let self else { return }
            let qnow = CACurrentMediaTime()
            self.outputs?.drain(now: qnow)
            if let se = self.stripEngine {
                var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
                let n = hostmpe_strip_tick(se, qnow, &m, 2)
                self.stripDispatch(m, count: n, exempt: false)   // wheel: policed
            }
        }

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
            let out = outputs.map { "  out v\($0.sentVirtual)/n\($0.sentNetwork)/b\($0.sentBLE)" } ?? ""
            let echoes = echoDroppedSnapshot > 0 ? "  echo \(echoDroppedSnapshot)" : ""
            statusLine = String(format: "t=%.0fs  %.1f fps  worst %.2f ms  thermal %@  dropped %u%@%@%@",
                                now - sessionStart, fps, worstFrameMs, thermal, dropped, lat, out, echoes)
            midiQueue.async { [weak self] in
                guard let self, let mpe = self.mpe else { return }
                let n = hostmpe_echo_dropped(mpe)
                DispatchQueue.main.async { self.echoDroppedSnapshot = n }
            }
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

    /// v0.4 (§4.3(5), DECISIONS_3 #34): CC74 routing (hue vs pinch) + which
    /// pinch look — a core params choice so the MIDI route honors it too.
    func setSlidePinch(_ pinch: Bool, crossed: Bool) {
        pendingSlideMode = pinch ? 1 : 0
        pendingPinchVariant = crossed ? 1 : 0
        applyParams()
    }

    /// v0.4 bend_mode (§4.3(6), #35 corrected): 0 = v1 glide (a note's bend
    /// drags its drop), 1 = the note bend raises the sine ripple — the
    /// shimmer's depth is the bend's distance from center (like glide), so
    /// vibrato breathes the water and it stills on re-center/release. The
    /// mod wheel / vortex routing is untouched.
    func setBendMode(ripple: Bool) {
        pendingBendMode = ripple ? 1 : 0
        // #36: ripple vibrato is PERMANENT like glide — the bake insertion
        // point, where the bend-driven phase drift feathers residue in.
        pendingRippleBake = ripple ? 1 : 0
        applyParams()
    }

    /// v0.4 press_mode (§3.4, step 20): 0xD0 hardware routing — feed (v1
    /// grow) or the Lamb–Oseen swirl. The surface's own down-pull emits 0xA0,
    /// which swirls in EITHER mode.
    func setPressMode(swirl: Bool) {
        pendingPressMode = swirl ? 1 : 0
        applyParams()
    }

    /// v0.7 (#53): the stylus wake's fluid — the inviscid doublet or the
    /// viscous 2-D Stokeslet stroke, with its spread l/a.
    func setWakeProfile(viscous: Bool, spread: Double) {
        pendingWakeProfile = viscous ? 1 : 0
        pendingWakeSpread = Float(min(12.0, max(1.5, spread)))
        applyParams()
    }

    /// #56: the desktop's "Layout & look" rows — palette, viscosity, ink feed,
    /// paper roughness, and the roll layouts' tempo and roll speed.
    func setLook(palette: Int, viscosity: Double, inkFeed: Double, roughness: Double,
                 bpm: Double, rollSpeed: Double) {
        pendingPalette = UInt32(min(2, max(0, palette)))
        pendingViscosity = Float(min(1.0, max(0.0, viscosity)))
        pendingInkFeed = Float(min(4.0, max(0.1, inkFeed)))
        pendingRoughness = Float(min(1.0, max(0.0, roughness)))
        pendingBpm = Float(min(300.0, max(20.0, bpm)))
        pendingRollSpeed = Float(min(0.25, max(0.02, rollSpeed)))
        applyParams()
    }

    /// #56: the CC-routed vortex's profile — and, as on desktop, the profile
    /// the two-finger twist stirs with.
    func setVortexProfile(rankine: Bool) {
        pendingVortexProfile = rankine ? 1 : 0
        applyParams()
    }

    /// #56: the ripple rows. Amount and wavelength travel as the routed CCs
    /// (102/103 by default) through the MIDI path, exactly like the desktop's
    /// sliders; the angle is a params field.
    func setRipple(amount: Int, wavelength: Int, angle: Double) {
        pendingRippleAngle = Float(min(180.0, max(0.0, angle))) / 57.29578
        pendingRipple = (min(127, max(0, amount)), min(127, max(0, wavelength)))
        applyParams()
        sendRipple()
    }

    private func sendRipple() {
        guard inst != nil else { return }
        if let sent = rippleSent, sent == pendingRipple { return }
        let ampCC = CcMap.route(ccRoutes, for: 7)
        let frqCC = CcMap.route(ccRoutes, for: 8)
        rippleSent = pendingRipple
        let (amp, frq) = pendingRipple
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst else { return }
            if let cc = ampCC {
                self.logByte(0xB0, cc, UInt8(amp), src: 2)
                sumi_push_midi(inst, 0xB0, cc, UInt8(amp))
            }
            if let cc = frqCC {
                self.logByte(0xB0, cc, UInt8(frq), src: 2)
                sumi_push_midi(inst, 0xB0, cc, UInt8(frq))
            }
        }
    }

    /// #56: the CC map editor's routes. Configuration is render-thread only;
    /// on iOS that is the main thread the display link runs on.
    func setCcMap(_ routes: [CcRoute]) {
        ccRoutes = routes
        applyCcMap()
    }

    private func applyCcMap() {
        guard let inst, ccRoutesApplied != ccRoutes else { return }
        sumi_clear_cc_map(inst)
        for r in ccRoutes {
            sumi_map_cc(inst, r.channel, r.cc, sumi_ctl_t(rawValue: r.target))
        }
        ccRoutesApplied = ccRoutes
        rippleSent = nil   // the handles may have moved to other CCs
        sendRipple()
    }


    /// Play mode (Phase 4): effective only on the playable layouts (grid,
    /// Jankó, piano grid — the probe refuses everything else anyway); Marble
    /// mode leaves the recognizers exactly as Step 13 shipped them.
    func setPlayMode(_ play: Bool) {
        playModeRequested = play
        applyMode()
    }

    private func applyMode() {
        let playable = pendingLayout == 1 || pendingLayout == 2 || pendingLayout == 5
        let effective = playModeRequested && playable
        for g in marbleRecognizers { g.isEnabled = !effective }
        overlay.isHidden = !effective
        overlay.isUserInteractionEnabled = effective
        NSLog("[mode] requested=%d layout=%u playable=%d effective=%d (was %d)",
              playModeRequested ? 1 : 0, pendingLayout, playable ? 1 : 0,
              effective ? 1 : 0, playEffective ? 1 : 0)
        strip.isHidden = !effective
        if effective != playEffective {
            playEffective = effective
            setNeedsLayout()   // §8: the strip displaces / releases the lattice
            if effective {
                // Working rule: entering Play mode pushes MCM/RPN0 into the
                // LOOPBACK before any notes — the normalizer's MPE mode and
                // ±48 range become deterministic, never heuristic.
                sendSessionConfig()
                syncStripMirrors()   // values persisted across the mode switch
            } else {
                overlay.releaseAllTouches()   // ends any held voices cleanly
            }
        }
    }

    private func sendSessionConfig() {
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else {
                NSLog("[cfg] sendSessionConfig SKIPPED (inst/mpe nil)")
                return
            }
            NSLog("[cfg] sending session config, outputs=%@",
                  self.outputs == nil ? "NIL" : "ok")
            self.outputs?.logDestinations()   // census: what sinks exist now
            var cfg = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 128)
            let n = hostmpe_session_config(mpe, &cfg, 128)
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                self.logByte(cfg[i].status, cfg[i].data1, cfg[i].data2, src: 2)
                sumi_push_midi(inst, cfg[i].status, cfg[i].data1, cfg[i].data2)
                // Byte order is preserved on every transport (verified on the
                // wire for USB/IDAM, rtpMIDI and BLE — DECISIONS_3 #22), so
                // the RPN select always precedes the data entry and the DAW
                // gets its ±48 range.
                self.outputs?.send(cfg[i], exempt: true, now: now)
            }
            // §8: the strip re-announces its latched values after every MCM
            // re-sync so a DAW and the strip never disagree. Exempt — an
            // announce repeats values by definition; change-only would eat it.
            if let se = self.stripEngine {
                var am = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 8)
                let an = hostmpe_strip_announce(se, &am, 8)
                self.stripDispatch(am, count: an, exempt: true)
            }
        }
    }

    // -- Step 18: control strip path (strip UI -> value engines -> both pipes) --

    /// midiQueue only. Loopback full-rate + outbound under each transport's
    /// policy with the message's §8 class (buttons exempt, wheels policed).
    private func stripDispatch(_ m: [hostmpe_msg_t], count: UInt32, exempt: Bool) {
        guard count > 0, let inst else { return }
        let now = CACurrentMediaTime()
        for i in 0..<Int(count) {
            logByte(m[i].status, m[i].data1, m[i].data2, src: 3)
            sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
            outputs?.send(m[i], exempt: exempt, now: now)
        }
    }

    func setSustainToggleMode(_ toggle: Bool) {
        guard sustainToggleMode != toggle else { return }
        sustainToggleMode = toggle
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            // A mode switch while sustain is ON emits the OFF (never a
            // stranded pedal) — button class, never dropped.
            let n = hostmpe_strip_sustain_mode(se, toggle, &m, 2)
            self.stripDispatch(m, count: n, exempt: true)
        }
        syncStripMirrors()
    }

    func stripPitchMove(_ v: Float) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_pitch_move(se, v, &m, 2)
            self.stripDispatch(m, count: n, exempt: false)
        }
    }

    func stripPitchRelease() {
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            hostmpe_strip_pitch_release(se, CACurrentMediaTime())
            // Ramp messages surface from hostmpe_strip_tick on the frame drain.
        }
    }

    func stripLatchMove(wheel: Int32, delta: Float) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_latch_move(se, wheel, delta, &m, 2)
            self.stripDispatch(m, count: n, exempt: false)
        }
    }

    func stripSustainDown() {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_sustain_press(se, &m, 2)
            self.stripDispatch(m, count: n, exempt: true)   // never-dropped class
        }
    }

    func stripSustainUp() {
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_sustain_release(se, &m, 2)
            self.stripDispatch(m, count: n, exempt: true)   // never-dropped class
        }
    }

    func stripAssign(wheel: Int32, cc: UInt8, completion: @escaping (Bool, UInt8) -> Void) {
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            let ok = hostmpe_strip_assign(se, wheel, cc)
            completion(ok, hostmpe_strip_assigned_cc(se, wheel))
        }
    }

    /// Pull the engine's latched state to the strip's display mirrors (mode
    /// re-entry, sustain-mode changes — values persist in the engine).
    func syncStripMirrors() {
        let toggle = sustainToggleMode
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            let pitch = hostmpe_strip_pitch_value(se)
            let latch = [hostmpe_strip_latch_value(se, 0),
                         hostmpe_strip_latch_value(se, 1),
                         hostmpe_strip_latch_value(se, 2)]
            let sus = hostmpe_strip_sustain_on(se)
            let ccs = [hostmpe_strip_assigned_cc(se, 0),
                       hostmpe_strip_assigned_cc(se, 1),
                       hostmpe_strip_assigned_cc(se, 2)]
            DispatchQueue.main.async {
                self.strip.syncMirrors(pitch: pitch, latch: latch, sustain: sus,
                                       toggleMode: toggle, ccs: ccs)
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
            let now = CACurrentMediaTime()
            voice = hostmpe_touch_begin(mpe, now, note, velocity,
                                        rMax, gradX, gradY, &m, 4, &n)
            for i in 0..<Int(n) {
                logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                outputs?.send(m[i], exempt: true, now: now)   // strike: never decimated
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
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: false, now: now)   // continuous: policed
            }
        }
    }

    func playTouchEnd(voice: Int32, lift: UInt8, isPen: Bool = false) {
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            let now = CACurrentMediaTime()
            let n = hostmpe_touch_end(mpe, voice, now, lift, &m, 4)
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: isPen ? 4 : 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: true, now: now)   // lift: never decimated
            }
        }
    }

    // -- Evidence capture (step-21 DONE artifacts) ---------------------------

    /// Full-screen snapshots into Documents (`capture_NN.png`), starting
    /// `delay` seconds from now so the sheet can be dismissed and the
    /// instrument played. Pull them with
    /// `devicectl device copy from --domain-type appDataContainer`.
    func startCaptureBurst(delay: Double = 3.0, frames: Int = 6, interval: Double = 1.0) {
        NSLog("[capture] burst armed: %d frames, %.1f s from now", frames, delay)
        for i in 0..<frames {
            DispatchQueue.main.asyncAfter(deadline: .now() + delay + interval * Double(i)) {
                [weak self] in self?.snapshot(index: i + 1)
            }
        }
    }

    private func snapshot(index: Int) {
        guard let window else { return }
        let renderer = UIGraphicsImageRenderer(bounds: window.bounds)
        let img = renderer.image { _ in
            // afterScreenUpdates goes through the render server, which is the
            // only path that can capture the CAMetalLayer's content.
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        guard let data = img.pngData(),
              let dir = FileManager.default.urls(for: .documentDirectory,
                                                 in: .userDomainMask).first else { return }
        let url = dir.appendingPathComponent(String(format: "capture_%02d.png", index))
        try? data.write(to: url)
        NSLog("[capture] wrote %@ (%d bytes)", url.lastPathComponent, data.count)
    }

    /// Write the byte/session/latency logs now (they otherwise flush on
    /// backgrounding), so they can be pulled mid-session.
    func flushLogsNow() {
        flushSessionLog()
        NSLog("[capture] logs flushed to Documents")
    }

    // -- Step 21: stylus path (§7 — pen -> hostmpe legato engine + gesture ABI) --

    /// Pen-down: like a strike (exempt class). Synchronous for the voice id.
    func penBegin(note: UInt8, velocity: UInt8) -> Int32 {
        markActivity()
        latencyMarks.append(CACurrentMediaTime())
        var voice: Int32 = -1
        midiQueue.sync { [self] in
            guard let inst, let mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            var n: UInt32 = 0
            voice = hostmpe_pen_begin(mpe, CACurrentMediaTime(), note, velocity, &m, 4, &n)
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                logByte(m[i].status, m[i].data1, m[i].data2, src: 4)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                outputs?.send(m[i], exempt: true, now: now)
            }
        }
        return voice
    }

    /// Legato glissando (#39): a batch containing a retrigger (Note On) goes
    /// out WHOLE as strike class — the bend→On→Off crossing must arrive
    /// intact on every transport; a bend-only batch is a policed continuous
    /// dimension.
    func penGlide(voice: Int32, note: UInt8, offset: Float, scale: Float, velocity: UInt8) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            let n = hostmpe_pen_glide(mpe, voice, note, offset, scale, velocity, &m, 4)
            var hasOn = false
            for i in 0..<Int(n) where (m[i].status & 0xF0) == 0x90 { hasOn = true }
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 4)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: hasOn, now: now)
            }
        }
    }

    /// §3.3 stylus CC74. With slide_mode = 1 the loopback is SKIPPED — the
    /// shell drives the azimuth pinch through the gesture ABI instead, and a
    /// second (mapper) pinch from the same CC74 would double it. Outbound
    /// still records the dimension (a DAW replay pinches via the mapper's
    /// CC74 route, pitch-axis fold — DECISIONS_3 #38).
    func penSlide(voice: Int32, eff: Float, outboundOnly: Bool) {
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_pen_slide(mpe, voice, eff, &m, 2)
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 4)
                if !outboundOnly {
                    sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                }
                self.outputs?.send(m[i], exempt: false, now: now)
            }
        }
    }

    func penPressure(voice: Int32, force: Float) {
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_pen_pressure(mpe, voice, force, &m, 2)
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 4)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: false, now: now)
            }
        }
    }

    /// Gesture-ABI passes — main thread IS the render thread on iOS (§5.2).
    func addWake(x0: Float, y0: Float, x1: Float, y1: Float, tip: Float) {
        guard let inst else { return }
        sumi_add_wake(inst, x0, y0, x1, y1, tip)
    }

    func penPinch(x: Float, y: Float, k: Float, angle: Float) {
        guard let inst else { return }
        sumi_add_pinch(inst, x, y, k, angle)
    }

    // -- Step 17: transports control ------------------------------------------

    func setTransports(virtualSrc: Bool, network: Bool, ble: Bool) {
        // SwiftUI can apply these BEFORE the view has created its outputs
        // (updateUIView before layoutSubviews/start): remember them and
        // re-apply once the sinks exist, or the flags are silently lost.
        pendingTransports = (virtualSrc, network, ble)
        applyTransports()
    }

    private func applyTransports() {
        let want = pendingTransports
        midiQueue.async { [weak self] in
            guard let self, let outputs = self.outputs else { return }
            // A transport being switched OFF gets a zone silence first (CC64
            // + CC123 on master and every member): otherwise a synth on that
            // sink holds whatever was sounding, forever. Voices sounding on
            // the OTHER pipes are untouched (stateless silence, no voice
            // release) — DECISIONS_3 #26.
            let losing = (outputs.virtualEnabled && !want.0,
                          outputs.networkEnabled && !want.1,
                          outputs.bleEnabled && !want.2)
            if losing.0 || losing.1 || losing.2 {
                var z = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 64)
                let zn = hostmpe_silence_zone(&z, 64)
                outputs.sendSilence(z, count: zn, toVirtual: losing.0,
                                    toNetwork: losing.1, toBLE: losing.2)
                NSLog("[out] silenced departing sinks v=%d n=%d b=%d",
                      losing.0 ? 1 : 0, losing.1 ? 1 : 0, losing.2 ? 1 : 0)
            }
            outputs.virtualEnabled = want.0
            outputs.setNetworkEnabled(want.1)
            outputs.bleEnabled = want.2
            NSLog("[out] transports: virtual=%d network=%d ble=%d",
                  want.0 ? 1 : 0, want.1 ? 1 : 0, want.2 ? 1 : 0)
            if want.2 { outputs.logDestinations() }   // BLE on: show the links
        }
    }

    /// #67: the paper dip is now DELIBERATE on the tablet — a sustain press
    /// no longer wipes the canvas, so the fresh-sheet action needs its own
    /// control (§5.3 print pipeline unchanged).
    func triggerPaperDip() { paperDip(savePrint: false) }

    /// Paper dip from the settings sheet (#48/#51). The core keeps two print
    /// buffers and recycles the older unread one, so a discard needs no read;
    /// a save waits for the async readback (§5.3) on the display link and
    /// writes the RGBA8 print to the Photos library.
    private var printSaveFrames = -1     // -1 idle; else frames waited
    func paperDip(savePrint: Bool) {
        guard let inst else { return }
        sumi_trigger_paper_dip(inst)
        printSaveFrames = savePrint ? 0 : -1
        NSLog("[dip] paper dip triggered from settings (save: %d)", savePrint ? 1 : 0)
    }

    /// Display-link service: when the save is pending and the print has
    /// landed, copy it out (which frees the core buffer) and hand it to Photos.
    private func servicePrintSave() {
        guard printSaveFrames >= 0, let inst else { return }
        var w: UInt32 = 0, h: UInt32 = 0
        if !sumi_read_print(inst, nil, 0, &w, &h) {
            printSaveFrames += 1
            if printSaveFrames > 180 {           // ~3 s: refused or lost
                NSLog("[dip] print never became readable — not saved")
                printSaveFrames = -1
            }
            return
        }
        printSaveFrames = -1
        let bytes = Int(w) * Int(h) * 4
        var px = [UInt8](repeating: 0, count: bytes)
        let ok = px.withUnsafeMutableBufferPointer { buf in
            sumi_read_print(inst, buf.baseAddress, bytes, &w, &h)
        }
        guard ok, let provider = CGDataProvider(data: Data(px) as CFData),
              let cg = CGImage(width: Int(w), height: Int(h), bitsPerComponent: 8, bitsPerPixel: 32,
                               bytesPerRow: Int(w) * 4, space: CGColorSpaceCreateDeviceRGB(),
                               bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue),
                               provider: provider, decode: nil, shouldInterpolate: false,
                               intent: .defaultIntent) else {
            NSLog("[dip] print readback failed (%ux%u)", w, h)
            return
        }
        UIImageWriteToSavedPhotosAlbum(UIImage(cgImage: cg), nil, nil, nil)
        NSLog("[dip] print %ux%u saved to Photos", w, h)
    }

    /// "Re-sync DAW": resend MCM/RPN0 everywhere (loopback tolerates it).
    func resyncTransports() { sendSessionConfig() }

    /// MIDI panic: release every held voice and silence the zone on the
    /// loopback AND every transport. Note: a BLE MIDI *peripheral* cannot
    /// force a connected central to disconnect (no public API — the central
    /// owns the link), so this is the meaningful "stop": nothing more is
    /// streamed and nothing is left hanging. Dropping the link itself is
    /// done from the connected device's Bluetooth settings.
    func panicAllNotes() {
        overlay.releaseAllTouches()
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 128)
            let n = hostmpe_panic(mpe, CACurrentMediaTime(), &m, 128)
            let now = CACurrentMediaTime()
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: true, now: now)   // never decimated
            }
            NSLog("[panic] released all voices, %d messages", Int(n))
        }
    }

    /// 60 s / 10-voice synthetic storm through the FULL pipeline (loopback +
    /// outbound limiters) for the BLE saturation DONE test. A 1 Hz exempt
    /// marker (CC 118 on the master, counting) rides along so the receiver
    /// can measure cumulative lag without clock sync.
    func startStormTest(seconds: Double = 60.0) {
        guard !stormRunning else { return }
        stormRunning = true
        let timer = DispatchSource.makeTimerSource(queue: midiQueue)
        var tick = 0
        var voices = [Int32](repeating: -1, count: 10)
        let t0 = CACurrentMediaTime()
        timer.schedule(deadline: .now(), repeating: .milliseconds(8))   // 125 Hz
        timer.setEventHandler { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            let now = CACurrentMediaTime()
            let t = now - t0
            if t >= seconds {
                for v in voices where v >= 0 {
                    var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
                    let n = hostmpe_touch_end(mpe, v, now, 64, &m, 4)
                    for i in 0..<Int(n) {
                        sumi_push_midi(inst, m[i].status, m[i].data1, m[i].data2)
                        self.outputs?.send(m[i], exempt: true, now: now)
                    }
                }
                self.stormTimer?.cancel()
                self.stormTimer = nil
                self.stormRunning = false
                // Sender-side truth for the budget assertion: comparing this
                // with the receiver's count separates "we sent too much" from
                // "the link duplicated on delivery".
                if let o = self.outputs {
                    NSLog("[storm] done: sent virtual=%d network=%d ble=%d over %.1fs (ble %.0f/s)",
                          o.sentVirtual, o.sentNetwork, o.sentBLE, t, Double(o.sentBLE) / max(t, 1))
                }
                return
            }
            // (Re)strike each voice every 6 s, staggered.
            for i in 0..<10 {
                let phase = t + Double(i) * 0.6
                if voices[i] < 0 || (tick % 750 == i * 75) {
                    if voices[i] >= 0 {
                        var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
                        let n = hostmpe_touch_end(mpe, voices[i], now, 64, &m, 4)
                        for k in 0..<Int(n) {
                            sumi_push_midi(inst, m[k].status, m[k].data1, m[k].data2)
                            self.outputs?.send(m[k], exempt: true, now: now)
                        }
                    }
                    var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
                    var n: UInt32 = 0
                    voices[i] = hostmpe_touch_begin(mpe, now, UInt8(48 + i * 3), 96,
                                                    0.0571, 1.0 / 0.1244, 0.0, &m, 4, &n)
                    for k in 0..<Int(n) {
                        sumi_push_midi(inst, m[k].status, m[k].data1, m[k].data2)
                        self.outputs?.send(m[k], exempt: true, now: now)
                    }
                }
                guard voices[i] >= 0 else { continue }
                // Expressive wiggle: bend sweep + upward-pressure oscillation.
                let dx = Float(0.12 * sin(phase * 2.1))
                let dy = Float(-0.05 * (0.5 + 0.5 * sin(phase * 3.3)))
                var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
                let n = hostmpe_touch_update(mpe, voices[i], dx, dy, &m, 4)
                for k in 0..<Int(n) {
                    sumi_push_midi(inst, m[k].status, m[k].data1, m[k].data2)
                    self.outputs?.send(m[k], exempt: false, now: now)
                }
            }
            // 1 Hz lag marker, exempt, on the master channel.
            if tick % 125 == 0 {
                var mk = hostmpe_msg_t()
                mk.status = 0xB0
                mk.data1 = 118
                mk.data2 = UInt8((tick / 125) % 128)
                self.outputs?.send(mk, exempt: true, now: now)
            }
            // Step 18 DONE rider: a CC64 transition every 2 s DURING the
            // storm, exempt (§8 never-dropped class) — the receiver capture
            // asserts every transition arrives, in order, undelayed.
            if tick % 250 == 125 {
                var su = hostmpe_msg_t()
                su.status = 0xB0
                su.data1 = 64
                su.data2 = (tick / 250) % 2 == 0 ? 127 : 0
                self.logByte(su.status, su.data1, su.data2, src: 3)
                self.outputs?.send(su, exempt: true, now: now)
            }
            tick += 1
        }
        stormTimer = timer
        timer.resume()
        NSLog("[storm] started: 10 voices, %.0f s", seconds)
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
        if p.sim_scale != pendingSimScale || p.pitch_layout != pendingLayout ||
           p.slide_mode != pendingSlideMode || p.pinch_variant != pendingPinchVariant ||
           p.bend_mode != pendingBendMode || p.ripple_bake != pendingRippleBake ||
           p.press_mode != pendingPressMode ||
           p.wake_profile != pendingWakeProfile || p.wake_spread != pendingWakeSpread ||
           p.active_palette_id != pendingPalette || p.fluid_viscosity != pendingViscosity ||
           p.expansion_rate != pendingInkFeed || p.paper_roughness != pendingRoughness ||
           p.bpm != pendingBpm || p.roll_speed != pendingRollSpeed ||
           p.vortex_profile != pendingVortexProfile || p.ripple_angle != pendingRippleAngle {
            p.sim_scale = pendingSimScale
            p.pitch_layout = pendingLayout
            p.slide_mode = pendingSlideMode
            p.pinch_variant = pendingPinchVariant
            p.bend_mode = pendingBendMode
            p.ripple_bake = pendingRippleBake
            p.press_mode = pendingPressMode
            p.wake_profile = pendingWakeProfile
            p.wake_spread = pendingWakeSpread
            p.active_palette_id = pendingPalette
            p.fluid_viscosity = pendingViscosity
            p.expansion_rate = pendingInkFeed
            p.paper_roughness = pendingRoughness
            p.bpm = pendingBpm
            p.roll_speed = pendingRollSpeed
            p.vortex_profile = pendingVortexProfile
            p.ripple_angle = pendingRippleAngle
            sumi_set_params(inst, &p)
        }

        // The shells own every params write, so this snapshot is the probe's
        // ground truth (PROJECT_SPEC.md §8.2: instance-free probing off the UI state).
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

    // -- Step 33 (#54): the Pencil in MARBLE mode = the dipolar wake ----------
    // In Play mode the overlay (a subview covering the canvas) receives the
    // pencil; here it is hidden, so the touches arrive at the canvas view.
    private var marblePens: [ObjectIdentifier: CGPoint] = [:]
    private func marblePenTip(_ t: UITouch) -> Float {
        // the overlay's mapping: tip radius from real force, normalized
        let force = t.maximumPossibleForce > 0 ? Float(t.force / t.maximumPossibleForce) : 0.3
        return 0.006 + 0.030 * force
    }
    override func touchesBegan(_ ts: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(ts, with: event)
        guard overlay.isHidden else { return }
        for t in ts where t.type == .pencil { marblePens[ObjectIdentifier(t)] = t.location(in: self) }
    }
    override func touchesMoved(_ ts: Set<UITouch>, with event: UIEvent?) {
        super.touchesMoved(ts, with: event)
        guard let inst, overlay.isHidden, bounds.width > 0, bounds.height > 0 else { return }
        for t in ts where t.type == .pencil {
            guard let last = marblePens[ObjectIdentifier(t)] else { continue }
            let loc = t.location(in: self)
            markActivity()
            sumi_add_wake(inst,
                          Float(last.x / bounds.width), Float(last.y / bounds.height),
                          Float(loc.x / bounds.width), Float(loc.y / bounds.height),
                          marblePenTip(t))
            marblePens[ObjectIdentifier(t)] = loc
        }
    }
    override func touchesEnded(_ ts: Set<UITouch>, with event: UIEvent?) {
        super.touchesEnded(ts, with: event)
        for t in ts { marblePens.removeValue(forKey: ObjectIdentifier(t)) }
    }
    override func touchesCancelled(_ ts: Set<UITouch>, with event: UIEvent?) {
        super.touchesCancelled(ts, with: event)
        for t in ts { marblePens.removeValue(forKey: ObjectIdentifier(t)) }
    }

    @objc private func onTap(_ g: UITapGestureRecognizer) {
        guard let inst, g.state == .ended else { return }
        markActivity()
        let (x, y) = norm(g.location(in: self))
        sumi_add_drop(inst, x, y, DROP_RADIUS, 0)
    }

    @objc private func onPress(_ g: UILongPressGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        let loc = g.location(in: self)
        switch g.state {
        case .began:
            let (x, y) = norm(loc)
            sumi_add_drop(inst, x, y, DROP_RADIUS, SUMI_DROP_INK.rawValue)
            press = PressState(x: x, y: y, R: DROP_RADIUS, cy0: loc.y, cy: loc.y)
        case .changed:
            press?.cy = loc.y
        default:
            press = nil
        }
    }

    /// Once per frame while a long press is held: hold or push up = the
    /// boundary growth on the pressed drop (sumi_add_drop FEED); pull down =
    /// the Lamb-Oseen swirl with the drop as its core (sumi_add_vortex
    /// LAMB_OSEEN). Play mode's Y axis, without a note.
    private func pressureTick(dt: CFTimeInterval) {
        guard let inst, var pr = press else { return }
        let dy = Float((pr.cy0 - pr.cy) / max(bounds.height, 1))   // up = positive, canvas heights
        let up = min(1, max(0, dy / PRESS_TRAVEL)), down = min(1, max(0, -dy / PRESS_TRAVEL))
        let fdt = Float(dt)
        if down > 0.02 {
            let R = max(pr.R, 1e-4)
            sumi_add_vortex(inst, pr.x, pr.y, PRESS_SWIRL_OMEGA * down * fdt * 6.2831853 * R * R, R,
                            SUMI_VORTEX_LAMB_OSEEN.rawValue)
        } else {
            let dR = PRESS_FEED_RATE * (PRESS_FEED_IDLE + up) * fdt
            let r = ((pr.R + dR) * (pr.R + dR) - pr.R * pr.R).squareRoot()
            if r > 1e-4 {
                sumi_add_drop(inst, pr.x, pr.y, r, SUMI_DROP_FEED.rawValue)
                pr.R += dR
                press = pr
            }
        }
    }

    @objc private func onPan(_ g: UIPanGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        let p = g.location(in: self)
        switch g.state {
        case .began:
            panLast = p
        case .changed:
            // A long press in progress owns the touch — it modulates, it does not draw.
            if press != nil { return }
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

    private var pinchLastScale: CGFloat = 1.0
    @objc private func onPinch(_ g: UIPinchGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        switch g.state {
        case .began:
            pinchLastScale = g.scale
        case .changed:
            let dk = Float(g.scale - pinchLastScale) * 1.5   // deltas, never absolute
            pinchLastScale = g.scale
            guard abs(dk) > 0.0015, g.numberOfTouches >= 2 else { return }
            let p0 = g.location(ofTouch: 0, in: self)
            let p1 = g.location(ofTouch: 1, in: self)
            // Point space is isotropic, so the finger-to-finger angle is the
            // aspect-corrected fold angle directly.
            let angle = atan2f(Float(p1.y - p0.y), Float(p1.x - p0.x))
            let (x, y) = norm(g.location(in: self))
            sumi_add_pinch(inst, x, y, dk, angle)
        default:
            break
        }
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
            // #56: the profile from the settings, as the desktop's right drag.
            sumi_add_vortex(inst, x, y, strength, VORTEX_RADIUS, pendingVortexProfile)
        default:
            break
        }
    }
}

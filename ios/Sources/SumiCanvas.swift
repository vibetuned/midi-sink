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
    // Phase 6 step 44a: the session (params, palette, CC map, input, controls,
    // strip assignments) is ONE model, persisted through the preset
    // serializer; the ledger keeps the dips. The rest are shell switches.
    @ObservedObject var session: SessionStore
    var ledger: PrintLedger
    var playMode: Bool
    var velocityFromTouchSize: Bool
    var outVirtual: Bool
    var outNetwork: Bool
    var outBLE: Bool
    var sustainToggle: Bool

    func makeUIView(context: Context) -> SumiCanvasView {
        let v = SumiCanvasView()
        v.session = session
        v.ledger = ledger
        return v
    }
    func updateUIView(_ view: SumiCanvasView, context: Context) {
        view.applySession()
        view.velocityFromTouchSize = velocityFromTouchSize
        view.setPlayMode(playMode)
        view.setTransports(virtualSrc: outVirtual, network: outNetwork, ble: outBLE)
        view.setSustainToggleMode(sustainToggle)
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
        // v0.9 (#69): the right hand's swirl trio and the two grasp pinches.
        (9, "Swirl strength"), (10, "Swirl center X"), (11, "Swirl center Y"),
        (12, "Pinch (saddle)"), (13, "Pinch (crossed tines)"),
        // Phase 6 (steps 36–40): the operators' dimensions, the desktop's names.
        (14, "Torsion wavelength"), (15, "Torsion phase"), (16, "Chladni stir"),
        (17, "Chladni balance"), (18, "Spark frequency"), (19, "Chirikov throw"),
    ]
    static func ctlName(_ t: UInt32) -> String { ctlNames.first { $0.0 == t }?.1 ?? "?" }

    /// desktop/src/app_settings.cpp app_settings_default_routes, verbatim: the
    /// core's install_default_cc_map + the shells' ripple handles 102/103.
    static let defaults: [CcRoute] = [
        CcRoute(channel: 0xFF, cc: 1,  target: 0),   // mod wheel
        CcRoute(channel: 0xFF, cc: 2,  target: 6),   // breath
        CcRoute(channel: 0xFF, cc: 7,  target: 6),   // volume = breath alias
        CcRoute(channel: 0xFF, cc: 11, target: 6),   // expression = breath alias
        CcRoute(channel: 0xFF, cc: 26, target: 0),   // Airwave Raise L (#50/#69)
        CcRoute(channel: 0xFF, cc: 24, target: 1),   // Glide L
        CcRoute(channel: 0xFF, cc: 22, target: 2),   // Slide L (Y reversed at emit)
        CcRoute(channel: 0xFF, cc: 27, target: 9),   // Raise R: swirl strength
        CcRoute(channel: 0xFF, cc: 25, target: 10),  // Glide R: swirl X
        CcRoute(channel: 0xFF, cc: 23, target: 11),  // Slide R: swirl Y (reversed)
        CcRoute(channel: 0xFF, cc: 20, target: 12),  // Grasp L: saddle pinch
        CcRoute(channel: 0xFF, cc: 21, target: 13),  // Grasp R: crossed pinch
        CcRoute(channel: 0xFF, cc: 28, target: 8),   // Tilt L: ripple wavelength
        CcRoute(channel: 0xFF, cc: 29, target: 7),   // Tilt R: ripple amount
        CcRoute(channel: 0xFF, cc: 102, target: 7),  // the ripple handles
        CcRoute(channel: 0xFF, cc: 103, target: 8),
        // Phase 6 (steps 36–40): the operators' handles, as the desktop ships them.
        CcRoute(channel: 0xFF, cc: 104, target: 14), // torsion wavelength
        CcRoute(channel: 0xFF, cc: 105, target: 15), // torsion phase
        CcRoute(channel: 0xFF, cc: 106, target: 16), // Chladni stir (the Anod bend shares it, #72)
        CcRoute(channel: 0xFF, cc: 107, target: 17), // Chladni balance
        CcRoute(channel: 0xFF, cc: 108, target: 18), // spark frequency
        CcRoute(channel: 0xFF, cc: 109, target: 19), // Chirikov throw
    ]

    static func encode(_ routes: [CcRoute]) -> String {
        routes.map { "\($0.channel):\($0.cc):\($0.target)" }.joined(separator: ";")
    }
    /// Earlier DEFAULT maps (#71): a stored map equal to one of these is the
    /// stock map of an older version and reads as today's defaults; anything
    /// else is the user's and is kept.
    static let olderDefaults: [[CcRoute]] = [
        [CcRoute(channel: 0xFF, cc: 1, target: 0), CcRoute(channel: 0xFF, cc: 2, target: 6),
         CcRoute(channel: 0xFF, cc: 7, target: 6), CcRoute(channel: 0xFF, cc: 11, target: 6),
         CcRoute(channel: 0xFF, cc: 26, target: 0), CcRoute(channel: 0xFF, cc: 24, target: 1),
         CcRoute(channel: 0xFF, cc: 22, target: 2), CcRoute(channel: 0xFF, cc: 29, target: 3),
         CcRoute(channel: 0xFF, cc: 30, target: 4), CcRoute(channel: 0xFF, cc: 31, target: 5),
         CcRoute(channel: 0xFF, cc: 27, target: 7), CcRoute(channel: 0xFF, cc: 28, target: 8),
         CcRoute(channel: 0xFF, cc: 102, target: 7), CcRoute(channel: 0xFF, cc: 103, target: 8)],   // #50
        Array(defaults.prefix(16)),   // 1.0.0's map (#69), before the Phase-6 handles 104–109 (step 44a)
    ]

    static func decode(_ s: String) -> [CcRoute] {
        if s.isEmpty { return defaults }
        var out: [CcRoute] = []
        for part in s.split(separator: ";") {
            let f = part.split(separator: ":")
            guard f.count == 3, let ch = UInt32(f[0]), let cc = UInt8(f[1]), let t = UInt32(f[2]),
                  cc < 128, t < 20, ch == 0xFF || ch < 16 else { continue }
            out.append(CcRoute(channel: UInt8(ch), cc: cc, target: t))
        }
        if olderDefaults.contains(where: { Set($0) == Set(out) }) { return defaults }   // #71
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
    // (the feed / swirl rates live in the core since #75: sumi_gesture_press)
    private struct PressState { var x: Float; var y: Float; var R: Float; var cy0: CGFloat; var cy: CGFloat }
    private var press: PressState?
    private let VORTEX_STRENGTH: Float = 4.0
    private let DRAG_THRESHOLD_PT: CGFloat = 5.0
    private var panLast = CGPoint.zero
    private var rotLast: CGFloat = 0
    private var twist: UIRotationGestureRecognizer?
    // Phase 6 step 44a: the session the settings own (SessionStore), applied
    // whole on the render (main) thread; the memos keep a redraw cheap.
    weak var session: SessionStore?
    weak var ledger: PrintLedger?
    private var appliedParams: sumi_params_t? = nil
    private var appliedPalette: sumi_palette_t? = nil
    private var ccRoutes: [CcRoute] = CcMap.defaults
    private var pendingInputMode: UInt32 = 1       // #60: MPE by default, never a detection
    private var appliedInputMode: UInt32 = 0
    private var ccRoutesApplied: [CcRoute]? = nil
    private var controlsSent: [UInt32: Int] = [:]  // last value pushed per routed control
    private var pendingControls: [UInt32: Int] = [:]
    private var stripAssignApplied: (UInt8, UInt8) = (0, 0)
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
        // Step 44a: the core's defaults are what a preset's missing keys fall
        // back to — hand them to the session (once), then apply the session.
        var coreDefaults = sumi_params_t()
        sumi_get_params(created, &coreDefaults)
        var paletteDefault = sumi_palette_t()
        sumi_palette_preset(0, 0, &paletteDefault, nil)   // the custom slot starts as Sumi black
        appliedParams = nil; appliedPalette = nil; ccRoutesApplied = nil; appliedInputMode = 0
        controlsSent = [:]; stripAssignApplied = (0, 0)
        if let session, !session.ready {
            DispatchQueue.main.async { [weak self] in
                session.attach(coreDefaults: coreDefaults, paletteDefault: paletteDefault)
                self?.applySession()
            }
        }
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
        applySession()   // the session, when ready (the MIDI queue exists now: the controls can be sent)
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
        ledger?.tick(inst)               // step 44a: the dip's print, an export in flight
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

    /// Phase 6 step 44a: the whole session into the core — params (one
    /// sumi_params_t, byte-compared), the custom palette, the CC map, the input
    /// dialect, the routed controls (sent as their CCs through the MIDI path,
    /// like a controller) and the strip's latch-wheel assignments. Render
    /// (main) thread; a no-op until the session is ready and the instance exists.
    func applySession() {
        guard let inst, let session, session.ready else { return }
        var p = session.params
        if appliedParams.map({ !podEqual($0, p) }) ?? true {
            sumi_set_params(inst, &p)
            appliedParams = p
            sumi_get_params(inst, &paramsSnapshot)   // the core's clamped values: the probe's ground truth (§8.2)
            let dark = paramsSnapshot.medium == SUMI_MEDIUM_ANOD.rawValue   // step 44a: light marks on the glass
            overlay.setDarkTheme(dark)
            strip.setDarkTheme(dark)
            overlay.layoutParamsChanged()
            applyMode()
        }
        var pal = session.palette
        if appliedPalette.map({ !podEqual($0, pal) }) ?? true {
            sumi_set_palette(inst, &pal)
            appliedPalette = pal
        }
        ccRoutes = session.ccRoutes
        applyCcMap()
        pendingInputMode = UInt32(min(3, max(1, session.inputMode)))
        applyInputMode()
        pendingControls = session.controls
        sendControls()
        let want = (session.stripAssignA, session.stripAssignB)
        if want != stripAssignApplied {
            stripAssignApplied = want
            midiQueue.async { [weak self] in
                guard let self, let se = self.stripEngine else { return }
                if want.0 != 0 { _ = hostmpe_strip_assign(se, 1, want.0) }
                if want.1 != 0 { _ = hostmpe_strip_assign(se, 2, want.1) }
                DispatchQueue.main.async { self.syncStripMirrors() }
            }
        }
    }

    /// The params the settings see (clamped by the core).
    var liveParams: sumi_params_t { paramsSnapshot }

    private func applyInputMode() {
        guard let inst, appliedInputMode != pendingInputMode else { return }
        sumi_set_input_mode(inst, sumi_input_mode_t(rawValue: pendingInputMode))
        appliedInputMode = pendingInputMode
    }

    /// The routed controls (ripple amount / wavelength, Chladni stir / balance,
    /// spark frequency, Chirikov throw): each value that changed travels as its
    /// routed CC through the sole producer — the route a controller would use.
    private func sendControls() {
        guard inst != nil else { return }
        var msgs: [(UInt8, UInt8)] = []
        for (ctl, v) in pendingControls.sorted(by: { $0.key < $1.key }) where controlsSent[ctl] != v {
            controlsSent[ctl] = v
            if let cc = CcMap.route(ccRoutes, for: ctl) { msgs.append((cc, UInt8(min(127, max(0, v))))) }
        }
        guard !msgs.isEmpty else { return }
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst else { return }
            for (cc, v) in msgs {
                self.logByte(0xB0, cc, v, src: 2)
                sumi_push_midi(inst, 0xB0, cc, v)
            }
        }
    }

    private func applyCcMap() {
        guard let inst, ccRoutesApplied != ccRoutes else { return }
        sumi_clear_cc_map(inst)
        for r in ccRoutes {
            sumi_map_cc(inst, r.channel, r.cc, sumi_ctl_t(rawValue: r.target))
        }
        ccRoutesApplied = ccRoutes
        controlsSent = [:]   // the handles may have moved to other CCs
    }

    /// Play mode (Phase 4): effective only on the playable layouts (grid,
    /// Jankó, piano grid — the probe refuses everything else anyway); Marble
    /// mode leaves the recognizers exactly as Step 13 shipped them.
    func setPlayMode(_ play: Bool) {
        playModeRequested = play
        applyMode()
    }

    private func applyMode() {
        let layout = paramsSnapshot.pitch_layout
        let playable = layout == 1 || layout == 2 || layout == 5
        let effective = playModeRequested && playable
        for g in marbleRecognizers { g.isEnabled = !effective }
        overlay.isHidden = !effective
        overlay.isUserInteractionEnabled = effective
        NSLog("[mode] requested=%d layout=%u playable=%d effective=%d (was %d)",
              playModeRequested ? 1 : 0, layout, playable ? 1 : 0,
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
            let assigned = hostmpe_strip_assigned_cc(se, wheel)
            completion(ok, assigned)
            // Step 44a: the assignment is part of the session (presets carry it).
            if ok {
                DispatchQueue.main.async {
                    guard let session = self.session else { return }
                    if wheel == 1 { session.stripAssignA = assigned; self.stripAssignApplied.0 = assigned }
                    if wheel == 2 { session.stripAssignB = assigned; self.stripAssignApplied.1 = assigned }
                }
            }
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
        sumi_gesture_pinch(inst, x, y, k, angle, 2 * VORTEX_RADIUS)   // #75: Anod the burst (the pen has no finger span)
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

    /// Step 44a (QOL §4, §6): the two sheet actions, worded apart — DIP lifts
    /// the sheet as a print into the ledger (the field kept for re-export at
    /// any size), then fresh paper; CLEAR is fresh paper and nothing kept.
    func paperDip() {
        guard let inst else { return }
        if let ledger { ledger.dip(inst) } else { sumi_trigger_paper_dip(inst) }
        NSLog("[dip] paper dip from settings (kept in the ledger)")
    }
    func clearCanvas() {
        guard let inst else { return }
        if let ledger { ledger.clear(inst) } else { sumi_trigger_paper_dip(inst) }
        NSLog("[dip] canvas cleared from settings (nothing kept)")
    }
    /// A ledger re-export: the entry's look round the render, the session's after.
    func exportPrint(id: UUID, choice: Int, anodAlpha: Bool) {
        guard let inst, let ledger else { return }
        ledger.export(inst, id: id, choice: choice, anodAlpha: anodAlpha,
                      current: appliedParams ?? paramsSnapshot,
                      currentPalette: appliedPalette ?? sumi_palette_t())
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
        sumi_gesture_tap(inst, x, y, DROP_RADIUS)   // #75: the medium's tap (Sumi the drop, Anod the strike)
    }

    @objc private func onPress(_ g: UILongPressGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        let loc = g.location(in: self)
        switch g.state {
        case .began:
            let (x, y) = norm(loc)
            sumi_gesture_tap(inst, x, y, DROP_RADIUS)   // #75: the press starts as a tap
            press = PressState(x: x, y: y, R: DROP_RADIUS, cy0: loc.y, cy: loc.y)
        case .changed:
            press?.cy = loc.y
        default:
            if press != nil { sumi_gesture_press_end(inst) }   // #75: lets go of a stir it set
            press = nil
        }
    }

    /// Once per frame while a long press is held: Play mode's Y axis, without
    /// a note. #75: the core plays the frame by the medium — Sumi: hold or push
    /// up = the boundary growth on the pressed drop, pull down = the Lamb–Oseen
    /// swirl on it (the 1.0 gesture); Anod: hold or push = the torsion sweep
    /// feed around the charge, pull = the Chladni stir, reversed.
    private func pressureTick(dt: CFTimeInterval) {
        guard let inst, var pr = press else { return }
        let dy = Float((pr.cy0 - pr.cy) / max(bounds.height, 1))   // up = positive, canvas heights
        let up = min(1, max(0, dy / PRESS_TRAVEL)), down = min(1, max(0, -dy / PRESS_TRAVEL))
        pr.R = sumi_gesture_press(inst, pr.x, pr.y, pr.R, up, down, dt)
        press = pr
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
            let span = Float(hypot(p1.x - p0.x, p1.y - p0.y) / max(bounds.height, 1))   // canvas heights
            sumi_gesture_pinch(inst, x, y, dk, angle, span)   // #75: Anod the burst
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
            sumi_gesture_twist(inst, x, y, strength, VORTEX_RADIUS, paramsSnapshot.vortex_profile)   // #75: Anod the torsion vortex
        default:
            break
        }
    }
}

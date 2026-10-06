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
import Voxo
import SumiReplay   // Phase 9 step 65: session replay
import SumiTrace    // step 67's app fixes: the orbit trace (DECISIONS_9 #11)

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
    var leftHanded: Bool       // step 63 (QOL §2)
    var quickSwitch: String    //   the Next pad's subset, layout ids comma-separated
    var fingeringHorizontal: Bool   //   the author's ask: the valves and the slide side by side at the bottom

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
        view.setMirror(leftHanded)
        view.setQuickSwitch(quickSwitch)
        view.setFingeringHorizontal(fingeringHorizontal)
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
        // Phase 7 step 53 (DECISIONS_6 #27): Voxo's bus, numbered from 1000 (presets/SCHEMA.md).
        (1000, "Reverb amount"), (1001, "Reverb room"), (1002, "Reverb damping"),
        (1003, "Delay amount"), (1004, "Delay time"), (1005, "Delay feedback"),
    ]
    static func isVoxo(_ t: UInt32) -> Bool { t >= 1000 }
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

// Phase 9 step 65: the replay library's push callbacks (C function pointers: no captures).
private func replayPushCoreCB(_ user: UnsafeMutableRawPointer?, _ s: UInt8, _ d1: UInt8, _ d2: UInt8, _ src: UInt8) {
    guard let user else { return }
    sumi_push_midi(OpaquePointer(user), s, d1, d2)   // a recording's staged bytes: the core alone (Voxo had them at once)
}
private func replayPushCB(_ user: UnsafeMutableRawPointer?, _ s: UInt8, _ d1: UInt8, _ d2: UInt8, _ src: UInt8) {
    guard let user else { return }
    Unmanaged<SumiCanvasView>.fromOpaque(user).takeUnretainedValue().replayPushBoth(s, d1, d2)   // a replay's bytes: the core and Voxo
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
    private let fingeringPanel = FingeringPanelView()   // step 63 (the author's fix): the valves and the slide, large, at the side
    private var stripEngine: OpaquePointer?   // hostmpe_strip_t*, midiQueue only
    private var sustainToggleMode = false
    // Phase 9 step 63 (DECISIONS_8 #13): THE FINGERING MIRROR — the strip's valves and slide as sent,
    // the shell-side reader of the bytes (one source of truth, the byte stream; DECISIONS_8 #1): the
    // state the overlay hands the probe. `fingering` is the main thread's copy, `fingeringQ` the MIDI
    // queue's (the retune reads it there); `paramsQ` / `aspectQ` the queue's copies of what the
    // retune's re-probe needs.
    private(set) var fingering = sumi_layout_state_t()
    private var fingeringQ = sumi_layout_state_t()
    private var paramsQ = sumi_params_t()
    private var aspectQ: Float = 1.0
    // the held voices' cells (midiQueue only): a fingering change re-probes them under the new state
    private struct HeldCell { var cx: Float; var cy: Float; var note: UInt8; var theremin: Bool }
    private var heldCells: [Int32: HeldCell] = [:]
    private var mirrored = false
    private var fingeringHorizontal = false
    private var quickIds: [UInt32] = []
    private var demoRunning = false

    // -- Phase 4 §5.2: the serial MIDI queue is the SOLE producer -----------
    // Every byte — CoreMIDI devices AND touch-generated — crosses
    // sumi_push_midi only from this queue; hostmpe state lives on it too.
    private let midiQueue = DispatchQueue(label: "com.vibetuned.midi-sink.midi")
    // Phase 7 step 48 (SOUND §1): Voxo beside the core — the one producer fans
    // the same bytes into its ring through push(). Created with the instance;
    // started only by the spike until step 53 wires the setting.
    private(set) var voxo: OpaquePointer?
    // Step 67's app fixes (DECISIONS_9 #11): the orbit trace — the synth drawing itself into the water at its cell, or
    // on the canvas scope — the shared library's bridge, run on the main thread before each live tick's update; its
    // ink segments go to the recorder while one runs; off (both routes) the water is as if it never existed.
    private var trace: OpaquePointer?
    private var traceActive = false
    private var localControlOn = true   // midiQueue only (step 53, DECISIONS_6 #12): the shell's own bytes reach Voxo while on
    // -- Phase 9 step 65 (QOL §1, DECISIONS_8 #22–#23): session replay ------------------------------------------
    // The recorder: while it runs, the one producer (midiQueue) STAGES its bytes in it instead of pushing, and the
    // render (main) thread hands them to the core at each tick's start — every byte's frame exact by construction.
    // The player: the live bytes drop (the replay owns the loopback and Voxo), the recorded frames run on the
    // scripted clock, as many per display tick as the wall clock asks. The flags are midiQueue-only and flipped
    // under midiQueue.sync; the pointers are main-only.
    private var recorder: OpaquePointer?
    private var recordingQ = false
    private var player: OpaquePointer?
    private var replayingQ = false
    private var playAcc = 0.0
    private var labSize: (UInt32, UInt32)?   // --record-lab: the small field of the cross-device gate
    private var labReplay = false            // --replay-file … --replay-dump: the field after the replay dumped beside it
    /// Every byte the core gets; the shell's own (touch, pen, strip — `local`) reach Voxo only under Local Control.
    private func push(_ inst: OpaquePointer, _ status: UInt8, _ d1: UInt8, _ d2: UInt8, local: Bool = true) {
        if replayingQ { return }   // step 65: a replay owns the loopback and the sound
        if recordingQ, let rec = recorder { _ = sumi_replay_rec_midi(rec, CACurrentMediaTime(), status, d1, d2, local ? 1 : 0) }   // staged: main pushes at the frame
        else { sumi_push_midi(inst, status, d1, d2) }
        if let v = voxo, !local || localControlOn { voxo_push_midi(v, status, d1, d2) }
    }
    /// A replay's bytes: the core and Voxo, from the render thread (the one producer while the live path is muted).
    fileprivate func replayPushBoth(_ status: UInt8, _ d1: UInt8, _ d2: UInt8) {
        guard let inst else { return }
        sumi_push_midi(inst, status, d1, d2)
        if let v = voxo { voxo_push_midi(v, status, d1, d2) }
    }
    func setLocalControl(_ on: Bool) { midiQueue.async { [self] in localControlOn = on } }
    /// Step 53 (#27): the play surface hides the cells the loaded instrument cannot sound.
    func refreshInstrumentReach(soundOn: Bool) {
        var mask = [UInt8](repeating: 0, count: 16)
        // step 63: Suzu sounds every note — the sampler's reach is the sampler's alone
        if soundOn, SoundController.shared.source == 0, let v = voxo, voxo_covered_notes(v, &mask) { overlay.setCoveredNotes(mask) }
        else { overlay.setCoveredNotes(nil) }
    }
    /// The spike's note-ons, through the one producer (midiQueue).
    func spikePush(_ status: UInt8, _ d1: UInt8, _ d2: UInt8) {
        midiQueue.sync { [self] in
            guard let inst else { return }
            push(inst, status, d1, d2)
        }
    }
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
        overlay.stateProvider = { [weak self] in self?.fingering ?? sumi_layout_state_t() }   // step 63
        overlay.host = self
        overlay.isHidden = true
        overlay.isUserInteractionEnabled = false
        addSubview(overlay)
        strip.host = self
        strip.isHidden = true
        addSubview(strip)
        fingeringPanel.host = self
        fingeringPanel.isHidden = true
        addSubview(fingeringPanel)
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
        let w = labSize?.0 ?? UInt32(bounds.width * contentScaleFactor)   // step 65: the lab's small field while it records
        let h = labSize?.1 ?? UInt32(bounds.height * contentScaleFactor)
        guard w > 0, h > 0, window != nil else { return }
        let a = Float(bounds.width / bounds.height)
        midiQueue.async { [weak self] in self?.aspectQ = a }
        layoutPlaySurface()
        if inst == nil {
            start(width: w, height: h)
        } else {
            let ratio: Float = labSize != nil ? 1 : Float(contentScaleFactor)
            sumi_resize(inst, w, h, ratio)
            if let rec = recorder { sumi_replay_rec_resize(rec, w, h, ratio) }   // step 65: a recording's resize event
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
        // step 63: the strip grows with its widgets (Next, Panic) and sits at the other corner
        // left-handed; on the brass layouts it moves to the RIGHT and the fingering panel takes the
        // left side at mid-height (the author's fix: the valves and the slide large, under the free
        // hand) — the other way round left-handed
        let layout = paramsSnapshot.pitch_layout
        let brass = layout == 8 || layout == 9
        let w: CGFloat = min(strip.preferredWidth, bounds.width * 0.6)
        let stripRight = brass != mirrored
        // at the right the strip stops short of the settings gear in the corner (the author's fix: Panic sat under it)
        let x: CGFloat = stripRight ? bounds.width - w - 10 - 52 : 10
        strip.frame = CGRect(x: x, y: safeAreaInsets.top + 10, width: w, height: 86)
        let ps = fingeringPanel.preferredSize
        if fingeringHorizontal {
            // the horizontal form: along the bottom edge, at the free hand's side
            let pw = min(ps.width, bounds.width * 0.5)
            let px: CGFloat = mirrored ? bounds.width - pw - 12 : 12
            fingeringPanel.frame = CGRect(x: px, y: bounds.height - safeAreaInsets.bottom - ps.height - 12, width: pw, height: ps.height)
        } else {
            let ph = min(ps.height, bounds.height * 0.64)
            let px: CGFloat = mirrored ? bounds.width - ps.width - 12 : 12
            fingeringPanel.frame = CGRect(x: px, y: (bounds.height - ph) / 2, width: ps.width, height: ph)
        }
    }
    func setFingeringHorizontal(_ on: Bool) {
        guard on != fingeringHorizontal else { return }
        fingeringHorizontal = on
        fingeringPanel.setOrientation(on ? .horizontal : .vertical)
        layoutPlaySurface()
    }
    /// The panel's own rotate button: the form flips and persists — the settings' toggle (@AppStorage
    /// "fingeringHorizontal") reads the same default and follows. Under the lab's `--fingering-horizontal`
    /// the override wins again on the next SwiftUI update.
    func toggleFingeringOrientation() {
        let on = !fingeringHorizontal
        UserDefaults.standard.set(on, forKey: "fingeringHorizontal")
        setFingeringHorizontal(on)
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
        // Phase 7 step 48: Voxo beside the core (the device stays closed until
        // the spike — or, from step 53, the setting — starts it).
        var vcfg = voxo_config_t(sample_rate: 48000, block_frames: 0, max_voices: 16,
                                 log_cb: { _, msg, _ in if let msg { NSLog("[voxo] %@", String(cString: msg)) } },
                                 log_user: nil)
        voxo = voxo_create(&vcfg)
        trace = sumi_trace_create()   // step 67: the orbit trace's bridge
        if let t = trace {
            sumi_trace_set_gesture_hook(t, { user, kind, args, n in
                guard let user else { return }
                Unmanaged<SumiCanvasView>.fromOpaque(user).takeUnretainedValue().recordTraceGesture(kind, args, n)
            }, Unmanaged.passUnretained(self).toOpaque())
        }
        if voxo == nil { NSLog("[voxo] create failed; the shell runs without sound") }
        else { SoundController.shared.canvasReady() }   // step 53: the session, the instrument, the device
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
                self.push(inst, status, d1, d2, local: false)   // an external controller: always sounds
            }
            self.markActivity()
        }
        applyTransports()   // sinks now exist: apply whatever SwiftUI set
        midi?.excludedUniqueIDs = excluded
        // Phase 9 step 63 (QOL §2): a known controller's settings are OFFERED when it appears — the ones
        // already connected included — never applied (DECISIONS_5 #7).
        let offer: (String) -> Void = { name in
            let p = hostmpe_device_profile(name)
            guard p.device != HOSTMPE_DEVICE_NONE else { return }
            let family = String(cString: p.name)
            let mode = Int(p.input_mode)
            DispatchQueue.main.async { DeviceOffers.shared.post(device: name, family: family, mode: mode) }
        }
        midi?.onSourceAppeared = offer
        midi?.forEachInput(offer)
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
        if let t = trace { sumi_trace_destroy(t); trace = nil }   // step 67: before Voxo, which it polls
        if let v = voxo { voxo_destroy(v) }   // step 48: after the producers, before the core
        voxo = nil
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
        if let pl = player {
            // step 65: the replay drives the clock — its frames, each an update at the recorded dt and a render, as many as
            // the wall clock asks (a 120 Hz recording on this 120 Hz link: one a tick; a 60 Hz one: every other tick);
            // nothing due: re-composite only (the field stays). The live input is muted meanwhile.
            playAcc += dt
            if playAcc > 0.25 { playAcc = 0.25 }
            var n = 0
            let me = Unmanaged.passUnretained(self).toOpaque()
            while n < 8 && playAcc > 0 {
                var fdt = 0.0
                if !sumi_replay_step(pl, inst, 0, replayPushCB, me, &fdt) { break }
                sumi_update(inst, fdt)
                sumi_render(inst)
                playAcc -= fdt
                n += 1
            }
            if n == 0 { sumi_render(inst) }
            ReplayStatus.shared.tickPlay(elapsed: sumi_replay_elapsed(pl), duration: sumi_replay_duration(pl))
            if sumi_replay_peek_dt(pl) <= 0 { stopReplay(finished: true) }
        } else {
            overlay.penGestureTick(dt: dt)   // #40: barrel gestures decay (τ 0.4 s)
            pressureTick(dt: dt)             // #49: the Marble-mode long press
            if let rec = recorder {          // step 65: the staged bytes into the core, then the frame boundary
                _ = sumi_replay_rec_frame(rec, now, dt, replayPushCoreCB, UnsafeMutableRawPointer(inst))
                ReplayStatus.shared.tickRec(frames: sumi_replay_rec_frames(rec), seconds: sumi_replay_rec_seconds(rec))
            }
            if traceActive, let v = voxo, let t = trace {   // step 67: the synth's orbits into the water / the scope, before the update
                var p = paramsSnapshot
                sumi_trace_frame(t, v, inst, &p, Float(bounds.width / max(bounds.height, 1)))
            }
            sumi_update(inst, dt)
            sumi_render(inst)
        }
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
            if let mpe = self.mpe {   // step 63: the brass retune's ramps (DECISIONS_8 #10)
                var v = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 16)
                let n = hostmpe_tick(mpe, qnow, &v, 16)
                self.voiceDispatch(v, count: n, exempt: false)
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
        if player != nil {   // step 65: while a replay plays the look is the viewer's, the physics the recording's
            var pal = session.palette
            if appliedPalette.map({ !podEqual($0, pal) }) ?? true { sumi_set_palette(inst, &pal); appliedPalette = pal }
            return
        }
        var p = session.params
        if appliedParams.map({ !podEqual($0, p) }) ?? true {
            sumi_set_params(inst, &p)
            appliedParams = p
            sumi_get_params(inst, &paramsSnapshot)   // the core's clamped values: the probe's ground truth (§8.2)
            let pq = paramsSnapshot
            midiQueue.async { [weak self] in self?.paramsQ = pq }   // step 63: the retune's re-probe reads it on the queue
            let dark = paramsSnapshot.medium == SUMI_MEDIUM_ANOD.rawValue   // step 44a: light marks on the glass
            overlay.setDarkTheme(dark)
            strip.setDarkTheme(dark)
            fingeringPanel.setDarkTheme(dark)
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
        recordState()   // step 65: the session as applied, a recording's state event (a repeat is skipped by the recorder)
    }

    // -- Phase 9 step 65: recording and playback (main thread) --------------------------------------------------

    /// The session as applied — the params AS THE CORE HOLDS THEM (sim_scale included): a recording's header and state events.
    private func sessionTextEffective() -> String {
        guard let session else { return "" }
        var p = session.toPreset(name: "session")
        p.params = paramsSnapshot
        return session.text(of: p)
    }
    private func recordState() {
        guard let rec = recorder else { return }
        sessionTextEffective().withCString { sumi_replay_rec_state(rec, $0, 0) }
    }
    private func recordDip() { if let rec = recorder { sumi_replay_rec_dip(rec) } }
    /// Step 67: the orbit trace's ink segments, as the core's calls, into the recording (main thread, from the hook).
    fileprivate func recordTraceGesture(_ kind: UInt32, _ args: UnsafePointer<Float>?, _ n: UInt32) {
        guard let rec = recorder else { return }
        sumi_replay_rec_gesture(rec, kind, args, n)
    }
    /// Step 67's app fixes (DECISIONS_9 #11): the orbit trace's routes from the Sound page — every voice kind traced
    /// (the patch picks what sounds), the ink into the water, the scope on the canvas; off, the scope is cleared once.
    func refreshTrace() {
        guard let t = trace else { return }
        let snd = SoundController.shared
        let on = snd.source != 0 && (snd.traceInk || snd.traceCanvas != 0)
        var c = sumi_trace_config_t(); sumi_trace_default_config(&c)
        c.scope = 0; c.ink = snd.traceInk ? 1 : 0; c.kinds = 0x1FF; c.canvas = UInt32(min(max(snd.traceCanvas, 0), 2)); c.scale = snd.traceScale
        sumi_trace_configure(t, &c)
        if let v = voxo { voxo_set_trace(v, on ? 0x1FF : 0) }
        if !on, traceActive, let inst { sumi_set_scope(inst, nil, nil, 0, SUMI_SCOPE_OFF) }
        traceActive = on
    }
    private func recordGesture(_ kind: sumi_replay_gesture_t, _ args: [Float]) {
        guard let rec = recorder else { return }
        args.withUnsafeBufferPointer { sumi_replay_rec_gesture(rec, UInt32(kind.rawValue), $0.baseAddress, UInt32($0.count)) }
    }
    // The gesture calls go through these: the core's call, recorded when recording, ignored while a replay plays.
    private func gTap(_ x: Float, _ y: Float, _ r: Float) {
        guard let inst, player == nil else { return }
        sumi_gesture_tap(inst, x, y, r); recordGesture(SUMI_REPLAY_G_TAP, [x, y, r])
    }
    private func gPinch(_ x: Float, _ y: Float, _ k: Float, _ angle: Float, _ span: Float) {
        guard let inst, player == nil else { return }
        sumi_gesture_pinch(inst, x, y, k, angle, span); recordGesture(SUMI_REPLAY_G_PINCH, [x, y, k, angle, span])
    }
    private func gTwist(_ x: Float, _ y: Float, _ strength: Float, _ radius: Float, _ profile: UInt32) {
        guard let inst, player == nil else { return }
        sumi_gesture_twist(inst, x, y, strength, radius, profile); recordGesture(SUMI_REPLAY_G_TWIST, [x, y, strength, radius, Float(profile)])
    }
    private func gPress(_ x: Float, _ y: Float, _ R: Float, _ up: Float, _ down: Float, _ dt: Double) -> Float {
        guard let inst, player == nil else { return R }
        recordGesture(SUMI_REPLAY_G_PRESS, [x, y, R, up, down, Float(dt)])   // the inputs as the core gets them
        return sumi_gesture_press(inst, x, y, R, up, down, Double(Float(dt)))   // dt as the file carries it
    }
    private func gPressEnd() {
        guard let inst, player == nil else { return }
        sumi_gesture_press_end(inst); recordGesture(SUMI_REPLAY_G_PRESS_END, [])
    }
    private func gTine(_ x0: Float, _ y0: Float, _ x1: Float, _ y1: Float, _ alpha: Float, _ magnitude: Float) {
        guard let inst, player == nil else { return }
        sumi_add_tine(inst, x0, y0, x1, y1, alpha, magnitude); recordGesture(SUMI_REPLAY_G_TINE, [x0, y0, x1, y1, alpha, magnitude])
    }
    private func gWake(_ x0: Float, _ y0: Float, _ x1: Float, _ y1: Float, _ tip: Float) {
        guard let inst, player == nil else { return }
        sumi_add_wake(inst, x0, y0, x1, y1, tip); recordGesture(SUMI_REPLAY_G_WAKE, [x0, y0, x1, y1, tip])
    }

    /// Record: the sheet kept and dipped (the recording's first event), the session as the header, frame 0 carrying
    /// the MCM and the strip's announce (sendSessionConfig) and the routed controls' CCs — the midiQueue stages from now.
    @discardableResult
    func startRecording() -> Bool {
        guard inst != nil, recorder == nil, player == nil, let session, session.ready else { return false }
        var info = sumi_replay_info_t()
        func put<T>(_ kp: WritableKeyPath<sumi_replay_info_t, T>, _ s: String) {
            let cap = MemoryLayout<T>.size
            let bytes = Array(s.utf8.prefix(cap - 1))
            withUnsafeMutableBytes(of: &info[keyPath: kp]) { raw in
                for (i, b) in bytes.enumerated() { raw[i] = b }
                raw[bytes.count] = 0
            }
        }
        put(\.platform, "ios"); put(\.backend, "metal"); put(\.device, Replay.deviceModel()); put(\.app, Replay.appVersion()); put(\.recorded, Replay.timestamp())
        info.sumi_version = sumi_version()
        info.width = labSize?.0 ?? UInt32(bounds.width * contentScaleFactor)
        info.height = labSize?.1 ?? UInt32(bounds.height * contentScaleFactor)
        info.pixel_ratio = labSize != nil ? 1 : Float(contentScaleFactor)
        let text = sessionTextEffective()
        guard let rec = text.withCString({ sumi_replay_rec_create(&info, $0, 0) }) else { return false }
        recorder = rec
        midiQueue.sync { [self] in recordingQ = true }
        paperDip()                                // the sheet kept, fresh paper
        sendSessionConfig()                       // frame 0: the MCM, the members' RPN 0, the strip's announce
        controlsSent = [:]; sendControls()        // the routed controls as their CCs
        ReplayStatus.shared.recording = true
        ReplayStatus.shared.status = "Recording"
        return true
    }
    /// Stop: the stage drained into the core (unrecorded: it belongs to the frames after), the file written.
    @discardableResult
    func stopRecording() -> URL? {
        guard let inst, let rec = recorder else { return nil }
        midiQueue.sync { [self] in
            recordingQ = false
            _ = sumi_replay_rec_flush(rec, replayPushCoreCB, UnsafeMutableRawPointer(inst))
        }
        recorder = nil
        let url = Replay.url(Replay.fileStamp() + ".sumireplay")
        let ok = sumi_replay_rec_save(rec, url.path)
        let frames = sumi_replay_rec_frames(rec), secs = sumi_replay_rec_seconds(rec), dropped = sumi_replay_rec_dropped(rec)
        sumi_replay_rec_destroy(rec)
        ReplayStatus.shared.recording = false
        ReplayStatus.shared.status = ok
            ? String(format: "Saved %@ (%u frames, %.1f s%@)", url.lastPathComponent, frames, secs, dropped > 0 ? ", some bytes dropped" : "")
            : "Could not write \(url.lastPathComponent)"
        ReplayStatus.shared.refreshNames()
        NSLog("[replay] %@", ReplayStatus.shared.status)
        return ok ? url : nil
    }
    /// Play: the live bytes muted, the recording's session applied (this screen's size and palette kept), the frames
    /// from the next tick on; the banner names the source.
    @discardableResult
    func playReplay(_ url: URL, lab: Bool = false) -> Bool {
        guard let inst, recorder == nil else { return false }
        if player != nil { stopReplay(finished: false) }
        guard let pl = sumi_replay_load(url.path) else { ReplayStatus.shared.status = "Not a replay file: \(url.lastPathComponent)"; return false }
        midiQueue.sync { [self] in replayingQ = true }
        player = pl
        playAcc = 0
        if traceActive { sumi_set_scope(inst, nil, nil, 0, SUMI_SCOPE_OFF) }   // step 67: the live trace stays out of a replay (it carries its own)
        labReplay = lab
        if lab {   // the gate's replay: the recording's size and palette, kept through every layout pass
            let i = sumi_replay_info(pl)!.pointee
            labSize = (i.width, i.height)
            sumi_replay_begin(pl, inst, UInt32(SUMI_REPLAY_APPLY_SIZE | SUMI_REPLAY_APPLY_PALETTE))
        } else {
            sumi_replay_begin(pl, inst, 0)
        }
        let i = sumi_replay_info(pl)!.pointee
        ReplayStatus.shared.banner = "Replaying \(Replay.cstr(i.device)) (\(Replay.cstr(i.platform)), \(Replay.cstr(i.backend))) · midi-sink \(Replay.cstr(i.app)) · \(Replay.cstr(i.recorded))"
        ReplayStatus.shared.playDuration = sumi_replay_duration(pl)
        ReplayStatus.shared.playElapsed = 0
        ReplayStatus.shared.status = ""
        NSLog("[replay] %@ (%u frames, %.1f s)", ReplayStatus.shared.banner, sumi_replay_frame_count(pl), sumi_replay_duration(pl))
        return true
    }
    func stopReplay(finished: Bool) {
        guard let pl = player else { return }
        if labReplay, let inst {   // the field as the last replayed frame left it, before the viewer's session resizes it
            Replay.dumpField(inst, to: Replay.url("replayed.field.bin"))
            labReplay = false
            labSize = nil
            setNeedsLayout()
            NSLog("[replay] lab replay done (%@)", finished ? "finished" : "stopped")
        }
        player = nil
        sumi_replay_close(pl)
        midiQueue.sync { [self] in replayingQ = false }
        appliedParams = nil; appliedPalette = nil; ccRoutesApplied = []; appliedInputMode = 0; controlsSent = [:]   // the viewer's session back
        applySession()
        ReplayStatus.shared.banner = ""
        ReplayStatus.shared.status = finished ? "Replay finished" : "Replay stopped"
    }
    /// --record-lab <s> (the evidence): the instance at a small field — 640 wide at the view's aspect, pixel ratio 1,
    /// sim_scale 1 — the recording for <s> seconds (the fingering demo's phrase lands inside), then the file copied to
    /// Documents/Replays/lab.sumireplay and the field after the last frame beside it as the comparator reads it.
    func startLabRecording(seconds: Double) {
        guard let session, inst != nil else { return }
        let aspect = Float(bounds.width / max(bounds.height, 1))
        labSize = (640, UInt32((640 / max(aspect, 0.1)).rounded()))
        session.params.sim_scale = 1.0
        setNeedsLayout(); layoutIfNeeded()
        NSLog("[replay] lab: field %ux%u @1, sim_scale 1", labSize!.0, labSize!.1)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            guard let self else { return }
            self.applySession()
            self.startRecording()
            DispatchQueue.main.asyncAfter(deadline: .now() + seconds) { [weak self] in
                guard let self, let inst = self.inst else { return }
                let url = self.stopRecording()
                let lab = Replay.url("lab.sumireplay")
                if let url { try? FileManager.default.removeItem(at: lab); try? FileManager.default.copyItem(at: url, to: lab) }
                Replay.dumpField(inst, to: Replay.url("lab.field.bin"))   // before the next tick: the field as the last recorded frame left it
                self.labSize = nil
                self.setNeedsLayout()
                ReplayStatus.shared.refreshNames()
                NSLog("[replay] lab recording done: %@", url?.lastPathComponent ?? "FAILED")
            }
        }
    }

    /// The params the settings see (clamped by the core).
    var liveParams: sumi_params_t { paramsSnapshot }

    private func applyInputMode() {
        guard let inst, appliedInputMode != pendingInputMode else { return }
        sumi_set_input_mode(inst, sumi_input_mode_t(rawValue: pendingInputMode))
        if let v = voxo { voxo_set_input_mode(v, pendingInputMode) }   // step 53 (#25): Voxo speaks the session's dialect too
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
                self.push(inst, 0xB0, cc, v)
            }
        }
    }

    private func applyCcMap() {
        guard let inst, ccRoutesApplied != ccRoutes else { return }
        sumi_clear_cc_map(inst)
        if let v = voxo { voxo_clear_cc_map(v) }
        for r in ccRoutes {
            if CcMap.isVoxo(r.target) { if let v = voxo { voxo_map_cc(v, r.channel, r.cc, r.target) } }   // step 53: the bus
            else { sumi_map_cc(inst, r.channel, r.cc, sumi_ctl_t(rawValue: r.target)) }
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
        // step 63: every keyed layout plays — the three of Phase 4 and Phase 9's five
        let playable = layout == 1 || layout == 2 || layout == 5 || (layout >= 8 && layout <= 12)
        let effective = playModeRequested && playable
        fingeringPanel.setMode(layout == 8 ? .valves : (layout == 9 ? .slide : .none))
        fingeringPanel.isHidden = !effective || !(layout == 8 || layout == 9)
        layoutPlaySurface()
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
                self.push(inst, cfg[i].status, cfg[i].data1, cfg[i].data2, local: false)   // the session's controls, not the surface
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
                var am = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 12)
                let an = hostmpe_strip_announce(se, &am, 12)   // step 62: nine — the fingering follows the five
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
            self.push(inst, m[i].status, m[i].data1, m[i].data2)
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

    /// midiQueue only. A voice's messages: the loopback full-rate, outbound under the policy.
    private func voiceDispatch(_ m: [hostmpe_msg_t], count: UInt32, exempt: Bool) {
        guard count > 0, let inst else { return }
        let now = CACurrentMediaTime()
        for i in 0..<Int(count) {
            logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
            self.push(inst, m[i].status, m[i].data1, m[i].data2)
            outputs?.send(m[i], exempt: exempt, now: now)
        }
    }

    // -- Phase 9 step 63: the fingering on the strip (DECISIONS_8 #9, #13) ---

    func stripValveDown(_ valve: Int32) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_valve_press(se, valve, &m, 2)
            self.stripDispatch(m, count: n, exempt: true)   // a button: never dropped
            self.fingeringChanged(se)
        }
    }
    func stripValveUp(_ valve: Int32) {
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_valve_release(se, valve, &m, 2)
            self.stripDispatch(m, count: n, exempt: true)
            self.fingeringChanged(se)
        }
    }
    func stripSlideSet(_ position: Float) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
            let n = hostmpe_strip_slide_set(se, position, &m, 2)
            self.stripDispatch(m, count: n, exempt: false)   // continuous: policed
            self.fingeringChanged(se)
        }
    }
    /// midiQueue only: the mirror follows the strip's engine, and every held brass voice RETUNES by
    /// what the probe now says of its cell — the trumpet over the 30 ms ramp (hostmpe_tick carries it),
    /// the trombone at once, the hand being the ramp (DECISIONS_8 #10).
    private func fingeringChanged(_ se: OpaquePointer) {
        let old = fingeringQ
        var st = sumi_layout_state_t()
        st.buttons = hostmpe_strip_valves(se)
        st.slider = hostmpe_strip_slide_value(se)
        fingeringQ = st
        if let mpe {
            var p = paramsQ
            let layout = p.pitch_layout
            for (voice, cell) in heldCells where !cell.theremin {
                var delta: Float = 0
                if layout == 9 {
                    delta = -(st.slider - old.slider) * 6.0
                } else if layout == 8 {
                    var o = old, n2 = st
                    var now = sumi_cell_info_t(), was = sumi_cell_info_t()
                    guard sumi_layout_probe(layout, &p, aspectQ, &n2, cell.cx, cell.cy, &now),
                          sumi_layout_probe(layout, &p, aspectQ, &o, cell.cx, cell.cy, &was) else { continue }
                    delta = Float(Int(now.note) - Int(was.note))
                } else { continue }
                if delta == 0 { continue }
                var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 2)
                let n = hostmpe_voice_retune(mpe, voice, CACurrentMediaTime(), delta, layout == 8 ? HOSTMPE_RETUNE_S : 0, &m, 2)
                voiceDispatch(m, count: n, exempt: false)
            }
        }
        let copy = st
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.fingering = copy
            self.fingeringPanel.syncFingering(valves: copy.buttons, slide: copy.slider)
        }
    }

    /// The Next pad: the layout after the current one in the chosen subset (QOL §2).
    func stripQuickNext() {
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            let next = hostmpe_strip_quick_next(se, self.paramsQ.pitch_layout)
            DispatchQueue.main.async { self.session?.params.pitch_layout = next }
        }
    }
    func setQuickSwitch(_ ids: String) {
        let parsed = ids.split(separator: ",").compactMap { UInt32($0) }
        guard parsed != quickIds else { return }
        quickIds = parsed
        midiQueue.async { [weak self] in
            guard let self, let se = self.stripEngine else { return }
            var arr = parsed
            hostmpe_strip_quick_set(se, &arr, UInt32(arr.count))
        }
        strip.setQuickAvailable(!parsed.isEmpty)
        layoutPlaySurface()
    }
    /// Left-handed (QOL §2): the overlay mirrors the lattice and the touches' x, hostmpe the hand's
    /// horizontal delta, the strip moves to the other corner.
    func setMirror(_ on: Bool) {
        guard on != mirrored else { return }
        mirrored = on
        overlay.setMirror(on)
        fingeringPanel.setMirror(on)
        midiQueue.async { [weak self] in
            guard let self, let mpe = self.mpe else { return }
            hostmpe_set_mirror(mpe, on)
        }
        layoutPlaySurface()
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
            let valves = hostmpe_strip_valves(se), slide = hostmpe_strip_slide_value(se)
            DispatchQueue.main.async {
                self.strip.syncMirrors(pitch: pitch, latch: latch, sustain: sus,
                                       toggleMode: toggle, ccs: ccs)
                self.fingeringPanel.syncFingering(valves: valves, slide: slide)
            }
        }
    }

    // -- Play-mode touch path (overlay -> hostmpe -> loopback) ---------------

    /// Returns the allocated voice (member channel) or -1 on saturation.
    /// Synchronous hop onto the MIDI queue: allocation must answer before the
    /// overlay can track the touch, and the calls are microseconds.
    func playTouchBegin(note: UInt8, velocity: UInt8, rMax: Float,
                        gradX: Float, gradY: Float,
                        offset: Float = 0, cellX: Float = 0, cellY: Float = 0) -> Int32 {
        markActivity()
        latencyMarks.append(CACurrentMediaTime())
        VoxoSpike.shared.markTouchDown(voxo_now_seconds())   // step 48: the spike's touch reference, on Voxo's clock
        let state = signposter.beginInterval("touch-to-render")
        defer { signposter.endInterval("touch-to-render", state) }
        var voice: Int32 = -1
        midiQueue.sync { [self] in
            guard let inst, let mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            var n: UInt32 = 0
            let now = CACurrentMediaTime()
            // step 63: the attack may begin between semitones (the trombone's slide): the first bend is
            // the fraction's, the attack in tune (DECISIONS_8 #10)
            voice = hostmpe_touch_begin_offset(mpe, now, note, velocity,
                                               rMax, gradX, gradY, offset, &m, 4, &n)
            for i in 0..<Int(n) {
                logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
                outputs?.send(m[i], exempt: true, now: now)   // strike: never decimated
            }
            if voice >= 0 { heldCells[voice] = HeldCell(cx: cellX, cy: cellY, note: note, theremin: false) }
        }
        return voice
    }

    // -- Phase 9 step 63: the theremin surface (DECISIONS_8 #11) -------------

    /// The hand lands: the probe's note under it and the fraction between semitones; the attack at that pitch.
    func playThereminBegin(note: UInt8, offset: Float, velocity: UInt8, rMax: Float) -> Int32 {
        markActivity()
        latencyMarks.append(CACurrentMediaTime())
        var voice: Int32 = -1
        midiQueue.sync { [self] in
            guard let inst, let mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
            var n: UInt32 = 0
            let now = CACurrentMediaTime()
            voice = hostmpe_theremin_begin(mpe, now, note, offset, velocity, rMax, &m, 4, &n)
            for i in 0..<Int(n) {
                logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
                outputs?.send(m[i], exempt: true, now: now)
            }
            if voice >= 0 { heldCells[voice] = HeldCell(cx: 0, cy: 0, note: note, theremin: true) }
        }
        return voice
    }
    /// The hand moves: pitch set absolutely (a re-anchor past 47 semitones ships WHOLE — it holds a
    /// Note On), Y the bipolar press.
    func playThereminMove(voice: Int32, note: UInt8, offset: Float, dy: Float) {
        markActivity()
        midiQueue.async { [weak self] in
            guard let self, let inst = self.inst, let mpe = self.mpe else { return }
            var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 8)
            let n = hostmpe_theremin_move(mpe, voice, note, offset, dy, &m, 8)
            let now = CACurrentMediaTime()
            var whole = false
            for i in 0..<Int(n) where (m[i].status & 0xF0) == 0x90 { whole = true }
            for i in 0..<Int(n) {
                self.logByte(m[i].status, m[i].data1, m[i].data2, src: 1)
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: whole, now: now)
            }
        }
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
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
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
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: true, now: now)   // lift: never decimated
            }
            self.heldCells.removeValue(forKey: voice)
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
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
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
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
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
                    self.push(inst, m[i].status, m[i].data1, m[i].data2)
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
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: false, now: now)
            }
        }
    }

    /// Gesture-ABI passes — main thread IS the render thread on iOS (§5.2).
    func addWake(x0: Float, y0: Float, x1: Float, y1: Float, tip: Float) {
        gWake(x0, y0, x1, y1, tip)
    }

    func penPinch(x: Float, y: Float, k: Float, angle: Float) {
        gPinch(x, y, k, angle, 2 * VORTEX_RADIUS)   // #75: Anod the burst (the pen has no finger span)
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
        recordDip()   // step 65
        NSLog("[dip] paper dip from settings (kept in the ledger)")
    }
    func clearCanvas() {
        guard let inst else { return }
        if let ledger { ledger.clear(inst) } else { sumi_trigger_paper_dip(inst) }
        recordDip()   // step 65
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
                self.push(inst, m[i].status, m[i].data1, m[i].data2)
                self.outputs?.send(m[i], exempt: true, now: now)   // never decimated
            }
            self.heldCells.removeAll()
            // step 63: the strip's half — sustain off, the valves up, the spring home (DECISIONS_8 #9)
            if let se = self.stripEngine {
                var r = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 8)
                let k = hostmpe_strip_reset(se, &r, 8)
                self.stripDispatch(r, count: k, exempt: true)
                self.fingeringChanged(se)
            }
            NSLog("[panic] released all voices, %d messages", Int(n))
            DispatchQueue.main.async { self.syncStripMirrors() }
        }
    }

    // -- Phase 9 step 63: the fingering demo (the evidence's scripted phrase) --

    /// A scripted phrase through the REAL path on the current layout — the trumpet's valves under a
    /// held partial (valve legato), the trombone's slide out under a held partial (the glissando), the
    /// theremin's sweep, a scale elsewhere — so the byte log carries what a hand would send. The cells
    /// come from the probe (one source of truth); the valves and the slide from the strip's engines.
    func runFingeringDemo() {
        guard !demoRunning, bounds.height > 0 else { return }
        demoRunning = true
        var p = paramsSnapshot
        let layout = p.pitch_layout
        let aspect = Float(bounds.width / bounds.height)
        var st = fingering
        // a note -> its cell, under the state now (a coarse probe sweep, like the overlay's)
        func cellOf(_ note: UInt8, _ state: sumi_layout_state_t) -> sumi_cell_info_t? {
            var s = state
            var info = sumi_cell_info_t()
            for iy in 0..<54 { for ix in 0..<96 {
                let x = (Float(ix) + 0.5) / 96, y = (Float(iy) + 0.5) / 54
                if sumi_layout_probe(layout, &p, aspect, &s, x, y, &info), info.note == note { return info }
            } }
            return nil
        }
        var steps: [(Double, () -> Void)] = []
        var t = 0.0
        func at(_ dt: Double, _ f: @escaping () -> Void) { t += dt; steps.append((t, f)) }
        var voice: Int32 = -1
        NSLog("[demo] fingering demo on layout %u", layout)
        if layout == 8 {
            // B♭3 open, held; valve 2 down (A3), 1+2 (A♭3), 1+2+3 (E3) — the legato; up; lift
            at(0.0) { [self] in if let c = cellOf(70, st) { voice = playTouchBegin(note: c.note, velocity: 96, rMax: c.cell_radius, gradX: c.semitone_dx / c.semitone_step, gradY: c.semitone_dy / c.semitone_step, offset: 0, cellX: c.cell_center_x, cellY: c.cell_center_y) } }
            at(0.3) { [self] in playTouchUpdate(voice: voice, dx: 0, dy: -0.03) }
            at(0.5) { [self] in stripValveDown(1) }
            at(0.6) { [self] in stripValveDown(0) }
            at(0.6) { [self] in stripValveDown(2) }
            at(0.8) { [self] in playTouchEnd(voice: voice, lift: 64) }
            at(0.2) { [self] in stripValveUp(0); stripValveUp(1); stripValveUp(2) }
            // then the open series up, one cell each
            for n in [58, 65, 74, 77, 82] as [UInt8] {
                at(0.35) { [self] in st = fingering; if let c = cellOf(n, st) { voice = playTouchBegin(note: c.note, velocity: 96, rMax: c.cell_radius, gradX: c.semitone_dx / c.semitone_step, gradY: c.semitone_dy / c.semitone_step, offset: 0, cellX: c.cell_center_x, cellY: c.cell_center_y) } }
                at(0.3) { [self] in playTouchEnd(voice: voice, lift: 64) }
            }
        } else if layout == 9 {
            // the slide in; B♭3's partial held; the slide out to the 7th over two seconds; lift
            at(0.0) { [self] in stripSlideSet(0) }
            at(0.2) { [self] in st = fingering; if let c = cellOf(70, st) { voice = playTouchBegin(note: c.note, velocity: 96, rMax: c.cell_radius, gradX: c.semitone_dx / c.semitone_step, gradY: c.semitone_dy / c.semitone_step, offset: 0, cellX: c.cell_center_x, cellY: c.cell_center_y) } }
            at(0.3) { [self] in playTouchUpdate(voice: voice, dx: 0, dy: -0.03) }
            for i in 1...40 { at(0.05) { [self] in stripSlideSet(Float(i) / 40.0) } }
            at(0.4) { [self] in playTouchEnd(voice: voice, lift: 64) }
            at(0.3) { [self] in stripSlideSet(0) }
        } else if layout == 12 {
            // the hand lands at C4 and slides two octaves across the field in two seconds, pushing up
            var info = sumi_cell_info_t()
            let x0: Float = 0.08 + (24.5 / 61.0) * 0.84   // C4's slot
            at(0.0) { [self] in
                var s = st
                if sumi_layout_probe(layout, &p, aspect, &s, x0, 0.5, &info) {
                    let off = (x0 - info.cell_center_x) * aspect / info.semitone_step
                    voice = playThereminBegin(note: info.note, offset: off, velocity: 96, rMax: info.cell_radius)
                }
            }
            for i in 1...80 {
                at(0.025) { [self] in
                    var s = st
                    let x = x0 + (24.0 / 61.0) * 0.84 * Float(i) / 80.0
                    if sumi_layout_probe(layout, &p, aspect, &s, x, 0.5, &info) {
                        let off = (x - info.cell_center_x) * aspect / info.semitone_step
                        playThereminMove(voice: voice, note: info.note, offset: off, dy: -0.25 * Float(i) / 80.0)
                    }
                }
            }
            at(0.3) { [self] in playTouchEnd(voice: voice, lift: 64) }
        } else {
            // a C major scale, one cell each
            for n in [60, 62, 64, 65, 67, 69, 71, 72] as [UInt8] {
                at(0.3) { [self] in if let c = cellOf(n, st) { voice = playTouchBegin(note: c.note, velocity: 96, rMax: c.cell_radius, gradX: c.semitone_dx / c.semitone_step, gradY: c.semitone_dy / c.semitone_step, offset: 0, cellX: c.cell_center_x, cellY: c.cell_center_y) } }
                at(0.25) { [self] in playTouchEnd(voice: voice, lift: 64) }
            }
        }
        at(0.5) { [self] in demoRunning = false; flushLogsNow(); NSLog("[demo] done") }
        for (when, f) in steps { DispatchQueue.main.asyncAfter(deadline: .now() + when, execute: f) }
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
                        self.push(inst, m[i].status, m[i].data1, m[i].data2)
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
                            self.push(inst, m[k].status, m[k].data1, m[k].data2)
                            self.outputs?.send(m[k], exempt: true, now: now)
                        }
                    }
                    var m = [hostmpe_msg_t](repeating: hostmpe_msg_t(), count: 4)
                    var n: UInt32 = 0
                    voices[i] = hostmpe_touch_begin(mpe, now, UInt8(48 + i * 3), 96,
                                                    0.0571, 1.0 / 0.1244, 0.0, &m, 4, &n)
                    for k in 0..<Int(n) {
                        self.push(inst, m[k].status, m[k].data1, m[k].data2)
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
                    self.push(inst, m[k].status, m[k].data1, m[k].data2)
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
            gWake(Float(last.x / bounds.width), Float(last.y / bounds.height),
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
        gTap(x, y, DROP_RADIUS)   // #75: the medium's tap (Sumi the drop, Anod the strike); step 65: recorded
    }

    @objc private func onPress(_ g: UILongPressGestureRecognizer) {
        guard let inst else { return }
        markActivity()
        let loc = g.location(in: self)
        switch g.state {
        case .began:
            let (x, y) = norm(loc)
            gTap(x, y, DROP_RADIUS)   // #75: the press starts as a tap
            press = PressState(x: x, y: y, R: DROP_RADIUS, cy0: loc.y, cy: loc.y)
        case .changed:
            press?.cy = loc.y
        default:
            if press != nil { gPressEnd() }   // #75: lets go of a stir it set
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
        pr.R = gPress(pr.x, pr.y, pr.R, up, down, dt)
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
                gTine(x0, y0, x1, y1, TINE_ALPHA, lengthAC(panLast, p))
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
            gPinch(x, y, dk, angle, span)   // #75: Anod the burst
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
            gTwist(x, y, strength, VORTEX_RADIUS, paramsSnapshot.vortex_profile)   // #75: Anod the torsion vortex
        default:
            break
        }
    }
}

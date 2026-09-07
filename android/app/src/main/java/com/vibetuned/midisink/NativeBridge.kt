package com.vibetuned.midisink

import android.media.midi.MidiDevice
import android.view.Surface

/**
 * The JNI face of android/cpp/sumi_jni.cpp + sumi_play.cpp. Threading
 * contract (§5.2): every call is safe from any thread — the native side
 * marshals sumi_* work onto its render thread and hostmpe work onto the
 * AMidi poller thread (the single producer, DECISIONS_2 #33); only
 * nativeSurfaceDestroyed BLOCKS (the §5.4 teardown contract), nativeFieldDump
 * and nativeRunSelfTests block their (worker) caller, and the touch-down /
 * pen-down / strip-state calls are microsecond sync hops onto the MIDI
 * thread (the voice id must answer before the overlay can track the touch,
 * DECISIONS_3 #14).
 */
object NativeBridge {
    init {
        System.loadLibrary("sumi-shell")
    }

    external fun nativeInit(filesDir: String)
    external fun nativeShutdown()

    external fun nativeSurfaceCreated(surface: Surface, density: Float)
    external fun nativeSurfaceChanged(width: Int, height: Int, density: Float)
    /** Blocks until the render thread has released the surface (§5.4). */
    external fun nativeSurfaceDestroyed()

    // -- Marble-mode gestures + the v0.4 gesture ABI (render thread via post) --
    external fun nativeAddDrop(x: Float, y: Float)
    external fun nativeAddTine(x0: Float, y0: Float, x1: Float, y1: Float, magnitude: Float)
    external fun nativeAddVortex(x: Float, y: Float, strength: Float)
    /** v0.4 dipolar wake — physical, never MIDI (PROJECT_SPEC.md §8.7 invariant). */
    external fun nativeAddWake(x0: Float, y0: Float, x1: Float, y1: Float, tip: Float)
    /** v0.4 pinch: fold axis in radians (atan2 convention, y down), k = DELTA. */
    external fun nativeAddPinch(x: Float, y: Float, k: Float, angle: Float)
    /** v0.6 pressure gesture (DECISIONS_4 #49): grow the drop under (x, y) by a pass of
     *  radius r (sumi_add_drop FEED) / stir with the Lamb-Oseen swirl of core rc and
     *  strength s = Γ·Δt (sumi_add_vortex LAMB_OSEEN). */
    external fun nativeAddFeed(x: Float, y: Float, r: Float)
    external fun nativeAddSwirl(x: Float, y: Float, s: Float, rc: Float)
    external fun nativeTriggerDip()
    /** Paper dip that hands the PRINT to Kotlin (settings sheet): drains any
     *  unconsumed print first (the core keeps two buffers and refuses a third
     *  dip while both are busy), dips, and — once the async readback lands —
     *  either parks [w, h, ARGB…] for nativeTakePrint() (keep) or frees it. */
    external fun nativeDipForPrint(keep: Boolean)
    /** The parked print as [w, h, ARGB…] once, or null while none is ready. */
    external fun nativeTakePrint(): IntArray?

    // -- host-owned params (UI snapshot + render-thread apply) -----------------
    external fun nativeSetSimScale(simScale: Float, whyThermal: Int)
    external fun nativeSetThermal(status: Int)
    external fun nativeSetLayout(layout: Int)
    // v0.4: CC74 routing (0 hue, 1 pinch) + pinch look (0 saddle, 1 crossed).
    external fun nativeSetSlidePinch(slideMode: Int, pinchVariant: Int)
    /** v0.7 (DECISIONS_4 #53): the stylus wake's fluid — 0 inviscid doublet, 1 the viscous
     *  2-D Stokeslet stroke; spread = l/a in [1.5, 12]. */
    external fun nativeSetWakeProfile(profile: Int, spread: Float)
    // #56: the desktop settings window's remaining rows — same ranges.
    external fun nativeSetLook(palette: Int, viscosity: Float, feed: Float, roughness: Float,
                               bpm: Float, rollSpeed: Float)
    external fun nativeSetVortexProfile(profile: Int)          // 0 exponential, 1 Rankine
    external fun nativeSetRippleAngle(degrees: Float)          // 0..180
    /** CC map as (channel, cc, target) triples, channel 0xFF = any; empty = default map. */
    external fun nativeSetCcMap(triples: IntArray)
    /** A settings slider riding a CC (ripple amount/wavelength): loopback only. */
    external fun nativeSendCC(cc: Int, value: Int)
    // v0.4: per-note bend routing (0 glide, 1 sine ripple; bake rides along).
    external fun nativeSetBendMode(mode: Int)
    // v0.4: 0xD0 hardware routing (0 ink feed, 1 Lamb–Oseen swirl).
    external fun nativeSetPressMode(mode: Int)

    // -- MIDI ingest ------------------------------------------------------------
    /** `deviceId` is MidiDeviceInfo.getId(), so the ports can be closed again. */
    external fun nativeAddMidiDevice(device: MidiDevice, deviceId: Int)
    /** A device left: close its ports and drop them from the poller. */
    external fun nativeRemoveMidiDevice(deviceId: Int)
    /** A device left: external occupancy on its channels clears (§5.1). */
    external fun nativeExternalClear()

    // -- Phase 4 play surface (all hostmpe work on the MIDI thread) -----------
    external fun nativeSetPlayMode(effective: Boolean)
    /** Returns the member channel (1..15) or -1 on saturation (silent drop). */
    external fun nativeTouchBegin(tDown: Double, note: Int, velocity: Int,
                                  rMax: Float, gradX: Float, gradY: Float): Int
    external fun nativeTouchUpdate(voice: Int, dx: Float, dy: Float)
    external fun nativeTouchEnd(voice: Int, lift: Int)
    external fun nativePenBegin(tDown: Double, note: Int, velocity: Int): Int
    /** Same allocator release as a finger; logged as the stylus's (src 4). */
    external fun nativePenEnd(voice: Int, lift: Int)
    external fun nativePenGlide(voice: Int, note: Int, offset: Float, scale: Float, velocity: Int)
    external fun nativePenSlide(voice: Int, eff: Float, outboundOnly: Boolean)
    external fun nativePenPressure(voice: Int, force: Float)

    // -- control strip (§8) -----------------------------------------------------
    external fun nativeStripPitchMove(v: Float)
    external fun nativeStripPitchRelease()
    external fun nativeStripLatchMove(wheel: Int, delta: Float)
    external fun nativeStripSustainDown()
    external fun nativeStripSustainUp()
    external fun nativeStripSustainMode(toggle: Boolean)
    /** Returns the wheel's CC after the request; -1 when refused (protocol CC). */
    external fun nativeStripAssign(wheel: Int, cc: Int): Int
    /** out[8] = pitch, latch0..2, sustain(0/1), cc0..2. */
    external fun nativeStripState(out: FloatArray)

    // -- transports (§5.4) --------------------------------------------------------
    external fun nativeSetTransports(usb: Boolean, virtual: Boolean, ble: Boolean)
    external fun nativeSinkAppeared(sink: Int)
    external fun nativeResyncSession()
    external fun nativePanic()
    external fun nativeStartStorm(seconds: Int)
    external fun nativeFlushLogs()
    external fun nativeStatusLine(): String

    // -- geometry (instance-free probe, any thread) ------------------------------
    /** out[7] = note, cx, cy, r, semitone_dx, semitone_dy, semitone_step. */
    external fun nativeLayoutProbe(x: Float, y: Float, aspect: Float, out: FloatArray): Boolean
    /** [note, cx, cy, r] per unique cell — the lattice IS a probe sweep. */
    external fun nativeLatticeSweep(aspect: Float, nx: Int, ny: Int): FloatArray
    external fun nativeJoystickEff(dx: Float, dy: Float, rMax: Float, out: FloatArray)

    // -- evidence hooks -----------------------------------------------------------
    external fun nativeStartStress(minutes: Int)
    external fun nativeFieldDump(path: String): Boolean
    external fun nativeDroppedMidi(): Int
    /** sumi_version(): (major << 16) | (minor << 8) | patch — the engine ABI, for About. */
    external fun nativeCoreVersion(): Int
    /** Runs the hostmpe + normalizer suites on the caller's thread; bit mask of failures. */
    external fun nativeRunSelfTests(path: String): Int

    /**
     * Upcall from the native MIDI thread: raw MIDI 1.0 bytes for ONE sink
     * (0 USB gadget, 1 virtual device, 2 BLE peripheral), already policed by
     * that sink's limiter. Returns true when a live endpoint took them.
     */
    @JvmStatic
    fun outboundWrite(sink: Int, bytes: ByteArray, len: Int): Boolean =
        MidiOutputs.write(sink, bytes, len)
}

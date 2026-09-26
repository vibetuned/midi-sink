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

    // -- Marble-mode gestures (render thread via post) ---------------------------
    external fun nativeAddTine(x0: Float, y0: Float, x1: Float, y1: Float, magnitude: Float)
    /** v0.4 dipolar wake — physical, never MIDI (PROJECT_SPEC.md §8.7 invariant). */
    external fun nativeAddWake(x0: Float, y0: Float, x1: Float, y1: Float, tip: Float)
    // 1.1 (DECISIONS_5 #75): the medium-aware gestures — the core plays the medium's table.
    external fun nativeGestureTap(x: Float, y: Float)
    /** k = the squeeze DELTA, angle the finger axis, span the finger distance in canvas heights (0 = a pen). */
    external fun nativeGesturePinch(x: Float, y: Float, k: Float, angle: Float, span: Float)
    external fun nativeGestureTwist(x: Float, y: Float, strength: Float)
    /** The long press: its first touch is a tap; then one frame per vsync; then the release. */
    external fun nativeGesturePressBegin(x: Float, y: Float)
    external fun nativeGesturePressFrame(up: Float, down: Float, dt: Float)
    external fun nativeGesturePressEnd()
    external fun nativeTriggerDip()

    // -- step 45b (DECISIONS_5 #79): THE SESSION through the one serializer ------
    /** Before the instance: the last session's text and, when there is none, a 0.x migration patch. */
    external fun nativeSessionInit(session: String?, legacyPatch: String?)
    /** The session as JSON (named, when name != null); "" until the instance has seeded it. */
    external fun nativeSessionJson(name: String?): String
    /** A JSON patch read OVER the session (a whole preset file is a patch too); the session after, or "". */
    external fun nativeSessionPatch(patch: String): String
    /** Palette library entry `index` of `medium` as a preset JSON (its name and palette); "" past the end. */
    external fun nativePalettePreset(medium: Int, index: Int): String
    external fun nativePalettePresetCount(medium: Int): Int

    // -- the print ledger (QOL §4) --------------------------------------------------
    /** keep = dip the paper, keep the print; false = clear the canvas, discard. */
    external fun nativeLedgerDip(keep: Boolean)
    /** [count, then per entry id, fw, fh, medium, printSeen, tw, th, when, pw, ph] newest last. */
    external fun nativeLedgerList(): IntArray
    /** RGBA8 of an entry's thumbnail (which 0) or the newest print (which 1). */
    external fun nativeLedgerPixels(id: Int, which: Int): ByteArray?
    external fun nativeLedgerExport(id: Int, w: Int, h: Int, alpha: Boolean): Boolean
    external fun nativeLedgerExportSize(): IntArray?
    external fun nativeLedgerExportTake(dst: java.nio.ByteBuffer): Boolean
    external fun nativeLedgerStatus(): String

    // -- host-owned params ------------------------------------------------------------
    /** The thermal listener owns sim_scale (DECISIONS #31); everything else is the session's. */
    external fun nativeSetSimScale(simScale: Float, whyThermal: Int)
    external fun nativeSetThermal(status: Int)

    // -- MIDI ingest ------------------------------------------------------------
    /** `deviceId` is MidiDeviceInfo.getId(), so the ports can be closed again. */
    external fun nativeAddMidiDevice(device: MidiDevice, deviceId: Int)
    /** A device left: close its ports and drop them from the poller. */
    external fun nativeRemoveMidiDevice(deviceId: Int)
    /** A device left: external occupancy on its channels clears (§5.1). */
    external fun nativeExternalClear()

    // -- Phase 4 play surface (all hostmpe work on the MIDI thread) -----------
    external fun nativeSetPlayMode(effective: Boolean)
    /** Phase 7 step 48: the Voxo latency spike — blocks its worker caller for `seconds`. */
    external fun nativeVoxoSpike(seconds: Int)
    // Phase 7 step 54: the product side of Voxo (Sound.kt).
    external fun nativeVoxoSetEnabled(on: Boolean): Boolean
    external fun nativeVoxoSetGain(gain: Float)
    external fun nativeVoxoSetLocalControl(on: Boolean)
    external fun nativeVoxoSetBudget(bytes: Long)
    /** Blocks the (worker) caller; "OK\n<report>" or "ERR\n<why>". */
    external fun nativeVoxoLoad(path: String): String
    external fun nativeVoxoUnload()
    /** "<line>|1" while running, "stopped|0" otherwise; polling it paces the AAudio buffer tuner. */
    external fun nativeVoxoStatus(): String
    /** 128 bits of the loaded instrument's reach, or null without a preset. */
    external fun nativeVoxoCoveredNotes(): ByteArray?
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

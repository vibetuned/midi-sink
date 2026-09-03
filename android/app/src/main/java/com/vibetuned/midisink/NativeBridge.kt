package com.vibetuned.midisink

import android.media.midi.MidiDevice
import android.view.Surface

/**
 * The JNI face of android/cpp/sumi_jni.cpp. Threading contract (§5.2): every
 * call is safe from any thread — the native side marshals sumi_* work onto
 * its render thread; only nativeSurfaceDestroyed BLOCKS (the §5.4 teardown
 * contract) and nativeFieldDump blocks its (worker) caller until the dump is
 * written.
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

    external fun nativeAddDrop(x: Float, y: Float)
    external fun nativeAddTine(x0: Float, y0: Float, x1: Float, y1: Float, magnitude: Float)
    external fun nativeAddVortex(x: Float, y: Float, strength: Float)
    external fun nativeTriggerDip()

    external fun nativeSetSimScale(simScale: Float, whyThermal: Int)
    external fun nativeSetThermal(status: Int)
    external fun nativeSetLayout(layout: Int)
    // v0.4: CC74 routing (0 hue, 1 pinch) + pinch look (0 saddle, 1 crossed).
    external fun nativeSetSlidePinch(slideMode: Int, pinchVariant: Int)
    // v0.4: master-bend routing (0 shear tine, 1 sine-ripple wavelength).
    external fun nativeSetBendMode(mode: Int)

    external fun nativeAddMidiDevice(device: MidiDevice)

    external fun nativeStartStress(minutes: Int)
    external fun nativeFieldDump(path: String): Boolean
    external fun nativeDroppedMidi(): Int
}

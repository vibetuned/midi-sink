package com.vibetuned.midisink

import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import kotlin.concurrent.thread
import kotlin.math.atan2
import kotlin.math.sqrt

private const val TAG = "sumi-shell"

class MainActivity : ComponentActivity() {

    private lateinit var midi: MidiInputs
    private val showSettings = mutableStateOf(false)
    private val showPairing = mutableStateOf(false)
    private val currentLayout = mutableStateOf(0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        NativeBridge.nativeInit(filesDir.absolutePath)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        midi = MidiInputs(this)
        midi.start()

        registerThermalListener()
        handleDebugIntent()

        setContent {
            Box(Modifier.fillMaxSize()) {
                AndroidView(
                    factory = { ctx -> SumiSurfaceView(ctx) },
                    modifier = Modifier.fillMaxSize()
                )
                // Minimal chrome: one translucent gear opening the settings
                // menu (layout picker + Bluetooth MIDI pairing — the iOS
                // settings sheet's Android sibling); everything else is the
                // canvas.
                BasicText(
                    "⚙",
                    style = TextStyle(color = Color(0x88FFFFFF), fontSize = 26.sp),
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .padding(10.dp)
                        .clickable { showSettings.value = true }
                        .padding(8.dp)
                )
                if (showSettings.value) {
                    SettingsDialog(
                        currentLayout = currentLayout.value,
                        onLayout = { id ->
                            currentLayout.value = id
                            NativeBridge.nativeSetLayout(id)
                        },
                        onPairBluetooth = {
                            showSettings.value = false
                            showPairing.value = true
                        },
                        onDismiss = { showSettings.value = false })
                }
                if (showPairing.value) {
                    BluetoothMidiPairingDialog(
                        midi = midi,
                        onDismiss = { showPairing.value = false })
                }
            }
        }
    }

    override fun onDestroy() {
        if (isFinishing) {
            midi.stop()
            NativeBridge.nativeShutdown()
        }
        super.onDestroy()
    }

    /** Host-side thermal policy (step brief): sim_scale 0.75 baseline, 0.6
     *  under THERMAL_STATUS_SEVERE, back at MODERATE or below. The core
     *  never detects devices — this is the host's knob (§ params comment). */
    private fun registerThermalListener() {
        val pm = getSystemService(PowerManager::class.java) ?: return
        var degraded = false
        pm.addThermalStatusListener(mainExecutor) { status ->
            NativeBridge.nativeSetThermal(status)
            if (!degraded && status >= PowerManager.THERMAL_STATUS_SEVERE) {
                degraded = true
                Log.i(TAG, "thermal SEVERE -> sim_scale 0.6")
                NativeBridge.nativeSetSimScale(0.6f, status)
            } else if (degraded && status <= PowerManager.THERMAL_STATUS_MODERATE) {
                degraded = false
                Log.i(TAG, "thermal recovered -> sim_scale 0.75")
                NativeBridge.nativeSetSimScale(0.75f, status)
            }
        }
    }

    /** Evidence hooks (step-14 DONE): `--es fieldDump 1` writes the §4.6
     *  canonical dump to app files; `--ei stressMinutes N` runs the Osmose
     *  stress feeder for N minutes. Driven via `adb shell am start`. */
    private fun handleDebugIntent() {
        if (intent?.getStringExtra("fieldDump") != null) {
            thread(name = "field-dump") {
                val path = "$filesDir/field_512_gles3.bin"
                var ok = false
                for (attempt in 1..60) {   // wait for the first surface/instance
                    ok = NativeBridge.nativeFieldDump(path)
                    if (ok) break
                    Thread.sleep(250)
                }
                Log.i(TAG, if (ok) "FIELD_DUMP_DONE $path" else "FIELD_DUMP_FAILED")
            }
        }
        val minutes = intent?.getIntExtra("stressMinutes", 0) ?: 0
        if (minutes > 0) {
            thread(name = "stress-arm") {
                Thread.sleep(3000)   // let the surface/instance come up
                Log.i(TAG, "starting stress feeder: $minutes min")
                NativeBridge.nativeStartStress(minutes)
            }
        }
    }
}

/** The settings menu — the iOS SettingsSheet's Android sibling (same layout
 *  names, same order; SumiApp.swift is the reference). sim_scale has no
 *  manual toggle here: on Android the thermal listener owns it (step-14
 *  brief — 0.75 baseline, 0.6 under SEVERE). */
@Composable
fun SettingsDialog(
    currentLayout: Int,
    onLayout: (Int) -> Unit,
    onPairBluetooth: () -> Unit,
    onDismiss: () -> Unit,
) {
    val layouts = listOf(
        0 to "Circle of fifths",
        1 to "Chromatic grid",
        2 to "Jankó",
        3 to "Piano roll (horizontal)",
        4 to "Piano roll (vertical)",
        5 to "Piano grid",
    )
    Dialog(onDismissRequest = onDismiss) {
        Column(
            Modifier
                .background(Color(0xEE18143A))
                .padding(20.dp)
                .fillMaxWidth()
        ) {
            BasicText("midi-sink", style = TextStyle(color = Color.White, fontSize = 18.sp))
            BasicText(
                "LAYOUT",
                style = TextStyle(color = Color(0x88FFFFFF), fontSize = 12.sp),
                modifier = Modifier.padding(top = 14.dp, bottom = 4.dp))
            layouts.forEach { (id, name) ->
                BasicText(
                    (if (id == currentLayout) "●  " else "○  ") + name,
                    style = TextStyle(
                        color = if (id == currentLayout) Color.White else Color(0xCCFFFFFF),
                        fontSize = 15.sp),
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { onLayout(id) }
                        .padding(vertical = 9.dp))
            }
            BasicText(
                "MIDI",
                style = TextStyle(color = Color(0x88FFFFFF), fontSize = 12.sp),
                modifier = Modifier.padding(top = 14.dp, bottom = 4.dp))
            BasicText(
                "Pair Bluetooth MIDI instrument…",
                style = TextStyle(color = Color(0xCCFFFFFF), fontSize = 15.sp),
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onPairBluetooth() }
                    .padding(vertical = 9.dp))
            BasicText(
                "Wired and virtual MIDI inputs connect automatically.",
                style = TextStyle(color = Color(0x77FFFFFF), fontSize = 12.sp),
                modifier = Modifier.padding(top = 2.dp))
        }
    }
}

/** The canvas: SurfaceView lifecycle -> JNI render thread, plus the iOS
 *  gesture set (tap = drop, one-finger drag = tine, two-finger twist =
 *  vortex) in §4.6's y-down normalized space (Android view coords match). */
class SumiSurfaceView(context: android.content.Context) : SurfaceView(context),
    SurfaceHolder.Callback {

    // Desktop/iOS gesture constants, verbatim.
    private val dragThresholdPx = 5f * resources.displayMetrics.density
    private val vortexStrengthScale = 1.0f   // iOS: rotationDelta × (4.0/4.0)

    private var lastX = 0f
    private var lastY = 0f
    private var moved = false
    private var twoFinger = false
    private var lastAngle = 0f

    init {
        holder.addCallback(this)
    }

    // Host surface policy: cap the EGL surface at phone-class pixel counts
    // (~2.8M px — a 2400×1080 phone stays native) by integer-halving; the
    // display processor upscales for free. This tablet's 2960×1848 panel
    // (5.5M px) halves once to 1480×924. sim_scale 0.75 then applies to the
    // SURFACE size, keeping the fp16 ping-pong inside mid-range GPU
    // bandwidth (the §5.4 host owns resolution policy; the core never
    // detects devices).
    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        if (w <= 0 || h <= 0) return
        var sw = w
        var sh = h
        while (sw.toLong() * sh > 2_800_000L) {
            sw /= 2
            sh /= 2
        }
        if (sw != w) {
            Log.i(TAG, "surface capped: view ${w}x${h} -> surface ${sw}x${sh}")
            holder.setFixedSize(sw, sh)
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeBridge.nativeSurfaceCreated(holder.surface, resources.displayMetrics.density)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeBridge.nativeSurfaceChanged(width, height, resources.displayMetrics.density)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // §5.4 hard requirement: must not return while the render thread can
        // still touch the surface — the native call blocks until released.
        NativeBridge.nativeSurfaceDestroyed()
    }

    private fun nx(x: Float) = x / width.coerceAtLeast(1)
    private fun ny(y: Float) = y / height.coerceAtLeast(1)
    /** Aspect-corrected length in canvas-height units (desktop segment_len_ac). */
    private fun lengthAC(dxPx: Float, dyPx: Float): Float {
        val h = height.coerceAtLeast(1).toFloat()
        return sqrt((dxPx / h) * (dxPx / h) + (dyPx / h) * (dyPx / h))
    }

    private fun angleBetween(e: MotionEvent): Float =
        atan2(e.getY(1) - e.getY(0), e.getX(1) - e.getX(0))

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = e.x; lastY = e.y
                moved = false; twoFinger = false
            }
            MotionEvent.ACTION_POINTER_DOWN -> if (e.pointerCount == 2) {
                twoFinger = true
                lastAngle = angleBetween(e)
            }
            MotionEvent.ACTION_MOVE -> {
                if (e.pointerCount >= 2) {
                    val a = angleBetween(e)
                    var d = a - lastAngle
                    while (d > Math.PI) d -= (2 * Math.PI).toFloat()
                    while (d < -Math.PI) d += (2 * Math.PI).toFloat()
                    lastAngle = a
                    val strength = (d * vortexStrengthScale).coerceIn(-0.5f, 0.5f)
                    val cx = (e.getX(0) + e.getX(1)) * 0.5f
                    val cy = (e.getY(0) + e.getY(1)) * 0.5f
                    NativeBridge.nativeAddVortex(nx(cx), ny(cy), strength)
                } else if (!twoFinger) {
                    val dx = e.x - lastX
                    val dy = e.y - lastY
                    if (dx * dx + dy * dy >= dragThresholdPx * dragThresholdPx) {
                        NativeBridge.nativeAddTine(
                            nx(lastX), ny(lastY), nx(e.x), ny(e.y), lengthAC(dx, dy))
                        moved = true
                        lastX = e.x; lastY = e.y
                    }
                }
            }
            MotionEvent.ACTION_UP -> {
                if (!moved && !twoFinger) NativeBridge.nativeAddDrop(nx(e.x), ny(e.y))
            }
        }
        return true
    }
}

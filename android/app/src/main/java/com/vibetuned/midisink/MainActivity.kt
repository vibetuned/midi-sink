package com.vibetuned.midisink

import android.Manifest
import android.content.ContentValues
import android.content.Intent
import android.graphics.Bitmap
import android.os.Environment
import android.provider.MediaStore
import android.content.SharedPreferences
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import kotlinx.coroutines.delay
import kotlin.concurrent.thread
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.sqrt

private const val TAG = "sumi-shell"

class MainActivity : ComponentActivity() {

    private lateinit var midi: MidiInputs
    private lateinit var prefs: SharedPreferences
    private lateinit var overlay: PlayOverlayView
    private lateinit var strip: ControlStripView

    private val showSettings = mutableStateOf(false)
    private val showPairing = mutableStateOf(false)
    private val currentLayout = mutableStateOf(0)
    private val slidePinch = mutableStateOf(false)     // v0.4: CC74 -> pinch
    private val pinchCrossed = mutableStateOf(false)   // v0.4: crossed-tine look
    private val bendRipple = mutableStateOf(false)     // v0.4: bend -> ripple
    private val pressSwirl = mutableStateOf(false)     // v0.4: 0xD0 -> swirl
    // Phase 4 §1: Marble (Step-13 gestures) vs Play (virtual MPE surface).
    private val playMode = mutableStateOf(false)
    private val playEffective = mutableStateOf(false)
    private val velocityFromTouchSize = mutableStateOf(false)
    private val sustainToggle = mutableStateOf(false)
    // §5.4 transports: USB gadget is the primary sink.
    private val outUsb = mutableStateOf(true)
    private val outVirtual = mutableStateOf(true)
    private val outBle = mutableStateOf(false)
    private val selfTestResult = mutableStateOf("")
    private var blePermissionPending = false
    /** Held so onDestroy can remove it: each Activity creation would
     *  otherwise add another listener, all driving sim_scale independently. */
    private var thermalListener: PowerManager.OnThermalStatusChangedListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = getSharedPreferences("sumi", MODE_PRIVATE)
        currentLayout.value = prefs.getInt("layout", 0)
        slidePinch.value = prefs.getBoolean("slidePinch", false)
        pinchCrossed.value = prefs.getBoolean("pinchCrossed", false)
        bendRipple.value = prefs.getBoolean("bendRipple", false)
        pressSwirl.value = prefs.getBoolean("pressSwirl", false)
        playMode.value = prefs.getBoolean("playMode", false)
        velocityFromTouchSize.value = prefs.getBoolean("velocityFromTouchSize", false)
        sustainToggle.value = prefs.getBoolean("sustainToggle", false)
        outUsb.value = prefs.getBoolean("outUsb", true)
        outVirtual.value = prefs.getBoolean("outVirtual", true)
        outBle.value = prefs.getBoolean("outBle", false)

        NativeBridge.nativeInit(filesDir.absolutePath)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        overlay = PlayOverlayView(this)
        strip = ControlStripView(this)
        overlay.velocityFromTouchSize = velocityFromTouchSize.value
        overlay.slideMode = if (slidePinch.value) 1 else 0
        // The S-Pen's barrel button drives the strip's sustain engine, so the
        // palette's pad has to follow what the pen did.
        overlay.onSustainChanged = { strip.post { strip.syncMirrors(sustainToggle.value) } }

        // Host-owned params -> the native snapshot (probe ground truth) and
        // the render thread.
        NativeBridge.nativeSetLayout(currentLayout.value)
        NativeBridge.nativeSetSlidePinch(if (slidePinch.value) 1 else 0, if (pinchCrossed.value) 1 else 0)
        NativeBridge.nativeSetBendMode(if (bendRipple.value) 1 else 0)
        NativeBridge.nativeSetPressMode(if (pressSwirl.value) 1 else 0)
        NativeBridge.nativeStripSustainMode(sustainToggle.value)

        midi = MidiInputs(this)
        midi.start()
        MidiOutputs.start(this)
        applyTransports()

        registerThermalListener()
        handleDebugIntent(intent)
        applyMode()

        setContent {
            Box(Modifier.fillMaxSize()) {
                AndroidView(
                    factory = { ctx -> SumiSurfaceView(ctx) },
                    modifier = Modifier.fillMaxSize()
                )
                // Phase 4 §6: the play overlay keeps the FULL bounds so a
                // touched cell and its loopback drop stay exactly aligned
                // (#31); hidden and inert in Marble mode.
                AndroidView(
                    factory = { overlay },
                    modifier = Modifier.fillMaxSize(),
                    update = { it.visibility = if (playEffective.value) View.VISIBLE else View.GONE }
                )
                // §8 rev (#31): the strip is a compact floating palette at the
                // top-left OVER the lattice; it consumes its own touches.
                AndroidView(
                    factory = { strip },
                    modifier = Modifier
                        .align(Alignment.TopStart)
                        .statusBarsPadding()
                        .padding(start = 10.dp, top = 10.dp)
                        .size(300.dp, 86.dp),
                    update = { it.visibility = if (playEffective.value) View.VISIBLE else View.GONE }
                )
                // Minimal chrome: one translucent gear opening the settings
                // menu; everything else is the canvas.
                BasicText(
                    "⚙",
                    style = TextStyle(color = Color(0x88FFFFFF), fontSize = 26.sp),
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .statusBarsPadding()
                        .padding(10.dp)
                        .clickable { showSettings.value = true }
                        .padding(8.dp)
                )
                if (showSettings.value) {
                    SettingsDialog(
                        currentLayout = currentLayout.value,
                        onLayout = { id -> setLayout(id) },
                        playMode = playMode.value,
                        onPlayMode = { setPlayMode(it) },
                        velocityFromTouchSize = velocityFromTouchSize.value,
                        onVelocityFromTouchSize = {
                            velocityFromTouchSize.value = it
                            overlay.velocityFromTouchSize = it
                            prefs.edit().putBoolean("velocityFromTouchSize", it).apply()
                        },
                        sustainToggle = sustainToggle.value,
                        onSustainToggle = {
                            sustainToggle.value = it
                            prefs.edit().putBoolean("sustainToggle", it).apply()
                            NativeBridge.nativeStripSustainMode(it)
                            strip.post { strip.syncMirrors(it) }
                        },
                        slidePinch = slidePinch.value,
                        pinchCrossed = pinchCrossed.value,
                        onSlidePinch = { pinch, crossed ->
                            slidePinch.value = pinch
                            pinchCrossed.value = crossed
                            overlay.slideMode = if (pinch) 1 else 0
                            prefs.edit().putBoolean("slidePinch", pinch)
                                .putBoolean("pinchCrossed", crossed).apply()
                            NativeBridge.nativeSetSlidePinch(if (pinch) 1 else 0, if (crossed) 1 else 0)
                        },
                        bendRipple = bendRipple.value,
                        onBendMode = { ripple ->
                            bendRipple.value = ripple
                            prefs.edit().putBoolean("bendRipple", ripple).apply()
                            NativeBridge.nativeSetBendMode(if (ripple) 1 else 0)
                        },
                        pressSwirl = pressSwirl.value,
                        onPressMode = { swirl ->
                            pressSwirl.value = swirl
                            prefs.edit().putBoolean("pressSwirl", swirl).apply()
                            NativeBridge.nativeSetPressMode(if (swirl) 1 else 0)
                        },
                        outUsb = outUsb.value, outVirtual = outVirtual.value, outBle = outBle.value,
                        onTransports = { usb, virt, ble -> setTransports(usb, virt, ble) },
                        usbStatus = MidiOutputs.usbStatus.value,
                        bleStatus = MidiOutputs.ble?.state?.value ?: "BLE: unavailable",
                        virtualClients = SumiMidiDeviceService.openClientsState.value,
                        onResync = { NativeBridge.nativeResyncSession() },
                        onPanic = { panic() },
                        onStorm = { NativeBridge.nativeStartStorm(60) },
                        onSelfTest = { runSelfTests() },
                        selfTestResult = selfTestResult.value,
                        onPairBluetooth = {
                            showSettings.value = false
                            showPairing.value = true
                        },
                        onPaperDip = { save ->
                            showSettings.value = false   // see the fresh sheet
                            paperDip(save)
                        },
                        onDismiss = {
                            showSettings.value = false
                            NativeBridge.nativeFlushLogs()
                        })
                }
                if (showPairing.value) {
                    BluetoothMidiPairingDialog(
                        midi = midi,
                        onDismiss = { showPairing.value = false })
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleDebugIntent(intent)
    }

    override fun onDestroy() {
        // Unconditional: a non-finishing destroy is reachable (a locale or
        // fontScale change, "don't keep activities", the system reclaiming
        // the instance). Skipping this used to leave a held voice sounding on
        // every sink forever — §5.1's 30 s timeout covers EXTERNAL occupancy
        // only — and left the USB_STATE receiver and the MIDI/thermal
        // callbacks registered against a dead Activity.
        overlay.releaseAll()
        NativeBridge.nativeFlushLogs()
        midi.stop()
        MidiOutputs.stop(this)
        thermalListener?.let {
            getSystemService(PowerManager::class.java)?.removeThermalStatusListener(it)
        }
        thermalListener = null
        // The native side is process-global (one render thread, one MIDI
        // thread): tear it down only when the process's last Activity is
        // actually going away.
        if (isFinishing) NativeBridge.nativeShutdown()
        super.onDestroy()
    }

    // -- mode / params -----------------------------------------------------------

    private fun setLayout(id: Int) {
        currentLayout.value = id
        prefs.edit().putInt("layout", id).apply()
        NativeBridge.nativeSetLayout(id)
        overlay.layoutChanged()
        applyMode()
    }

    private fun setPlayMode(play: Boolean) {
        playMode.value = play
        prefs.edit().putBoolean("playMode", play).apply()
        applyMode()
    }

    /** Play mode is effective only on the playable layouts (grid, Jankó, piano
     *  grid — the probe refuses everything else anyway); Marble mode leaves
     *  the SurfaceView gestures exactly as they shipped. */
    private fun applyMode() {
        val layout = currentLayout.value
        val playable = layout == 1 || layout == 2 || layout == 5
        val effective = playMode.value && playable
        if (effective == playEffective.value) return
        if (!effective) overlay.releaseAll()   // ends any held voices cleanly
        playEffective.value = effective
        Log.i(TAG, "[mode] play=${playMode.value} layout=$layout playable=$playable effective=$effective")
        // Working rule: entering Play mode pushes MCM/RPN0 into the LOOPBACK
        // before any notes (and out every sink), then the strip announces.
        NativeBridge.nativeSetPlayMode(effective)
        if (effective) strip.post { strip.syncMirrors(sustainToggle.value) }
    }

    private fun setTransports(usb: Boolean, virt: Boolean, ble: Boolean) {
        outUsb.value = usb
        outVirtual.value = virt
        var wantBle = ble
        if (ble && !MidiOutputs.hasAdvertisePermissions(this)) {
            // BLE peripheral needs ADVERTISE + CONNECT at runtime on 31+.
            // Ask, and leave it OFF (and unpersisted) until granted — a pref
            // saying "on" for a transport that could not start shows an
            // enabled toggle over a dead sink on the next launch.
            blePermissionPending = true
            requestPermissions(arrayOf(Manifest.permission.BLUETOOTH_ADVERTISE,
                                       Manifest.permission.BLUETOOTH_CONNECT), 72)
            wantBle = false
        }
        outBle.value = wantBle
        prefs.edit().putBoolean("outUsb", usb).putBoolean("outVirtual", virt)
            .putBoolean("outBle", wantBle).apply()
        applyTransports()
    }

    private fun applyTransports() {
        val ble = outBle.value
        val p = MidiOutputs.ble
        if (p != null) {
            if (ble) p.start() else p.stop()
        }
        NativeBridge.nativeSetTransports(outUsb.value, outVirtual.value, ble)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>,
                                            grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 72 && blePermissionPending) {
            blePermissionPending = false
            if (MidiOutputs.hasAdvertisePermissions(this)) {
                outBle.value = true
                prefs.edit().putBoolean("outBle", true).apply()
                applyTransports()
            }
        }
    }

    /** Paper dip from the settings sheet. save = the print goes to
     *  Pictures/midi-sink as PNG through MediaStore (no permission needed on
     *  API 29+, our minSdk); otherwise the sheet is renewed and the print
     *  buffer freed. The readback is asynchronous in the core (§5.3), so the
     *  save waits for it off the UI thread and reports with a toast. */
    private fun paperDip(save: Boolean) {
        NativeBridge.nativeDipForPrint(save)
        if (!save) {
            toast("Fresh sheet")
            return
        }
        thread(name = "print-save") {
            var px: IntArray? = null
            val deadline = System.currentTimeMillis() + 3000
            while (px == null && System.currentTimeMillis() < deadline) {
                px = NativeBridge.nativeTakePrint()
                if (px == null) Thread.sleep(40)
            }
            val msg = if (px == null) "No print — the canvas was not ready" else savePrint(px)
            Log.i("sumi", "[dip] $msg")
            runOnUiThread { toast(msg) }
        }
    }

    private fun savePrint(px: IntArray): String {
        val w = px[0]
        val h = px[1]
        if (w <= 0 || h <= 0 || px.size < 2 + w * h) return "Print not saved (bad readback ${w}x$h)"
        val bmp = Bitmap.createBitmap(px, 2, w, w, h, Bitmap.Config.ARGB_8888)
        val stamp = java.text.SimpleDateFormat("yyyyMMdd-HHmmss", java.util.Locale.US).format(java.util.Date())
        val name = "midi-sink-print-$stamp.png"
        val values = ContentValues().apply {
            put(MediaStore.Images.Media.DISPLAY_NAME, name)
            put(MediaStore.Images.Media.MIME_TYPE, "image/png")
            put(MediaStore.Images.Media.RELATIVE_PATH, Environment.DIRECTORY_PICTURES + "/midi-sink")
            put(MediaStore.Images.Media.IS_PENDING, 1)
        }
        val resolver = contentResolver
        val uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
            ?: return "Print not saved (MediaStore refused)"
        return try {
            resolver.openOutputStream(uri)!!.use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
            values.clear()
            values.put(MediaStore.Images.Media.IS_PENDING, 0)
            resolver.update(uri, values, null, null)
            "Print saved: Pictures/midi-sink/$name (${w}x$h)"
        } catch (e: Exception) {
            resolver.delete(uri, null, null)
            "Print not saved: ${e.message}"
        } finally {
            bmp.recycle()
        }
    }

    private fun toast(msg: String) =
        android.widget.Toast.makeText(this, msg, android.widget.Toast.LENGTH_LONG).show()

    private fun panic() {
        overlay.releaseAll()
        NativeBridge.nativePanic()
        // The zone silence sends CC 64 = 0 on the wire; the strip engine's own
        // sustain state has to follow, or the pad and the DAW disagree until
        // the next press.
        NativeBridge.nativeStripSustainUp()
        strip.post { strip.syncMirrors(sustainToggle.value) }
    }

    private fun runSelfTests() {
        selfTestResult.value = "running…"
        thread(name = "selftest") {
            val path = "$filesDir/selftest.txt"
            val mask = NativeBridge.nativeRunSelfTests(path)
            val text = when {
                mask == 0 -> "self-tests PASS (hostmpe + normalizer suites) — $path"
                mask < 0 -> "self-tests could not write $path"
                else -> "self-tests FAIL mask=$mask (1 hostmpe, 2 normalizer) — $path"
            }
            Log.i(TAG, "SELFTEST_DONE mask=$mask $path")
            runOnUiThread { selfTestResult.value = text }
        }
    }

    /** Host-side thermal policy (step brief): sim_scale 0.75 baseline, 0.6
     *  under THERMAL_STATUS_SEVERE, back at MODERATE or below. The core
     *  never detects devices — this is the host's knob (§ params comment). */
    private fun registerThermalListener() {
        val pm = getSystemService(PowerManager::class.java) ?: return
        var degraded = false
        val listener = PowerManager.OnThermalStatusChangedListener { status ->
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
        thermalListener = listener
        pm.addThermalStatusListener(mainExecutor, listener)
    }

    /** Evidence hooks, driven via `adb shell am start` extras:
     *  `--es fieldDump 1` (§4.6 dump), `--ei stressMinutes N` (Osmose feeder),
     *  Step 22: `--es hostmpeTests 1` (on-device suites -> files/selftest.txt),
     *  `--ei layout N`, `--es playMode 1|0`, `--ei stormSeconds N`,
     *  `--es transports usb,virtual,ble` (any subset), `--es flushLogs 1`. */
    private fun handleDebugIntent(intent: Intent?) {
        if (intent == null) return
        if (intent.getStringExtra("fieldDump") != null) {
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
        val minutes = intent.getIntExtra("stressMinutes", 0)
        if (minutes > 0) {
            thread(name = "stress-arm") {
                Thread.sleep(3000)   // let the surface/instance come up
                Log.i(TAG, "starting stress feeder: $minutes min")
                NativeBridge.nativeStartStress(minutes)
            }
        }
        if (intent.getStringExtra("hostmpeTests") != null) {
            thread(name = "selftest-arm") {
                Thread.sleep(1500)   // the MIDI thread's engines are up by then
                runSelfTests()
            }
        }
        if (intent.hasExtra("layout")) {
            val id = intent.getIntExtra("layout", 0)
            if (id in 0..5) setLayout(id)
        }
        intent.getStringExtra("playMode")?.let { setPlayMode(it == "1" || it == "true") }
        intent.getStringExtra("transports")?.let { spec ->
            val parts = spec.split(",").map { it.trim().lowercase() }
            setTransports("usb" in parts, "virtual" in parts, "ble" in parts)
        }
        val storm = intent.getIntExtra("stormSeconds", 0)
        if (storm > 0) {
            thread(name = "storm-arm") {
                Thread.sleep(2000)
                NativeBridge.nativeStartStorm(storm)
            }
        }
        // Step 22 addendum: exercise the S-Pen barrel button without a hand on
        // the pen — `--es penButton click|down|up`.
        intent.getStringExtra("penButton")?.let { what ->
            when (what) {
                "down" -> overlay.simulatePenButton(true)
                "up" -> overlay.simulatePenButton(false)
                else -> thread(name = "pen-button") {
                    runOnUiThread { overlay.simulatePenButton(true) }
                    Thread.sleep(400)
                    runOnUiThread { overlay.simulatePenButton(false) }
                }
            }
        }
        if (intent.getStringExtra("flushLogs") != null) NativeBridge.nativeFlushLogs()
        if (intent.getStringExtra("panic") != null) runOnUiThread { panic() }
        if (intent.getStringExtra("resync") != null) NativeBridge.nativeResyncSession()
    }
}

private fun toggleRow(label: String, on: Boolean, onClick: () -> Unit): @Composable () -> Unit = {
    BasicText(
        (if (on) "●  " else "○  ") + label,
        style = TextStyle(color = if (on) Color.White else Color(0xCCFFFFFF), fontSize = 15.sp),
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 9.dp))
}

@Composable
private fun SectionTitle(text: String) {
    BasicText(
        text,
        style = TextStyle(color = Color(0x88FFFFFF), fontSize = 12.sp),
        modifier = Modifier.padding(top = 14.dp, bottom = 4.dp))
}

@Composable
private fun Footnote(text: String) {
    BasicText(
        text,
        style = TextStyle(color = Color(0x77FFFFFF), fontSize = 12.sp),
        modifier = Modifier.padding(top = 2.dp, bottom = 4.dp))
}

@Composable
private fun ActionRow(text: String, color: Color = Color(0xCCFFFFFF), onClick: () -> Unit) {
    BasicText(
        text,
        style = TextStyle(color = color, fontSize = 15.sp),
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 9.dp))
}

@Composable
private fun SegmentRow(a: String, b: String, second: Boolean, onClick: () -> Unit) {
    BasicText(
        (if (second) "○  $a" else "●  $a") + "      " + (if (second) "●  $b" else "○  $b"),
        style = TextStyle(color = Color(0xCCFFFFFF), fontSize = 15.sp),
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 9.dp))
}

/** The settings menu — the iOS SettingsSheet's Android sibling (same
 *  sections, same order; SumiApp.swift is the reference). sim_scale has no
 *  manual toggle here: on Android the thermal listener owns it. */
@Composable
fun SettingsDialog(
    currentLayout: Int,
    onLayout: (Int) -> Unit,
    playMode: Boolean,
    onPlayMode: (Boolean) -> Unit,
    velocityFromTouchSize: Boolean,
    onVelocityFromTouchSize: (Boolean) -> Unit,
    sustainToggle: Boolean,
    onSustainToggle: (Boolean) -> Unit,
    slidePinch: Boolean,
    pinchCrossed: Boolean,
    onSlidePinch: (Boolean, Boolean) -> Unit,
    bendRipple: Boolean,
    onBendMode: (Boolean) -> Unit,
    pressSwirl: Boolean,
    onPressMode: (Boolean) -> Unit,
    outUsb: Boolean,
    outVirtual: Boolean,
    outBle: Boolean,
    onTransports: (Boolean, Boolean, Boolean) -> Unit,
    usbStatus: String,
    bleStatus: String,
    virtualClients: Int,
    onResync: () -> Unit,
    onPanic: () -> Unit,
    onStorm: () -> Unit,
    onSelfTest: () -> Unit,
    selfTestResult: String,
    onPairBluetooth: () -> Unit,
    onPaperDip: (Boolean) -> Unit,
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
    val playable = currentLayout == 1 || currentLayout == 2 || currentLayout == 5
    val status = remember { mutableStateOf("") }
    LaunchedEffect(Unit) {
        while (true) {
            status.value = NativeBridge.nativeStatusLine()
            delay(1000)
        }
    }
    Dialog(onDismissRequest = onDismiss) {
        Column(
            Modifier
                .background(Color(0xEE18143A))
                .padding(20.dp)
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
        ) {
            BasicText("midi-sink", style = TextStyle(color = Color.White, fontSize = 18.sp))
            SectionTitle("LAYOUT")
            layouts.forEach { (id, name) -> toggleRow(name, id == currentLayout) { onLayout(id) }() }

            SectionTitle("MODE")
            SegmentRow("Marble", "Play", playMode && playable) { if (playable) onPlayMode(!playMode) }
            Footnote(
                if (!playable) "Play mode is available on the Chromatic grid, Jankó and Piano grid layouts."
                else if (playMode) "Play: each touch is an MPE joystick on the lattice; the S-Pen plays legato."
                else "Marble: tap = drop, drag = tine, twist = vortex, pinch = fold.")
            if (playMode && playable) {
                toggleRow("Velocity from touch size", velocityFromTouchSize) {
                    onVelocityFromTouchSize(!velocityFromTouchSize)
                }()
                Footnote("Glass has no force sensor: finger velocity is synthesized (96 fixed, or " +
                    "coarse touch-size modulation). The S-Pen's tip pressure is real.")
                SectionTitle("CONTROL STRIP")
                toggleRow("Sustain button latches (toggle)", sustainToggle) { onSustainToggle(!sustainToggle) }()
                Footnote("The strip floats top-left over the full lattice. Pitch springs back to " +
                    "center on release; Mod and the two assignable wheels latch (drag adds — " +
                    "regrasping never jumps). Long-press an assignable wheel to change its CC. " +
                    "All strip traffic rides the MPE master channel.")
            }

            SectionTitle("NOTE BEND")
            SegmentRow("Glide", "Ripple", bendRipple) { onBendMode(!bendRipple) }
            SectionTitle("PRESSURE (AFTERTOUCH)")
            SegmentRow("Feed", "Swirl", pressSwirl) { onPressMode(!pressSwirl) }
            Footnote(if (pressSwirl) "Hardware aftertouch stirs a Lamb–Oseen swirl at the note. On the play " +
                "surface: pull DOWN to stir (the down half-axis is always the swirl)."
                else "Hardware aftertouch feeds the drop (the v1 grow). The play surface's down-pull " +
                "plays the swirl either way; this only routes 0xD0 hardware.")
            SectionTitle("SLIDE (CC74)")
            SegmentRow("Hue", "Pinch", slidePinch) { onSlidePinch(!slidePinch, pinchCrossed) }
            if (slidePinch) {
                SegmentRow("Saddle", "Crossed tines", pinchCrossed) { onSlidePinch(slidePinch, !pinchCrossed) }
            }

            SectionTitle("MIDI")
            ActionRow("Pair Bluetooth MIDI instrument…") { onPairBluetooth() }
            Footnote("Wired and virtual MIDI inputs connect automatically.")

            SectionTitle("OUTBOUND MIDI (PLAY MODE)")
            toggleRow("USB-MIDI to the host computer (primary)", outUsb) { onTransports(!outUsb, outVirtual, outBle) }()
            Footnote(usbStatus)
            toggleRow("Virtual device (on-device DAWs)", outVirtual) { onTransports(outUsb, !outVirtual, outBle) }()
            Footnote("\"midi-sink Play Surface\" in any Android DAW's MIDI input list — " +
                (if (virtualClients > 0) "$virtualClients client(s) connected." else "no client connected."))
            toggleRow("Bluetooth (BLE-MIDI) advertise", outBle) { onTransports(outUsb, outVirtual, !outBle) }()
            Footnote(bleStatus)
            ActionRow("Re-sync DAW (MCM + bend range)") { onResync() }
            ActionRow("Stop all notes (panic)", Color(0xFFFF8A80)) { onPanic() }
            Footnote("Panic releases every held voice and silences all pipes. Switching a transport " +
                "off silences it so nothing hangs. USB and the virtual device stream at ≤100 Hz per " +
                "dimension; BLE uses a shared ~300 msg/s budget.")
            ActionRow("Run 60 s storm test (10 voices)") { onStorm() }
            ActionRow("Run on-device hostmpe + normalizer suites") { onSelfTest() }
            if (selfTestResult.isNotEmpty()) Footnote(selfTestResult)

            // The paper dip is DELIBERATE on the tablet (#67, as on the iPad): the
            // sustain pedal is a musical control in Play mode. Two buttons — keep
            // the print (PNG under Pictures/midi-sink through MediaStore) or start
            // a fresh sheet and let the print go.
            SectionTitle("CANVAS")
            ActionRow("Paper dip — save the print") { onPaperDip(true) }
            ActionRow("Paper dip — discard (fresh sheet, no print)", color = Color(0xCCFFB4A2)) {
                onPaperDip(false)
            }
            Footnote("Freezes and snapshots the canvas, then starts a clean sheet. Saved prints " +
                "land in Pictures/midi-sink as PNG (the gallery shows them).")

            SectionTitle("SESSION")
            BasicText(
                if (status.value.isEmpty()) "—" else status.value,
                style = TextStyle(color = Color(0xCCFFFFFF), fontSize = 12.sp))

            // The on-device DONE check for a release (ROADMAP_4 Step 31): the
            // installed build IS the tag. Same shape as the iPad's About (#37):
            // midi-sink X.Y.Z (versionCode) · describe, and the engine's ABI.
            SectionTitle("ABOUT")
            val core = remember { NativeBridge.nativeCoreVersion() }
            BasicText(
                "midi-sink ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE}) · ${BuildConfig.BUILD_DESCRIBE}",
                style = TextStyle(color = Color(0xCCFFFFFF), fontSize = 13.sp))
            BasicText(
                "libsumi ${core shr 16}.${(core shr 8) and 0xFF}.${core and 0xFF} · AGPL-3.0 · midi-sink.vibetuned.com",
                style = TextStyle(color = Color(0x99FFFFFF), fontSize = 12.sp))
        }
    }
}

/** The canvas: SurfaceView lifecycle -> JNI render thread, plus the iOS
 *  Marble gesture set (tap = drop, one-finger drag = tine, two-finger twist =
 *  vortex, two-finger pinch = fold — #41) in §4.6's y-down normalized space
 *  (Android view coords match). In Play mode the overlay covers this view and
 *  consumes every touch, so the marble path stays bit-identical. */
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
    private var pinchDist0 = 0f
    private var pinchLastScale = 1f

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
    private fun distBetween(e: MotionEvent): Float =
        hypot(e.getX(1) - e.getX(0), e.getY(1) - e.getY(0))

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = e.x; lastY = e.y
                moved = false; twoFinger = false
            }
            MotionEvent.ACTION_POINTER_DOWN -> if (e.pointerCount == 2) {
                twoFinger = true
                lastAngle = angleBetween(e)
                pinchDist0 = distBetween(e).coerceAtLeast(1f)
                pinchLastScale = 1f
            }
            MotionEvent.ACTION_MOVE -> {
                if (e.pointerCount >= 2) {
                    val cx = (e.getX(0) + e.getX(1)) * 0.5f
                    val cy = (e.getY(0) + e.getY(1)) * 0.5f
                    // Twist -> vortex (Step 13, unchanged).
                    val a = angleBetween(e)
                    var d = a - lastAngle
                    while (d > Math.PI) d -= (2 * Math.PI).toFloat()
                    while (d < -Math.PI) d += (2 * Math.PI).toFloat()
                    lastAngle = a
                    val strength = (d * vortexStrengthScale).coerceIn(-0.5f, 0.5f)
                    NativeBridge.nativeAddVortex(nx(cx), ny(cy), strength)
                    // #41: a literal two-finger pinch -> the v0.4 fold. The
                    // fold axis IS the finger-to-finger line (point space is
                    // isotropic, so its angle is the aspect-corrected fold
                    // angle directly); the squeeze is the DELTA-driven k.
                    val scale = distBetween(e) / pinchDist0
                    val dk = (scale - pinchLastScale) * 1.5f
                    pinchLastScale = scale
                    if (abs(dk) > 0.0015f) {
                        NativeBridge.nativeAddPinch(nx(cx), ny(cy), dk, a)
                    }
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

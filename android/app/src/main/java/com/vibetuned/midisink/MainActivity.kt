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
import android.view.Choreographer
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
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
import kotlin.math.PI
import kotlin.math.max
import kotlin.math.sqrt

private const val TAG = "sumi-shell"

class MainActivity : ComponentActivity() {

    private lateinit var midi: MidiInputs
    private lateinit var prefs: SharedPreferences
    private lateinit var overlay: PlayOverlayView
    private lateinit var strip: ControlStripView

    private val showSettings = mutableStateOf(false)
    private val showPairing = mutableStateOf(false)
    // Phase 4 §1: Marble (Step-13 gestures) vs Play (virtual MPE surface).
    private val playMode = mutableStateOf(false)
    private val playEffective = mutableStateOf(false)
    private val velocityFromTouchSize = mutableStateOf(false)
    private val sustainToggle = mutableStateOf(false)
    // Step 33 (author's request on the Pixel): the strip covers a fifth of a
    // phone's lattice — shown by default on tablets, hidden on phones.
    private val showStrip = mutableStateOf(true)
    // §5.4 transports: USB gadget is the primary sink.
    private val outUsb = mutableStateOf(true)
    private val outVirtual = mutableStateOf(true)
    private val outBle = mutableStateOf(false)
    private val selfTestResult = mutableStateOf("")
    // Step 45b (DECISIONS_5 #79): everything else is THE SESSION.
    private lateinit var session: SessionStore
    private var appliedLayout = -1
    private var appliedDark: Boolean? = null
    private var appliedStrip = 0 to 0
    private var pendingExportName = "midi-sink session"
    private val exportLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri != null) try {
            contentResolver.openOutputStream(uri)?.use { it.write(session.write(pendingExportName).toByteArray()) }
            toast("Exported '$pendingExportName'")
        } catch (e: Exception) { toast("Export failed: ${e.message}") }
    }
    private val importLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) try {
            val text = contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) } ?: ""
            val fallback = uri.lastPathSegment?.substringAfterLast('/')?.substringBeforeLast('.') ?: "imported"
            val name = session.importText(text, fallback)
            toast(if (name != null) "Imported '$name'" else "Not a midi-sink preset")
        } catch (e: Exception) { toast("Import failed: ${e.message}") }
    }
    private var blePermissionPending = false
    /** Held so onDestroy can remove it: each Activity creation would
     *  otherwise add another listener, all driving sim_scale independently. */
    private var thermalListener: PowerManager.OnThermalStatusChangedListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = getSharedPreferences("sumi", MODE_PRIVATE)
        playMode.value = prefs.getBoolean("playMode", false)
        velocityFromTouchSize.value = prefs.getBoolean("velocityFromTouchSize", false)
        sustainToggle.value = prefs.getBoolean("sustainToggle", false)
        outUsb.value = prefs.getBoolean("outUsb", true)
        outVirtual.value = prefs.getBoolean("outVirtual", true)
        outBle.value = prefs.getBoolean("outBle", false)
        showStrip.value = prefs.getBoolean("showStrip", resources.configuration.smallestScreenWidthDp >= 600)

        NativeBridge.nativeInit(filesDir.absolutePath)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        overlay = PlayOverlayView(this)
        strip = ControlStripView(this)
        overlay.velocityFromTouchSize = velocityFromTouchSize.value
        // The S-Pen's barrel button drives the strip's sustain engine, so the
        // palette's pad has to follow what the pen did.
        overlay.onSustainChanged = { strip.post { strip.syncMirrors(sustainToggle.value) } }
        strip.onAssigned = { wheel, cc ->
            val (a, b) = session.stripAssign()
            appliedStrip = if (wheel == 1) cc to b else a to cc
            session.setStripAssign(appliedStrip.first, appliedStrip.second)
        }
        NativeBridge.nativeStripSustainMode(sustainToggle.value)

        // The session: the last one, or the 0.x rows migrated once, applied by the
        // native side right after sumi_create (the core's defaults first).
        session = SessionStore(filesDir, prefs)
        session.onChange = { onSessionChange() }
        session.init()

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
                    update = { it.visibility = if (playEffective.value && showStrip.value) View.VISIBLE else View.GONE }
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
                if (showSettings.value) SettingsSheet(session, sheetHost)
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

    private val currentLayout: Int get() = if (session.ready.value) session.u("pitch_layout") else 0

    private fun setLayout(id: Int) { session.setParam("pitch_layout", id.coerceIn(0, 7)) }
    /** `--ei layout N` may arrive before the instance has seeded the session: it waits for it. */
    private var layoutFromIntent: Int? = null
        set(v) { field = v; if (v != null && session.ready.value) { field = null; setLayout(v) } }

    /** The session changed (a row, a preset, the first seed): the shell follows —
     *  the play surface's theme and lattice, the pen's slide, the strip's wheels. */
    private fun onSessionChange() {
        layoutFromIntent?.let { layoutFromIntent = null; setLayout(it); return }
        val dark = session.anod
        if (appliedDark != dark) { appliedDark = dark; overlay.setDarkTheme(dark); strip.setDarkTheme(dark) }
        overlay.slideMode = if (session.u("slide_mode") == 1) 1 else 0
        val lay = currentLayout
        if (lay != appliedLayout) { appliedLayout = lay; overlay.layoutChanged(); applyMode() }
        val want = session.stripAssign()
        if (want != appliedStrip) {
            appliedStrip = want
            if (want.first != 0) NativeBridge.nativeStripAssign(1, want.first)
            if (want.second != 0) NativeBridge.nativeStripAssign(2, want.second)
            strip.post { strip.syncMirrors(sustainToggle.value) }
        }
    }

    override fun onPause() {
        super.onPause()
        if (::session.isInitialized) session.save()
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
        val layout = currentLayout
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

    /** QOL §6: keep = "Dip the paper — keep the print" (into the ledger); false =
     *  "Clear the canvas — discard" (its print dropped on arrival). */
    private fun dip(keep: Boolean) {
        NativeBridge.nativeLedgerDip(keep)
        toast(if (keep) "Dipped: the sheet is in Prints" else "Cleared: a fresh sheet")
    }

    /** Re-export a ledger entry at Screen / 2K / 4K / 8K (Anod optionally over
     *  alpha) to Pictures/midi-sink, then the share sheet. */
    private fun exportPrint(id: Int, choice: Int, alpha: Boolean) {
        val e = ledgerRows().firstOrNull { it.id == id } ?: return
        val (w, h) = exportSize(choice, e.fw, e.fh)
        if (!NativeBridge.nativeLedgerExport(id, w, h, alpha)) { toast("Export could not start"); return }
        thread(name = "print-export") {
            val deadline = System.currentTimeMillis() + 20000
            var wh: IntArray? = null
            while (wh == null && System.currentTimeMillis() < deadline) { wh = NativeBridge.nativeLedgerExportSize(); if (wh == null) Thread.sleep(40) }
            if (wh == null) { runOnUiThread { toast("Export failed") }; return@thread }
            val buf = java.nio.ByteBuffer.allocateDirect(wh[0] * wh[1] * 4)
            if (!NativeBridge.nativeLedgerExportTake(buf)) { runOnUiThread { toast("Export failed") }; return@thread }
            val suffix = "-${wh[0]}x${wh[1]}" + (if (alpha && e.anod) "-alpha" else "")
            val uri = writePng(buf, wh[0], wh[1], straightAlpha = alpha && e.anod, suffix = suffix)
            runOnUiThread {
                if (uri == null) toast("Print not saved") else {
                    toast("Saved to Pictures/midi-sink (${wh[0]}×${wh[1]})")
                    startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("image/png")
                        .putExtra(Intent.EXTRA_STREAM, uri).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION), "Share the print"))
                }
            }
        }
    }

    private fun saveNewestPrint() {
        val e = ledgerRows().lastOrNull { it.printSeen } ?: return
        thread(name = "print-save") {
            val px = NativeBridge.nativeLedgerPixels(e.id, 1)
            val uri = px?.let { writePng(java.nio.ByteBuffer.wrap(it), e.pw, e.ph, false, "") }
            runOnUiThread { toast(if (uri != null) "Saved to Pictures/midi-sink" else "Print not saved") }
        }
    }

    /** RGBA8 -> PNG through MediaStore (no storage permission on API 29+, our minSdk). */
    private fun writePng(rgba: java.nio.ByteBuffer, w: Int, h: Int, straightAlpha: Boolean, suffix: String): android.net.Uri? {
        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        if (straightAlpha) bmp.isPremultiplied = false
        rgba.rewind(); bmp.copyPixelsFromBuffer(rgba)
        val stamp = java.text.SimpleDateFormat("yyyyMMdd-HHmmss", java.util.Locale.US).format(java.util.Date())
        val name = "midi-sink-print-$stamp$suffix.png"
        val values = ContentValues().apply {
            put(MediaStore.Images.Media.DISPLAY_NAME, name)
            put(MediaStore.Images.Media.MIME_TYPE, "image/png")
            put(MediaStore.Images.Media.RELATIVE_PATH, Environment.DIRECTORY_PICTURES + "/midi-sink")
            put(MediaStore.Images.Media.IS_PENDING, 1)
        }
        val uri = contentResolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values) ?: return null
        return try {
            contentResolver.openOutputStream(uri)!!.use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
            values.clear(); values.put(MediaStore.Images.Media.IS_PENDING, 0)
            contentResolver.update(uri, values, null, null)
            Log.i(TAG, "[print] saved $name (${w}x$h)")
            uri
        } catch (ex: Exception) { contentResolver.delete(uri, null, null); null } finally { bmp.recycle() }
    }

    private val sheetHost = object : SheetHost {
        override val playMode get() = this@MainActivity.playMode.value
        override val playEffective get() = this@MainActivity.playEffective.value
        override fun setPlayMode(on: Boolean) = this@MainActivity.setPlayMode(on)
        override val velocityFromTouchSize get() = this@MainActivity.velocityFromTouchSize.value
        override fun setVelocityFromTouchSize(on: Boolean) {
            this@MainActivity.velocityFromTouchSize.value = on; overlay.velocityFromTouchSize = on
            prefs.edit().putBoolean("velocityFromTouchSize", on).apply()
        }
        override val showStrip get() = this@MainActivity.showStrip.value
        override fun setShowStrip(on: Boolean) { this@MainActivity.showStrip.value = on; prefs.edit().putBoolean("showStrip", on).apply() }
        override val sustainToggle get() = this@MainActivity.sustainToggle.value
        override fun setSustainToggle(on: Boolean) {
            this@MainActivity.sustainToggle.value = on; prefs.edit().putBoolean("sustainToggle", on).apply()
            NativeBridge.nativeStripSustainMode(on); strip.post { strip.syncMirrors(on) }
        }
        override val outUsb get() = this@MainActivity.outUsb.value
        override val outVirtual get() = this@MainActivity.outVirtual.value
        override val outBle get() = this@MainActivity.outBle.value
        override fun setTransports(usb: Boolean, virt: Boolean, ble: Boolean) = this@MainActivity.setTransports(usb, virt, ble)
        override val usbStatus get() = MidiOutputs.usbStatus.value
        override val bleStatus get() = MidiOutputs.ble?.state?.value ?: "BLE: unavailable"
        override val virtualClients get() = SumiMidiDeviceService.openClientsState.value
        override val selfTestResult get() = this@MainActivity.selfTestResult.value
        override fun resync() = NativeBridge.nativeResyncSession()
        override fun panic() = this@MainActivity.panic()
        override fun storm() = NativeBridge.nativeStartStorm(60)
        override fun selfTest() = runSelfTests()
        override fun pairBluetooth() { showSettings.value = false; showPairing.value = true }
        override fun dip(keep: Boolean) = this@MainActivity.dip(keep)
        override fun exportPreset(name: String) { pendingExportName = name; exportLauncher.launch(SessionStore.safeName(name) + ".json") }
        override fun importPreset() = importLauncher.launch(arrayOf("application/json", "text/plain", "application/octet-stream"))
        override fun sharePreset(name: String) {
            val f = java.io.File(cacheDir, SessionStore.safeName(name) + ".json"); f.writeText(session.presetFile(name).readText())
            startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("application/json")
                .putExtra(Intent.EXTRA_TEXT, f.readText()).putExtra(Intent.EXTRA_SUBJECT, name), "Share the preset"))
        }
        override fun exportPrint(id: Int, choice: Int, alpha: Boolean) = this@MainActivity.exportPrint(id, choice, alpha)
        override fun saveNewestPrint() = this@MainActivity.saveNewestPrint()
        override fun dismiss() { showSettings.value = false; NativeBridge.nativeFlushLogs(); session.save() }
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
     *  `--es transports usb,virtual,ble` (any subset), `--es flushLogs 1`,
     *  Phase 7 step 48: `--ei voxoSpike N` (Voxo on AAudio, the latency numbers
     *  -> files/voxo_spike.csv; pair with `--es playMode 1` and injected touches). */
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
            if (id in 0..7) layoutFromIntent = id
        }
        intent.getStringExtra("playMode")?.let { setPlayMode(it == "1" || it == "true") }
        intent.getStringExtra("transports")?.let { spec ->
            val parts = spec.split(",").map { it.trim().lowercase() }
            setTransports("usb" in parts, "virtual" in parts, "ble" in parts)
        }
        val spike = intent.getIntExtra("voxoSpike", 0)
        if (spike > 0) {
            thread(name = "voxo-spike") {
                Thread.sleep(2500)   // the surface, the engines and the MIDI thread are up by then
                NativeBridge.nativeVoxoSpike(spike)
            }
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

/** The CC map's names and default tables — the desktop's app_settings_default_routes
 *  and the iPad's CcMap, verbatim: channel 0xFF = any, target = sumi_ctl_t. */
object CcMap {
    data class Route(val channel: Int, val cc: Int, val target: Int)

    private val ctlNames = listOf(
        "Vortex strength", "Vortex center X", "Vortex center Y", "Viscosity",
        "Paper roughness", "Palette morph", "Ink flow (breath)", "Ripple amount", "Ripple wavelength",
        // v0.9 (#69 of Part IV): the right hand's swirl trio and the two grasp pinches.
        "Swirl strength", "Swirl center X", "Swirl center Y", "Pinch (saddle)", "Pinch (crossed tines)",
        // Phase 6 (steps 36–40): the operators' dimensions, the desktop's names.
        "Torsion wavelength", "Torsion phase", "Chladni stir", "Chladni balance", "Spark frequency", "Chirikov throw")
    fun ctlName(t: Int): String = ctlNames.getOrNull(t) ?: "?"
    val ctlCount: Int get() = ctlNames.size

    val defaults: List<Route> = listOf(
        Route(0xFF, 1, 0), Route(0xFF, 2, 6), Route(0xFF, 7, 6), Route(0xFF, 11, 6),
        Route(0xFF, 26, 0), Route(0xFF, 24, 1), Route(0xFF, 22, 2),
        Route(0xFF, 27, 9), Route(0xFF, 25, 10), Route(0xFF, 23, 11),
        Route(0xFF, 20, 12), Route(0xFF, 21, 13),
        Route(0xFF, 28, 8), Route(0xFF, 29, 7),
        Route(0xFF, 102, 7), Route(0xFF, 103, 8),
        Route(0xFF, 104, 14), Route(0xFF, 105, 15), Route(0xFF, 106, 16),
        Route(0xFF, 107, 17), Route(0xFF, 108, 18), Route(0xFF, 109, 19))

    /** The routed controls at rest: ripple amount / wavelength, the Chladni stir and balance, the spark frequency, the Chirikov throw. */
    val controlDefaults: Map<Int, Int> = mapOf(7 to 0, 8 to 32, 16 to 0, 17 to 0, 18 to 64, 19 to 0)

    /** Earlier DEFAULT maps (#71 of Part IV): a stored map equal to one of these
     *  is the stock map of an older version and reads as today's. */
    private val olderDefaults: List<Set<Route>> = listOf(
        setOf(Route(0xFF, 1, 0), Route(0xFF, 2, 6), Route(0xFF, 7, 6), Route(0xFF, 11, 6),
            Route(0xFF, 26, 0), Route(0xFF, 24, 1), Route(0xFF, 22, 2), Route(0xFF, 29, 3),
            Route(0xFF, 30, 4), Route(0xFF, 31, 5), Route(0xFF, 27, 7), Route(0xFF, 28, 8),
            Route(0xFF, 102, 7), Route(0xFF, 103, 8)),   // #50
        defaults.take(16).toSet())                        // 1.0.0's map, before the Phase-6 handles 104–109

    /** The 0.x SharedPreferences form "ch:cc:target;…" ("" = the default map). */
    fun decode(s: String): List<Route> {
        if (s.isEmpty()) return defaults
        val out = s.split(";").mapNotNull { part ->
            val f = part.split(":")
            if (f.size != 3) return@mapNotNull null
            val ch = f[0].toIntOrNull() ?: return@mapNotNull null
            val cc = f[1].toIntOrNull() ?: return@mapNotNull null
            val t = f[2].toIntOrNull() ?: return@mapNotNull null
            if (cc !in 0..127 || t !in 0 until 20 || !(ch == 0xFF || ch in 0..15)) null else Route(ch, cc, t)
        }
        if (olderDefaults.any { it == out.toSet() }) return defaults
        return out
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

    // The long press (DECISIONS_4 #49; 1.1 #75): 250 ms without travel; its first
    // touch is a tap (Sumi a drop, Anod the strike), then Play mode's bipolar Y —
    // hold or push up, pull back — goes to the core every vsync, which plays the
    // medium's press (Sumi feed / swirl, Anod torsion feed / the Chladni stir).
    private data class Press(val cy0: Float, var cy: Float)
    private var press: Press? = null
    private var pressLastNs = 0L
    private var downX = 0f
    private var downY = 0f
    private val longPress = Runnable {
        if (!moved && !twoFinger && press == null) {
            NativeBridge.nativeGesturePressBegin(nx(downX), ny(downY))
            press = Press(downY, downY)
            pressLastNs = 0L
            Choreographer.getInstance().postFrameCallback(pressTick)
        }
    }
    private val pressTick = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            val p = press ?: return
            val dt = if (pressLastNs == 0L) 1f / 60f
                     else ((frameTimeNanos - pressLastNs) / 1e9f).coerceIn(0f, 0.1f)
            pressLastNs = frameTimeNanos
            val dy = (p.cy0 - p.cy) / height.coerceAtLeast(1)          // up = positive, canvas heights
            NativeBridge.nativeGesturePressFrame((dy / PRESS_TRAVEL).coerceIn(0f, 1f), (-dy / PRESS_TRAVEL).coerceIn(0f, 1f), dt)
            Choreographer.getInstance().postFrameCallback(this)
        }
    }
    private fun endPress() {
        if (press != null) NativeBridge.nativeGesturePressEnd()
        press = null
    }

    // Step 33 (#54): the S-Pen in MARBLE mode draws the wake — spec §8.7 says the
    // wake rides every stroke segment in BOTH modes, but it lived only in the
    // Play overlay, which Marble mode hides, so a pen fell through to the
    // finger path and drew tines. Same tip mapping as the overlay's movePen.
    private val stylusLast = HashMap<Int, FloatArray>()
    private fun isStylus(e: MotionEvent, i: Int): Boolean {
        val t = e.getToolType(i)
        return t == MotionEvent.TOOL_TYPE_STYLUS || t == MotionEvent.TOOL_TYPE_ERASER
    }
    private fun handleStylus(e: MotionEvent): Boolean {
        val idx = e.actionIndex
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> if (isStylus(e, idx)) {
                stylusLast[e.getPointerId(idx)] = floatArrayOf(e.getX(idx), e.getY(idx))
                removeCallbacks(longPress)          // a pen never long-presses into the pressure gesture
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (stylusLast.isEmpty()) return false
                val w = width.coerceAtLeast(1).toFloat()
                val h = height.coerceAtLeast(1).toFloat()
                for (i in 0 until e.pointerCount) {
                    val last = stylusLast[e.getPointerId(i)] ?: continue
                    val x = e.getX(i); val y = e.getY(i)
                    val pressure = e.getPressure(i).coerceIn(0f, 1f)
                    NativeBridge.nativeAddWake(last[0] / w, last[1] / h, x / w, y / h,
                                               0.006f + 0.030f * pressure)
                    last[0] = x; last[1] = y
                }
                return true                          // a pen owns the event; fingers wait
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                if (stylusLast.remove(e.getPointerId(idx)) != null) return true
                if (e.actionMasked == MotionEvent.ACTION_CANCEL) stylusLast.clear()
            }
        }
        return false
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (handleStylus(e)) return true
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = e.x; lastY = e.y
                downX = e.x; downY = e.y
                moved = false; twoFinger = false
                endPress()
                removeCallbacks(longPress)
                postDelayed(longPress, LONG_PRESS_MS)
            }
            MotionEvent.ACTION_POINTER_DOWN -> if (e.pointerCount == 2) {
                removeCallbacks(longPress)
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
                    NativeBridge.nativeGestureTwist(nx(cx), ny(cy), strength)   // #75: Anod the torsion vortex
                    // #41: a literal two-finger pinch -> the v0.4 fold. The
                    // fold axis IS the finger-to-finger line (point space is
                    // isotropic, so its angle is the aspect-corrected fold
                    // angle directly); the squeeze is the DELTA-driven k.
                    val scale = distBetween(e) / pinchDist0
                    val dk = (scale - pinchLastScale) * 1.5f
                    pinchLastScale = scale
                    if (abs(dk) > 0.0015f) {
                        // #75: Anod the burst; span = the finger distance in canvas heights
                        NativeBridge.nativeGesturePinch(nx(cx), ny(cy), dk, a, distBetween(e) / height.coerceAtLeast(1))
                    }
                } else if (press != null) {
                    press?.cy = e.y                     // the press modulates, it does not draw
                } else if (!twoFinger) {
                    val dx = e.x - lastX
                    val dy = e.y - lastY
                    if (dx * dx + dy * dy >= dragThresholdPx * dragThresholdPx) {
                        removeCallbacks(longPress)      // travel before it fires = a stroke
                        NativeBridge.nativeAddTine(
                            nx(lastX), ny(lastY), nx(e.x), ny(e.y), lengthAC(dx, dy))
                        moved = true
                        lastX = e.x; lastY = e.y
                    }
                }
            }
            MotionEvent.ACTION_UP -> {
                removeCallbacks(longPress)
                val pressed = press != null
                endPress()
                if (!moved && !twoFinger && !pressed) NativeBridge.nativeGestureTap(nx(e.x), ny(e.y))   // #75: Sumi a drop, Anod the strike
            }
            MotionEvent.ACTION_CANCEL -> {
                removeCallbacks(longPress)
                endPress()
            }
        }
        return true
    }

    private companion object {
        const val LONG_PRESS_MS = 250L
        const val PRESS_TRAVEL = 0.15f   // canvas heights of push / pull for full effect (the rates live in the core, #75)
    }
}

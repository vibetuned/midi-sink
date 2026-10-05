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
import android.graphics.PixelFormat
import android.util.Log
import android.view.MotionEvent
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.WindowInsets
import android.widget.FrameLayout
import android.view.Gravity
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
import org.json.JSONObject
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
    lateinit var sound: Sound                      // Phase 7 step 54 (Sound.kt)
    private val soundTick = object : Runnable { override fun run() { if (::sound.isInitialized) sound.tick(); tickHandler.postDelayed(this, 1000) } }
    private val tickHandler = android.os.Handler(android.os.Looper.getMainLooper())
    private val pickInstrumentFile = registerForActivityResult(androidx.activity.result.contract.ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) sound.importDocument(uri)?.let { sound.setInstrument(it) }
    }
    private val pickInstrumentFolder = registerForActivityResult(androidx.activity.result.contract.ActivityResultContracts.OpenDocumentTree()) { uri ->
        if (uri != null) sound.importTree(uri)?.let { sound.setInstrument(it) }
    }
    private lateinit var overlay: PlayOverlayView
    private lateinit var strip: ControlStripView

    private val showSettings = mutableStateOf(false)
    private val showPairing = mutableStateOf(false)
    // Phase 4 §1: Marble (Step-13 gestures) vs Play (virtual MPE surface).
    private val playMode = mutableStateOf(false)
    private val playEffective = mutableStateOf(false)
    private val velocityFromTouchSize = mutableStateOf(false)
    private val sustainToggle = mutableStateOf(false)
    // Phase 9 step 64 (the iPad's step 63, DECISIONS_8 #13–#17 on the Tab): left-handed, the fingering
    // panel's form and the quick-switch subset (persisted); `--es mirror 1` and `--es fingeringHorizontal 1`
    // are TRANSIENT lab overrides — the author's stored switches are never written by an extra.
    private val leftHanded = mutableStateOf(false)
    private val fingeringHorizontal = mutableStateOf(false)
    private val quickSwitch = mutableStateOf(setOf<Int>())
    private var argMirror = false
    private var argHorizontal = false
    private lateinit var panel: FingeringPanelView
    private var frame: FrameLayout? = null
    private var statusTopPx = 0
    private var navBottomPx = 0
    private val offered = HashSet<String>()   // one offer per device per launch (QOL §2)
    private var demoRunning = false
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
    // Phase 9 step 65 (QOL §1): session replay — the status polled from the native side (the banner while a replay
    // plays), the export/import pickers for the files under files/Replays.
    private val replayState = mutableStateOf("idle|")
    private val replayTick = object : Runnable { override fun run() { replayState.value = NativeBridge.nativeReplayStatus(); tickHandler.postDelayed(this, 250) } }
    private var pendingReplayExport = ""
    private val replayExportLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        if (uri != null) try {
            val f = java.io.File(java.io.File(filesDir, "Replays"), pendingReplayExport)
            contentResolver.openOutputStream(uri)?.use { it.write(f.readBytes()) }
            toast("Exported '$pendingReplayExport'")
        } catch (e: Exception) { toast("Export failed: ${e.message}") }
    }
    private val replayImportLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) try {
            val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: ByteArray(0)
            var name = uri.lastPathSegment?.substringAfterLast('/') ?: "imported"
            if (!name.endsWith(".sumireplay")) name += ".sumireplay"
            if (!String(bytes, Charsets.UTF_8).startsWith("#sumi-replay")) { toast("Not a midi-sink recording"); return@registerForActivityResult }
            val dir = java.io.File(filesDir, "Replays").also { it.mkdirs() }
            java.io.File(dir, name).writeBytes(bytes)
            toast("Imported '$name'")
        } catch (e: Exception) { toast("Import failed: ${e.message}") }
    }
    private fun replayNames(): List<String> =
        (java.io.File(filesDir, "Replays").listFiles() ?: emptyArray()).filter { it.name.endsWith(".sumireplay") }.map { it.name }.sortedDescending()
    private fun replayRecord() {
        val app = try { packageManager.getPackageInfo(packageName, 0).versionName ?: "?" } catch (_: Exception) { "?" }
        Log.i(TAG, "[replay] record requested (${android.os.Build.MODEL}, $app)")
        NativeBridge.nativeReplayRecordStart(android.os.Build.MODEL ?: "android", app)
    }
    /** step 65's evidence: `--es recordLab <s>` — the lab's small field, the recording for <s> seconds (the fingering
     *  demo's phrase lands inside), then files/Replays/lab.sumireplay with the field after the last frame beside it. */
    private fun runLabRecording(seconds: Int) {
        Log.i(TAG, "[replay] lab: $seconds s at the small field")
        val dir = java.io.File(filesDir, "Replays").also { it.mkdirs() }
        NativeBridge.nativeReplayLabSize(true)
        NativeBridge.nativeSetSimScale(1.0f, 0)
        tickHandler.postDelayed({ replayRecord(); NativeBridge.nativeResyncSession() }, 600)
        tickHandler.postDelayed({
            val path = NativeBridge.nativeReplayRecordStop(java.io.File(dir, "lab.field.bin").absolutePath)
            if (path.isNotEmpty()) java.io.File(path).copyTo(java.io.File(dir, "lab.sumireplay"), overwrite = true)
            NativeBridge.nativeReplayLabSize(false)
            NativeBridge.nativeSetSimScale(if (prefs.getBoolean("fullRes", true)) 1.0f else 0.75f, 0)
            Log.i(TAG, "[replay] lab recording done: $path")
        }, 600 + seconds * 1000L)
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
        leftHanded.value = prefs.getBoolean("leftHanded", false)
        fingeringHorizontal.value = prefs.getBoolean("fingeringHorizontal", false)
        quickSwitch.value = (prefs.getString("quickSwitch", "") ?: "").split(",").mapNotNull { it.trim().toIntOrNull() }.toSet()

        NativeBridge.nativeInit(filesDir.absolutePath)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        overlay = PlayOverlayView(this)
        strip = ControlStripView(this)
        panel = FingeringPanelView(this)
        // step 64: the panel's engines are the strip's (hostmpe on the MIDI thread); its mirror — exact,
        // the slide quantised as sent — is the state the overlay hands the probe
        panel.onValve = { k, down -> if (down) NativeBridge.nativeStripValveDown(k) else NativeBridge.nativeStripValveUp(k); overlay.setFingering(panel.valves, panel.slide) }
        panel.onSlide = { pos -> NativeBridge.nativeStripSlideSet(pos); overlay.setFingering(panel.valves, pos) }
        panel.onRotate = { setFingeringHorizontal(!fingeringHorizontal.value) }
        strip.onNext = { quickNext() }
        strip.onPanic = { panic() }
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
        sound = Sound(this, prefs)   // Phase 7 step 54: the instrument inside
        sound.onReachChanged = { mask -> overlay.setCoveredNotes(mask) }
        sound.start()
        session.onChange = { onSessionChange() }
        session.init()

        midi = MidiInputs(this)
        midi.onSourceAppeared = { name -> runOnUiThread { offer(name) } }   // step 64: the per-device offer
        midi.start()
        MidiOutputs.start(this)
        applyTransports()

        registerThermalListener()
        handleDebugIntent(intent)
        applyMode()
        applyMirror()
        applyOrientation()
        applyQuick()

        setContent {
            Box(Modifier.fillMaxSize()) {
                // The canvas, the play overlay and the strip live in ONE native
                // frame (step 54, DECISIONS_6 #30): Compose hands a whole
                // multi-finger gesture to the interop view that took the first
                // finger, so as three separate AndroidViews the strip and the
                // cells could never be touched together; a ViewGroup splits
                // pointers between its children by default.
                val density = LocalDensity.current
                val statusTop = WindowInsets.statusBars.getTop(density)
                val navBottom = WindowInsets.navigationBars.getBottom(density)
                val stripH = with(density) { 86.dp.roundToPx() }
                AndroidView(
                    factory = { ctx ->
                        FrameLayout(ctx).apply {
                            isMotionEventSplittingEnabled = true
                            addView(SumiSurfaceView(ctx), FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
                            // Phase 4 §6: the play overlay keeps the FULL bounds so a
                            // touched cell and its loopback drop stay exactly aligned
                            // (#31); hidden and inert in Marble mode.
                            addView(this@MainActivity.overlay, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))   // (a View has an `overlay` of its own)
                            // §8 rev (#31): the strip is a compact floating palette at the
                            // top-left OVER the lattice; it consumes its own touches.
                            addView(this@MainActivity.strip, FrameLayout.LayoutParams(0, stripH, Gravity.TOP or Gravity.START))
                            // step 64: the fingering panel — the valves or the slide, large, at the side or along the
                            // bottom — is NOT a child here: it lives in a window of its own over the frame (#21,
                            // syncPanelWindow), so the fingers' input stream never meets the S Pen's.
                            addOnLayoutChangeListener { _, l, t, r, b, ol, ot, orr, ob -> if (r - l != orr - ol || b - t != ob - ot) post { layoutPlaySurface() } }
                            this@MainActivity.frame = this
                        }
                    },
                    modifier = Modifier.fillMaxSize(),
                    update = {
                        statusTopPx = statusTop; navBottomPx = navBottom
                        overlay.visibility = if (playEffective.value) View.VISIBLE else View.GONE
                        strip.visibility = if (playEffective.value && showStrip.value) View.VISIBLE else View.GONE
                        layoutPlaySurface()
                    }
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
                if (replayState.value.startsWith("play|")) {   // step 65 (QOL §1): the replay banner — the source device and app version
                    Row(Modifier.align(Alignment.TopCenter).statusBarsPadding().padding(10.dp).background(Color(0xAA18143A)).padding(10.dp)) {
                        BasicText(replayState.value.split("|").getOrNull(1) ?: "", style = TextStyle(color = Color(0xEEFFFFFF), fontSize = 13.sp))
                        BasicText("   Stop", style = TextStyle(color = Color.White, fontSize = 13.sp), modifier = Modifier.clickable { NativeBridge.nativeReplayStopPlay() })
                    }
                }
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
        handleOpenIntent(intent)
    }

    /** Step 54 (#29): a .dslibrary handed over by "Open with", Nearby Share, Mail. */
    private fun handleOpenIntent(intent: Intent?) {
        val uri = intent?.data ?: return
        if (intent.action != Intent.ACTION_VIEW || !::sound.isInitialized) return
        sound.importDocument(uri)?.let { sound.setInstrument(it); sound.setEnabled(true); showSettings.value = true }
    }

    override fun onStart() {
        super.onStart()
        panelStarted = true
        window.decorView.post { layoutPlaySurface() }   // the panel's window once the decor has its token
    }
    override fun onStop() {
        panelStarted = false
        removePanelWindow()   // a sub-window may not outlive its parent's visibility
        super.onStop()
    }

    override fun onResume() {
        super.onResume()
        if (::sound.isInitialized) { sound.onResume(); tickHandler.removeCallbacks(soundTick); tickHandler.postDelayed(soundTick, 1000) }
        tickHandler.removeCallbacks(replayTick); tickHandler.postDelayed(replayTick, 250)   // step 65: the replay status for the banner and the sheet
    }

    override fun onDestroy() {
        // Unconditional: a non-finishing destroy is reachable (a locale or
        // fontScale change, "don't keep activities", the system reclaiming
        // the instance). Skipping this used to leave a held voice sounding on
        // every sink forever — §5.1's 30 s timeout covers EXTERNAL occupancy
        // only — and left the USB_STATE receiver and the MIDI/thermal
        // callbacks registered against a dead Activity.
        overlay.releaseAll()
        removePanelWindow()
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

    private fun setLayout(id: Int) { session.setParam("pitch_layout", id.coerceIn(0, 12)) }
    /** `--ei layout N` may arrive before the instance has seeded the session: it waits for it. Step 64: a
     *  scripted layout is the whole layout — `--es trumpetArc 1` arranges the brass on the ring (absent, the
     *  column), `--ei stringTuning N` picks the strings' preset. */
    private var layoutExtras = JSONObject()
    private var layoutFromIntent: Int? = null
        set(v) { field = v; if (v != null && session.ready.value) { field = null; applyLayoutFromIntent(v) } }
    private fun applyLayoutFromIntent(id: Int) {
        val p = JSONObject(layoutExtras.toString()).put("pitch_layout", id.coerceIn(0, 12))
        session.patch(JSONObject().put("params", p))
    }

    /** The session changed (a row, a preset, the first seed): the shell follows —
     *  the play surface's theme and lattice, the pen's slide, the strip's wheels. */
    private fun onSessionChange() {
        layoutFromIntent?.let { layoutFromIntent = null; applyLayoutFromIntent(it); return }
        val dark = session.anod
        if (appliedDark != dark) { appliedDark = dark; overlay.setDarkTheme(dark); strip.setDarkTheme(dark); panel.setDarkTheme(dark) }
        overlay.slideMode = if (session.u("slide_mode") == 1) 1 else 0
        val lay = currentLayout
        // step 64: the arc and the tuning re-cut the lattice without a layout change
        val key = lay * 100 + session.u("trumpet_arc") * 10 + session.u("string_tuning")
        Log.i(TAG, "[session] change: layout=$lay key=$key applied=$appliedLayout")
        if (key != appliedLayout) { appliedLayout = key; overlay.layoutId = lay; overlay.layoutChanged(); applyMode() }
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
        if (::sound.isInitialized) { sound.onPause(); tickHandler.removeCallbacks(soundTick) }
        tickHandler.removeCallbacks(replayTick)
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
        // step 64: every keyed layout plays — the three of Phase 4 and Phase 9's five
        val playable = layout == 1 || layout == 2 || layout == 5 || layout in 8..12
        val effective = playMode.value && playable
        overlay.layoutId = layout
        panel.setMode(if (layout == 8) FingeringPanelView.Mode.VALVES else if (layout == 9) FingeringPanelView.Mode.SLIDE else FingeringPanelView.Mode.NONE)
        panelWanted = effective && (layout == 8 || layout == 9)
        layoutPlaySurface()
        if (effective == playEffective.value) return
        if (!effective) overlay.releaseAll()   // ends any held voices cleanly
        playEffective.value = effective
        Log.i(TAG, "[mode] play=${playMode.value} layout=$layout playable=$playable effective=$effective")
        // Working rule: entering Play mode pushes MCM/RPN0 into the LOOPBACK
        // before any notes (and out every sink), then the strip announces.
        NativeBridge.nativeSetPlayMode(effective)
        if (effective) strip.post { strip.syncMirrors(sustainToggle.value); fingeringSync() }
    }

    /** Step 64 (the iPad's layoutPlaySurface): the strip at the top-left — the top-RIGHT on the brass
     *  layouts (the panel takes the left), stopping short of the settings gear — the sides swapped
     *  left-handed; the panel at the side at mid-height, or along the bottom edge. */
    private fun layoutPlaySurface() {
        val f = frame ?: return
        if (!::panel.isInitialized) return
        val d = resources.displayMetrics.density
        fun px(dp: Float) = (dp * d + 0.5f).toInt()
        val layout = currentLayout
        val brass = layout == 8 || layout == 9
        val mirrored = leftHanded.value || argMirror
        val stripRight = brass != mirrored
        val fw = f.width; val fh = f.height
        val stripW = if (fw > 0) minOf(px(strip.preferredWidthDp), (fw * 0.6f).toInt()) else px(strip.preferredWidthDp)
        (strip.layoutParams as? FrameLayout.LayoutParams)?.let { lp ->
            val g = Gravity.TOP or (if (stripRight) Gravity.END else Gravity.START)
            val l = if (stripRight) 0 else px(10f); val r = if (stripRight) px(62f) else 0   // the gear's column at the right
            if (lp.width != stripW || lp.gravity != g || lp.leftMargin != l || lp.rightMargin != r || lp.topMargin != statusTopPx + px(10f)) {
                lp.width = stripW; lp.gravity = g; lp.setMargins(l, statusTopPx + px(10f), r, 0); strip.layoutParams = lp
            }
        }
        // the panel's rect inside the frame: at the side at mid-height, or along the bottom edge
        val (pwDp, phDp) = panel.preferredDp
        val w: Int; val h: Int; val x: Int; val y: Int
        if (fingeringHorizontal.value || argHorizontal) {
            w = if (fw > 0) minOf(px(pwDp), (fw * 0.5f).toInt()) else px(pwDp); h = px(phDp)
            x = if (mirrored) fw - w - px(12f) else px(12f); y = fh - h - navBottomPx - px(12f)
        } else {
            w = px(pwDp); h = if (fh > 0) minOf(px(phDp), (fh * 0.64f).toInt()) else px(phDp)
            x = if (mirrored) fw - w - px(12f) else px(12f); y = (fh - h) / 2
        }
        syncPanelWindow(w, h, x, y)
    }

    // -- the panel's window (DECISIONS_8 #21) --------------------------------------------------------
    // Android's input dispatcher lets ONE input device be active at a time in a window and prefers the
    // stylus (InputState::shouldCancelPreviousStream, "for compatibility, only one input device can be
    // active at a time in the same window"): the S Pen coming into hover range cancels the fingers'
    // gestures in the window and drops their presses until it leaves. The valves and the slide must
    // follow the fingers WHILE the pen plays the partials, so the panel is a sub-window of its own over
    // the frame — split-touch, non-modal, unfocusable, laid in screen coordinates at the rect the frame
    // would have given it — and the fingers' stream there never meets the pen's on the overlay.
    private var panelWanted = false
    private var panelStarted = false
    private var panelAdded = false
    private val panelLp = WindowManager.LayoutParams().apply {
        type = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL
        format = PixelFormat.TRANSLUCENT
        flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
            WindowManager.LayoutParams.FLAG_SPLIT_TOUCH or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS or WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED
        gravity = Gravity.TOP or Gravity.START
        windowAnimations = 0
        title = "sumi-fingering"
        layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        if (Build.VERSION.SDK_INT >= 30) setFitInsetsTypes(0)
    }
    private fun syncPanelWindow(w: Int, h: Int, x: Int, y: Int) {
        val f = frame ?: return
        val token = f.windowToken
        if (!panelWanted || !panelStarted || token == null || w <= 0 || h <= 0) { removePanelWindow(); return }
        val loc = IntArray(2); f.getLocationOnScreen(loc)
        val sx = loc[0] + x; val sy = loc[1] + y
        if (panelAdded && panelLp.width == w && panelLp.height == h && panelLp.x == sx && panelLp.y == sy) return
        panelLp.width = w; panelLp.height = h; panelLp.x = sx; panelLp.y = sy; panelLp.token = token
        if (panelAdded) windowManager.updateViewLayout(panel, panelLp)
        else { windowManager.addView(panel, panelLp); panelAdded = true }
    }
    private fun removePanelWindow() {
        if (!panelAdded) return
        panelAdded = false
        try { windowManager.removeViewImmediate(panel) } catch (_: IllegalArgumentException) {}
    }

    /** The engines' fingering to the panel's and the overlay's mirrors (mode entry, the panic). */
    private fun fingeringSync() {
        val st = FloatArray(10)
        NativeBridge.nativeStripState(st)
        panel.syncFingering(st[8].toInt(), st[9])
        overlay.setFingering(st[8].toInt(), st[9])
    }
    private fun quickNext() {
        val next = NativeBridge.nativeStripQuickNext(currentLayout)
        if (next != currentLayout) session.setParam("pitch_layout", next)
    }
    private fun setQuick(id: Int, on: Boolean) {
        val set = quickSwitch.value.toMutableSet(); if (on) set.add(id) else set.remove(id)
        quickSwitch.value = set
        prefs.edit().putString("quickSwitch", set.sorted().joinToString(",")).apply()
        applyQuick()
    }
    private fun applyQuick() {
        val ids = quickSwitch.value.sorted()
        NativeBridge.nativeStripQuickSet(ids.toIntArray())
        strip.setQuickAvailable(ids.isNotEmpty())
        layoutPlaySurface()
    }
    private fun setLeftHanded(on: Boolean) { leftHanded.value = on; prefs.edit().putBoolean("leftHanded", on).apply(); applyMirror() }
    /** Left-handed (QOL §2): the overlay mirrors the lattice and the touches' x, hostmpe the hand's
     *  horizontal delta, the strip and the panel swap sides. */
    private fun applyMirror() {
        val m = leftHanded.value || argMirror
        overlay.setMirror(m); panel.setMirror(m); NativeBridge.nativeSetMirror(m)
        layoutPlaySurface()
    }
    private fun setFingeringHorizontal(on: Boolean) { fingeringHorizontal.value = on; prefs.edit().putBoolean("fingeringHorizontal", on).apply(); applyOrientation() }
    private fun applyOrientation() {
        panel.setOrientation(if (fingeringHorizontal.value || argHorizontal) FingeringPanelView.Orientation.HORIZONTAL else FingeringPanelView.Orientation.VERTICAL)
        layoutPlaySurface()
    }

    /** Step 64 (QOL §2, DECISIONS_5 #7): a known controller appearing is OFFERED its input mode — one alert
     *  per device per launch; Use it applies, Not now dismisses; nothing is ever applied by itself. */
    private fun offer(name: String) {
        val prof = NativeBridge.nativeDeviceProfile(name)
        if (prof.isEmpty() || !offered.add(name)) return
        val family = prof.substringBefore('|'); val mode = prof.substringAfter('|').toIntOrNull() ?: 0
        Log.i(TAG, "[offer] $name is a $family (mode $mode)")
        val modeName = when (mode) { 1 -> "MPE"; 2 -> "Classic keyboard"; 3 -> "Wind"; else -> "any" }
        val b = android.app.AlertDialog.Builder(this).setTitle("$family connected")
        if (mode == 0) b.setMessage("$name is a $family. Its control map is the default; nothing to change.").setPositiveButton("OK", null)
        else b.setMessage("$name plays best as $modeName. Use that input dialect now?")
            .setPositiveButton("Use it") { _, _ -> session.patch(org.json.JSONObject().put("input_mode", mode)) }
            .setNegativeButton("Not now", null)
        b.show()
    }

    // -- step 64's evidence: the fingering demo (the iPad's runFingeringDemo, one for one) ---------------

    private fun nowS() = System.nanoTime() / 1e9
    private fun valve(k: Int, down: Boolean) {
        if (down) NativeBridge.nativeStripValveDown(k) else NativeBridge.nativeStripValveUp(k)
        val v = if (down) panel.valves or (1 shl k) else panel.valves and (1 shl k).inv()
        panel.syncFingering(v, panel.slide); overlay.setFingering(v, panel.slide)
    }
    private fun slideTo(pos: Float) {
        val q = Math.round(pos.coerceIn(0f, 1f) * 127f) / 127f   // as the engine sends it
        NativeBridge.nativeStripSlideSet(q)
        panel.syncFingering(panel.valves, q); overlay.setFingering(panel.valves, q)
    }
    /** A scripted phrase through the REAL path on the current layout — the trumpet's valves under a held
     *  partial (valve legato), the trombone's slide out under a held partial (the glissando), the theremin's
     *  sweep, a scale elsewhere — so the byte log carries what a hand would send. The cells come from the
     *  probe (one source of truth); the valves and the slide from the strip's engines. */
    private fun runFingeringDemo() {
        if (demoRunning || overlay.width <= 0 || overlay.height <= 0) return
        demoRunning = true
        val layout = currentLayout
        val aspect = overlay.width.toFloat() / overlay.height.toFloat()
        val out = FloatArray(8)
        fun cellOf(note: Int): FloatArray? {
            for (iy in 0 until 54) for (ix in 0 until 96) {
                val x = (ix + 0.5f) / 96f; val y = (iy + 0.5f) / 54f
                if (NativeBridge.nativeLayoutProbe(x, y, aspect, panel.valves, panel.slide, out) && out[0].toInt() == note) return out.copyOf()
            }
            return null
        }
        fun begin(c: FloatArray): Int = NativeBridge.nativeTouchBegin(nowS(), c[0].toInt(), 96, c[3], c[4] / c[6], c[5] / c[6], 0f, c[1], c[2])
        val steps = ArrayList<Pair<Long, () -> Unit>>()
        var t = 0L
        fun at(dtMs: Long, f: () -> Unit) { t += dtMs; steps.add(t to f) }
        var voice = -1
        Log.i(TAG, "[demo] fingering demo on layout $layout")
        when (layout) {
            8 -> {   // B♭3 open, held; valve 2 down (A3), 1+2 (A♭3), 1+2+3 (E3) — the legato; up; lift; the open series
                at(0) { cellOf(70)?.let { voice = begin(it) } }
                at(300) { NativeBridge.nativeTouchUpdate(voice, 0f, -0.03f) }
                at(200) { valve(1, true) }
                at(100) { valve(0, true) }
                at(0) { valve(2, true) }
                at(200) { NativeBridge.nativeTouchEnd(voice, 64) }
                at(200) { valve(0, false); valve(1, false); valve(2, false) }
                for (n in listOf(58, 65, 74, 77, 82)) { at(350) { cellOf(n)?.let { voice = begin(it) } }; at(300) { NativeBridge.nativeTouchEnd(voice, 64) } }
            }
            9 -> {   // the slide in; B♭3's partial held; the slide out to the 7th over two seconds; lift
                at(0) { slideTo(0f) }
                at(200) { cellOf(70)?.let { voice = begin(it) } }
                at(300) { NativeBridge.nativeTouchUpdate(voice, 0f, -0.03f) }
                for (i in 1..40) at(50) { slideTo(i / 40f) }
                at(400) { NativeBridge.nativeTouchEnd(voice, 64) }
                at(300) { slideTo(0f) }
            }
            12 -> {   // the hand lands at C4 and slides two octaves across the field in two seconds, pushing up
                val x0 = 0.08f + (24.5f / 61f) * 0.84f
                at(0) { if (NativeBridge.nativeLayoutProbe(x0, 0.5f, aspect, 0, 0f, out)) { val off = (x0 - out[1]) * aspect / out[6]; voice = NativeBridge.nativeThereminBegin(nowS(), out[0].toInt(), off, 96, out[3]) } }
                for (i in 1..80) at(25) { val x = x0 + (24f / 61f) * 0.84f * i / 80f; if (NativeBridge.nativeLayoutProbe(x, 0.5f, aspect, 0, 0f, out)) { val off = (x - out[1]) * aspect / out[6]; NativeBridge.nativeThereminMove(voice, out[0].toInt(), off, -0.25f * i / 80f) } }
                at(300) { NativeBridge.nativeTouchEnd(voice, 64) }
            }
            else -> for (n in listOf(60, 62, 64, 65, 67, 69, 71, 72)) { at(300) { cellOf(n)?.let { voice = begin(it) } }; at(250) { NativeBridge.nativeTouchEnd(voice, 64) } }
        }
        at(500) { demoRunning = false; NativeBridge.nativeFlushLogs(); Log.i(TAG, "DEMO_DONE") }
        for ((whenMs, f) in steps) tickHandler.postDelayed(f, whenMs)
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
        override val leftHanded get() = this@MainActivity.leftHanded.value
        override fun setLeftHanded(on: Boolean) = this@MainActivity.setLeftHanded(on)
        override val fingeringHorizontal get() = this@MainActivity.fingeringHorizontal.value
        override fun setFingeringHorizontal(on: Boolean) = this@MainActivity.setFingeringHorizontal(on)
        override val quickSwitch get() = this@MainActivity.quickSwitch.value
        override fun setQuick(id: Int, on: Boolean) = this@MainActivity.setQuick(id, on)
        override fun fingeringDemo() { showSettings.value = false; tickHandler.postDelayed({ runFingeringDemo() }, 600) }
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
        override val sound: Sound get() = this@MainActivity.sound
        override fun importInstrumentFile() { pickInstrumentFile.launch(arrayOf("*/*")) }
        override fun importInstrumentFolder() { pickInstrumentFolder.launch(null) }
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
        override val replayStatus get() = replayState.value
        override fun replayRecord() = this@MainActivity.replayRecord()
        override fun replayStopRecording() { NativeBridge.nativeReplayRecordStop(null) }
        override fun replayPlay(name: String) { showSettings.value = false; NativeBridge.nativeReplayPlay(java.io.File(java.io.File(filesDir, "Replays"), name).absolutePath) }
        override fun replayStopPlay() = NativeBridge.nativeReplayStopPlay()
        override fun replayNames() = this@MainActivity.replayNames()
        override fun replayDelete(name: String) { java.io.File(java.io.File(filesDir, "Replays"), name).delete() }
        override fun replayExport(name: String) { pendingReplayExport = name; replayExportLauncher.launch(name) }
        override fun replayImport() = replayImportLauncher.launch(arrayOf("*/*"))
        override fun dismiss() { showSettings.value = false; NativeBridge.nativeFlushLogs(); session.save() }
    }

    private fun toast(msg: String) =
        android.widget.Toast.makeText(this, msg, android.widget.Toast.LENGTH_LONG).show()

    private fun panic() {
        overlay.releaseAll()
        // step 64: the native panic releases every voice, silences the zone AND resets the strip's held
        // state (sustain off, the valves up, the spring home — hostmpe_strip_reset); the mirrors re-read.
        NativeBridge.nativePanic()
        strip.post { strip.syncMirrors(sustainToggle.value); fingeringSync() }
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
            layoutExtras = JSONObject().put("trumpet_arc", if (intent.getStringExtra("trumpetArc") != null) 1 else 0)
            if (intent.hasExtra("stringTuning")) layoutExtras.put("string_tuning", intent.getIntExtra("stringTuning", 0).coerceIn(0, 2))
            if (id in 0..12) layoutFromIntent = id
        }
        intent.getStringExtra("playMode")?.let { setPlayMode(it == "1" || it == "true") }
        // step 64's evidence: transient overrides and the demo
        intent.getStringExtra("mirror")?.let { argMirror = it == "1" || it == "true"; if (::panel.isInitialized) applyMirror() }
        intent.getStringExtra("fingeringHorizontal")?.let { argHorizontal = it == "1" || it == "true"; if (::panel.isInitialized) applyOrientation() }
        if (intent.getStringExtra("fingeringDemo") != null) tickHandler.postDelayed({ runFingeringDemo() }, 3000)
        intent.getStringExtra("recordLab")?.toIntOrNull()?.let { s -> tickHandler.postDelayed({ runLabRecording(s) }, 2000) }   // step 65's evidence
        intent.getStringExtra("replayFile")?.let { n -> tickHandler.postDelayed({ NativeBridge.nativeReplayPlay(java.io.File(java.io.File(filesDir, "Replays"), n).absolutePath) }, 2000) }
        intent.getStringExtra("transports")?.let { spec ->
            val parts = spec.split(",").map { it.trim().lowercase() }
            setTransports("usb" in parts, "virtual" in parts, "ble" in parts)
        }
        // Step 54: `--ei voxoBudgetMb N` caps the gate's advice, `--es voxoInstrument <relative path | demo | ->` picks ("-" = the sine).
        val budgetMb = intent.getIntExtra("voxoBudgetMb", 0)
        val inst = intent.getStringExtra("voxoInstrument")
        val src = intent.getStringExtra("voxoSource")
        val patch = if (intent.hasExtra("suzuPatch")) intent.getIntExtra("suzuPatch", 0) else -1
        if ((budgetMb > 0 || inst != null || src != null || patch >= 0) && ::sound.isInitialized) sound.applyLabExtras(budgetMb, if (inst == "-") "" else inst, src, patch)
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

/** The one native frame of the play surface (step 54): instrumented at step 64 to read the device's event
 *  streams off logcat (the author: the S-Pen on the partials while a finger holds a valve). */
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
    // Phase 7 step 54 (DECISIONS_6 #27): Voxo's bus, numbered from 1000 (presets/SCHEMA.md).
    private val voxoNames = listOf("Reverb amount", "Reverb room", "Reverb damping", "Delay amount", "Delay time", "Delay feedback")
    fun ctlName(t: Int): String = if (t >= 1000) (voxoNames.getOrNull(t - 1000) ?: "?") else (ctlNames.getOrNull(t) ?: "?")
    /** Every target the picker cycles through: the core's, then the bus. */
    val ctlIds: List<Int> = (ctlNames.indices).toList() + voxoNames.indices.map { 1000 + it }
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

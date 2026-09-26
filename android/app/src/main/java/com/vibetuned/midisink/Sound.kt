// Sound.kt — the Tab's side of Voxo (Phase 7 step 54, SOUND §4; DECISIONS_6
// #28–#29): the settings (on by default with the demo, the volume, the chosen
// instrument, Local Control), the device with the activity's foreground and
// under audio focus (a call or another player takes it: stop; it returns:
// start), the instrument library in filesDir/Instruments (imported through
// the Storage Access Framework — a .dslibrary, or the folder holding a
// .dspreset and its samples — or dropped in by hand), the demo copied out of
// the assets once per build, the load on a worker behind the advisory gate
// (60% of what the activity manager says is available), the stats line that
// also paces the AAudio buffer tuner, and the play surface's reach.
package com.vibetuned.midisink

import android.app.ActivityManager
import android.content.Context
import android.content.SharedPreferences
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import java.io.File
import kotlin.concurrent.thread

class Sound(private val ctx: Context, private val prefs: SharedPreferences) {
    val enabled = mutableStateOf(prefs.getBoolean("soundEnabled", true))     // first launch makes a sound (SOUND §3)
    val gain = mutableStateOf(prefs.getFloat("soundGain", 0.8f))
    val localControl = mutableStateOf(prefs.getBoolean("soundLocalControl", true))
    /** "demo", a path relative to filesDir/Instruments, or "" = the sine. */
    val instrument = mutableStateOf(prefs.getString("soundInstrument", "demo") ?: "demo")
    val instruments = mutableStateOf<List<String>>(emptyList())
    val report = mutableStateOf("")
    val loading = mutableStateOf(false)
    val status = mutableStateOf("")
    val advice = mutableStateOf("")

    private var resumed = false
    private var focus = false
    private var budgetOverride: Long? = null
    private var loadGeneration = 0
    var onReachChanged: ((ByteArray?) -> Unit)? = null

    private val audio = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val focusRequest: AudioFocusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
        .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
        .setAcceptsDelayedFocusGain(false)
        .setOnAudioFocusChangeListener { change ->
            when (change) {
                AudioManager.AUDIOFOCUS_GAIN -> { focus = true; apply() }
                AudioManager.AUDIOFOCUS_LOSS, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> { focus = false; apply() }
            }
            Log.i(TAG, "audio focus change $change")
        }
        .build()

    val instrumentsDir: File get() = File(ctx.filesDir, "Instruments").also { it.mkdirs() }
    private val demoDir: File get() = File(ctx.filesDir, "demo")

    /** Once the native side exists: the demo out of the assets, the list, the instrument, the device. */
    fun start() {
        installDemo()
        NativeBridge.nativeVoxoSetLocalControl(localControl.value)
        refreshInstruments()
        loadInstrument()
        apply()
    }

    fun setEnabled(on: Boolean) { enabled.value = on; prefs.edit().putBoolean("soundEnabled", on).apply(); apply(); pushReach() }
    fun setGain(g: Float) { gain.value = g.coerceIn(0f, 1.5f); prefs.edit().putFloat("soundGain", gain.value).apply(); NativeBridge.nativeVoxoSetGain(gain.value) }
    fun setLocalControl(on: Boolean) { localControl.value = on; prefs.edit().putBoolean("soundLocalControl", on).apply(); NativeBridge.nativeVoxoSetLocalControl(on) }
    fun setInstrument(rel: String) { instrument.value = rel; prefs.edit().putString("soundInstrument", rel).apply(); loadInstrument() }

    fun onResume() { resumed = true; apply() }
    fun onPause() { resumed = false; apply() }

    /** The device runs only in the foreground, with the setting on and the focus held (SOUND §4). */
    private fun apply() {
        NativeBridge.nativeVoxoSetGain(gain.value)
        val want = enabled.value && resumed
        if (want && !focus) {
            focus = audio.requestAudioFocus(focusRequest) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
            if (!focus) Log.w(TAG, "audio focus refused")
        }
        if (!want && focus) { audio.abandonAudioFocusRequest(focusRequest); focus = false }
        val run = want && focus
        NativeBridge.nativeVoxoSetEnabled(run)
        if (!run) status.value = if (!enabled.value) "Off." else if (!resumed) "Paused with the app." else "Waiting for audio focus."
    }

    /** Once a second from the activity: the status line (which paces the tuner), and a restart if the device dropped. */
    fun tick() {
        val s = NativeBridge.nativeVoxoStatus()
        val parts = s.split("|")
        val running = parts.getOrNull(1) == "1"
        if (running) status.value = parts[0]
        else if (enabled.value && resumed && focus) NativeBridge.nativeVoxoSetEnabled(true)   // the spike stopped it, or AAudio dropped it
    }

    // -- the instruments --------------------------------------------------------

    private fun installDemo() {
        val marker = File(demoDir, ".version")
        val want = BuildConfig.VERSION_CODE.toString() + "-" + BuildConfig.BUILD_DESCRIBE
        if (marker.exists() && marker.readText() == want) return
        demoDir.deleteRecursively(); demoDir.mkdirs(); File(demoDir, "Samples").mkdirs()
        try {
            val am = ctx.assets
            for (name in am.list("demo") ?: emptyArray()) {
                if (name == "Samples") continue
                am.open("demo/$name").use { i -> File(demoDir, name).outputStream().use { o -> i.copyTo(o) } }
            }
            for (name in am.list("demo/Samples") ?: emptyArray()) {
                am.open("demo/Samples/$name").use { i -> File(demoDir, "Samples/$name").outputStream().use { o -> i.copyTo(o) } }
            }
            marker.writeText(want)
            Log.i(TAG, "demo instrument installed to $demoDir")
        } catch (e: Exception) {
            Log.e(TAG, "demo install failed: $e")
        }
    }

    fun refreshInstruments() {
        val root = instrumentsDir
        val found = ArrayList<String>()
        root.walkTopDown().forEach { f ->
            if (f.isFile && (f.name.endsWith(".dspreset", true) || f.name.endsWith(".dslibrary", true)))
                found.add(f.relativeTo(root).path)
        }
        instruments.value = found.sorted()
        Log.i(TAG, "instruments in $root: ${instruments.value.joinToString()}")
    }

    /** A picked FILE (a .dslibrary or a lone .dspreset): copied into Instruments; the relative name. */
    fun importDocument(uri: Uri): String? {
        val name = displayName(uri) ?: return null
        val ext = name.substringAfterLast('.', "").lowercase()
        if (ext != "dslibrary" && ext != "dspreset") { report.value = "Not a Decent Sampler library or preset: $name"; return null }
        return try {
            ctx.contentResolver.openInputStream(uri)?.use { i -> File(instrumentsDir, name).outputStream().use { o -> i.copyTo(o) } }
            refreshInstruments(); name
        } catch (e: Exception) { report.value = "Import failed: $e"; null }
    }

    /** A picked FOLDER (the preset with its Samples/): copied whole; the first preset inside. */
    fun importTree(tree: Uri): String? {
        val name = displayName(tree) ?: tree.lastPathSegment?.substringAfterLast('/') ?: "Instrument"
        val dest = File(instrumentsDir, name)
        dest.deleteRecursively()
        return try {
            copyTree(tree, DocumentsContract.getTreeDocumentId(tree), dest)
            refreshInstruments()
            instruments.value.firstOrNull { it.startsWith("$name/") }
        } catch (e: Exception) { report.value = "Import failed: $e"; null }
    }

    private fun copyTree(tree: Uri, docId: String, dest: File) {
        dest.mkdirs()
        val children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId)
        ctx.contentResolver.query(children, arrayOf(DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE), null, null, null)?.use { c ->
            while (c.moveToNext()) {
                val id = c.getString(0); val nm = c.getString(1); val mime = c.getString(2)
                if (mime == DocumentsContract.Document.MIME_TYPE_DIR) copyTree(tree, id, File(dest, nm))
                else {
                    val fileUri = DocumentsContract.buildDocumentUriUsingTree(tree, id)
                    ctx.contentResolver.openInputStream(fileUri)?.use { i -> File(dest, nm).outputStream().use { o -> i.copyTo(o) } }
                }
            }
        }
    }

    private fun displayName(uri: Uri): String? =
        ctx.contentResolver.query(uri, arrayOf(DocumentsContract.Document.COLUMN_DISPLAY_NAME), null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getString(0) else null
        }

    fun deleteInstrument(rel: String) {
        File(instrumentsDir, rel.substringBefore('/')).deleteRecursively()
        if (instrument.value == rel) setInstrument("")
        refreshInstruments()
    }

    private fun instrumentPath(rel: String): File? = when {
        rel == "demo" -> File(demoDir, "demo.dspreset")
        rel.isEmpty() -> null
        else -> File(instrumentsDir, rel)
    }

    /** The advisory gate's advice: 60% of what the activity manager says is available (or the lab's override). */
    private fun refreshBudget(): Long {
        val mi = ActivityManager.MemoryInfo()
        (ctx.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager).getMemoryInfo(mi)
        val budget = budgetOverride ?: (mi.availMem / 10 * 6)
        advice.value = if (budgetOverride != null) "Memory advised for instruments: %d MB (the lab's override).".format(budget / 1048576)
                       else "Memory advised for instruments: %d MB (of %d MB available on the device).".format(budget / 1048576, mi.availMem / 1048576)
        return budget
    }

    private fun loadInstrument() {
        loadGeneration++
        val gen = loadGeneration
        val path = instrumentPath(instrument.value)
        if (path == null) { NativeBridge.nativeVoxoUnload(); report.value = ""; pushReach(); return }
        NativeBridge.nativeVoxoSetBudget(refreshBudget())
        loading.value = true
        report.value = "Loading ${path.name} …"
        thread(name = "voxo-load") {
            val r = NativeBridge.nativeVoxoLoad(path.absolutePath)
            val ok = r.startsWith("OK\n")
            val text = r.substringAfter('\n')
            (ctx as? android.app.Activity)?.runOnUiThread {
                if (gen != loadGeneration) return@runOnUiThread
                loading.value = false
                report.value = if (ok) text else "Could not load ${path.name}: $text"
                Log.i(TAG, "[voxo] ${report.value}")
                pushReach()
            }
        }
    }

    private fun pushReach() { onReachChanged?.invoke(if (enabled.value) NativeBridge.nativeVoxoCoveredNotes() else null) }

    /** The lab's intent extras (step 54's evidence): the budget override and the instrument. */
    fun applyLabExtras(budgetMb: Int, instrumentRel: String?) {
        if (budgetMb > 0) budgetOverride = budgetMb.toLong() * 1048576L
        if (instrumentRel != null) setInstrument(instrumentRel) else if (budgetMb > 0) loadInstrument()
    }

    companion object { private const val TAG = "sumi-shell" }
}

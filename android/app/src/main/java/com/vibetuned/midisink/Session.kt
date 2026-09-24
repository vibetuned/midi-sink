package com.vibetuned.midisink

import android.content.SharedPreferences
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Step 45b (DECISIONS_5 #79, the iPad's #73): THE SESSION — everything the
 * shell holds that is not the field: the core params (one sumi_params_t), the
 * custom palette, the CC map, the input dialect, the routed controls' values
 * and the strip's latch-wheel CCs. It lives NATIVELY as one sumi_preset_t and
 * is written and read only by the one C serializer (presets/): Kotlin sees it
 * as that serializer's JSON and changes it with small JSON patches, which the
 * serializer reads over the session (keys present overwrite, missing keys
 * keep — a whole preset file is such a patch). So the bytes on disk are the
 * desktop's, the iPad's and the web page's.
 *
 * Files: filesDir/last_session.json (written on every change, debounced, and
 * on pause; restored at launch) and filesDir/Presets/<name>.json. The shell's
 * own switches (Play mode, transports, the sustain latch, touch-size
 * velocity, the strip's visibility) stay SharedPreferences.
 */
class SessionStore(private val filesDir: File, private val prefs: SharedPreferences) {
    /** The session as the serializer wrote it; empty until the instance has seeded it. */
    val json = mutableStateOf(JSONObject())
    val ready = mutableStateOf(false)
    val presetNames = mutableStateOf(listOf<String>())
    private val main = Handler(Looper.getMainLooper())
    private val saveTask = Runnable { save() }

    val sessionFile get() = File(filesDir, "last_session.json")
    val presetsDir: File get() = File(filesDir, "Presets").also { it.mkdirs() }

    /** Called in onCreate, before the surface: hands the last session (or a
     *  one-time migration of the 0.x rows) to the native side. */
    fun init() {
        val text = if (sessionFile.exists()) sessionFile.readText() else null
        val legacy = if (text == null) legacyPatch() else null
        if (legacy != null) Log.i(TAG, "[session] no last session: migrating the 0.x settings once")
        NativeBridge.nativeSessionInit(text, legacy)
        refreshPresetNames()
        pollReady()
    }

    private fun pollReady() {
        val t = NativeBridge.nativeSessionJson(null)
        if (t.isEmpty()) { main.postDelayed({ pollReady() }, 100); return }
        json.value = JSONObject(t)
        ready.value = true
        onChange?.invoke()
        save()
    }

    /** The shell listens (theme, layout, strip assignments). */
    var onChange: (() -> Unit)? = null

    /** A JSON patch over the session; false when it is not a preset. */
    fun patch(p: JSONObject): Boolean = patchText(p.toString())
    fun patchText(text: String): Boolean {
        if (!ready.value) return false
        val t = NativeBridge.nativeSessionPatch(text)
        if (t.isEmpty()) return false
        json.value = JSONObject(t)
        onChange?.invoke()
        main.removeCallbacks(saveTask)
        main.postDelayed(saveTask, 800)
        return true
    }

    // -- reading the session ---------------------------------------------------------
    val params: JSONObject get() = json.value.optJSONObject("params") ?: JSONObject()
    val palette: JSONObject get() = json.value.optJSONObject("palette") ?: JSONObject()
    fun f(key: String, def: Float = 0f): Float = params.optDouble(key, def.toDouble()).toFloat()
    fun u(key: String, def: Int = 0): Int = params.optInt(key, def)
    val anod: Boolean get() = u("medium") == 1
    val inputMode: Int get() = json.value.optInt("input_mode", 1)

    fun setParam(key: String, v: Number) = patch(JSONObject().put("params", JSONObject().put(key, v)))
    fun setParams(vararg kv: Pair<String, Any>) {
        val o = JSONObject(); for ((k, v) in kv) o.put(k, v)
        patch(JSONObject().put("params", o))
    }
    fun setPalette(pal: JSONObject) = patch(JSONObject().put("palette", pal))

    /** The CC map as (channel, cc, target) rows. */
    fun routes(): List<CcMap.Route> {
        val a = json.value.optJSONArray("cc_map") ?: return emptyList()
        return (0 until a.length()).mapNotNull { i ->
            val r = a.optJSONArray(i) ?: return@mapNotNull null
            CcMap.Route(r.optInt(0), r.optInt(1), r.optInt(2))
        }
    }
    fun setRoutes(rs: List<CcMap.Route>) {
        val a = JSONArray(); for (r in rs) a.put(JSONArray().put(r.channel).put(r.cc).put(r.target))
        patch(JSONObject().put("cc_map", a))
    }
    fun routeFor(target: Int): Int? = routes().firstOrNull { it.target == target }?.cc

    /** The routed controls' values (ctl -> 0..127). */
    fun controls(): Map<Int, Int> {
        val a = json.value.optJSONArray("controls") ?: return emptyMap()
        return (0 until a.length()).mapNotNull { i -> a.optJSONArray(i)?.let { it.optInt(0) to it.optInt(1) } }.toMap()
    }
    fun control(ctl: Int): Int = controls()[ctl] ?: 0
    fun setControl(ctl: Int, v: Int) {
        val m = controls().toMutableMap(); m[ctl] = v.coerceIn(0, 127)
        val a = JSONArray(); for ((k, x) in m.toSortedMap()) a.put(JSONArray().put(k).put(x))
        patch(JSONObject().put("controls", a))
    }
    fun stripAssign(): Pair<Int, Int> {
        val s = json.value.optJSONObject("strip") ?: return 0 to 0
        return s.optInt("assign_a") to s.optInt("assign_b")
    }
    fun setStripAssign(a: Int, b: Int) = patch(JSONObject().put("strip", JSONObject().put("assign_a", a).put("assign_b", b)))

    // -- files -------------------------------------------------------------------------
    fun write(name: String?): String = NativeBridge.nativeSessionJson(name)

    fun save() {
        if (!ready.value) return
        try { sessionFile.writeText(write("last session")) } catch (e: Exception) { Log.w(TAG, "[session] save failed: $e") }
    }

    fun refreshPresetNames() {
        presetNames.value = (presetsDir.listFiles() ?: emptyArray())
            .filter { it.extension.lowercase() == "json" }.map { it.nameWithoutExtension }.sorted()
    }
    fun presetFile(name: String) = File(presetsDir, safeName(name) + ".json")
    fun savePreset(name: String): Boolean = try {
        presetFile(name).writeText(write(name)); refreshPresetNames(); true
    } catch (e: Exception) { false }
    fun loadPreset(name: String): Boolean {
        val f = presetFile(name)
        return f.exists() && patchText(f.readText())
    }
    fun deletePreset(name: String) { presetFile(name).delete(); refreshPresetNames() }
    /** An imported file is applied and kept in the library under its own name. */
    fun importText(text: String, fallbackName: String): String? {
        if (!patchText(text)) return null
        val named = try { JSONObject(text).optString("name", "") } catch (e: Exception) { "" }
        val name = if (named.isEmpty() || named == "last session") fallbackName else named
        presetFile(name).writeText(text)
        refreshPresetNames()
        return name
    }

    // -- the 0.x rows (SharedPreferences) -> a patch, once -----------------------------
    /** Only rows that EXIST: an untouched bend picker lands on "Medium default". */
    private fun legacyPatch(): String? {
        val keys = listOf("palette", "viscosity", "inkFeed", "roughness", "bpm", "rollSpeed", "vortexRankine", "rippleAngle",
            "slidePinch", "pinchCrossed", "pressSwirl", "bendRipple", "wakeViscous", "wakeSpread", "inputMode", "ccMap",
            "rippleAmount", "rippleWavelength", "layout")
        if (keys.none { prefs.contains(it) }) return null
        val p = JSONObject()
        fun has(k: String) = prefs.contains(k)
        if (has("palette")) p.put("active_palette_id", prefs.getInt("palette", 0).coerceIn(0, 2))
        if (has("viscosity")) p.put("fluid_viscosity", prefs.getFloat("viscosity", 0.5f))
        if (has("inkFeed")) p.put("expansion_rate", prefs.getFloat("inkFeed", 1f))
        if (has("roughness")) p.put("paper_roughness", prefs.getFloat("roughness", 0.5f))
        if (has("bpm")) p.put("bpm", prefs.getFloat("bpm", 120f))
        if (has("rollSpeed")) p.put("roll_speed", prefs.getFloat("rollSpeed", 0.0625f))
        if (has("vortexRankine")) p.put("vortex_profile", if (prefs.getBoolean("vortexRankine", false)) 1 else 0)
        if (has("rippleAngle")) p.put("ripple_angle", prefs.getFloat("rippleAngle", 0f) / 57.29578f)
        if (has("slidePinch")) p.put("slide_mode", if (prefs.getBoolean("slidePinch", false)) 1 else 0)
        if (has("pinchCrossed")) p.put("pinch_variant", if (prefs.getBoolean("pinchCrossed", false)) 1 else 0)
        if (has("pressSwirl")) p.put("press_mode", if (prefs.getBoolean("pressSwirl", false)) 1 else 0)
        if (has("bendRipple")) { val r = prefs.getBoolean("bendRipple", false); p.put("bend_mode", if (r) 1 else 0); p.put("ripple_bake", if (r) 1 else 0) }
        if (has("wakeViscous")) p.put("wake_profile", if (prefs.getBoolean("wakeViscous", false)) 1 else 0)
        if (has("wakeSpread")) p.put("wake_spread", prefs.getFloat("wakeSpread", 3f))
        if (has("layout")) p.put("pitch_layout", prefs.getInt("layout", 0).coerceIn(0, 7))
        val out = JSONObject().put("params", p)
        if (has("inputMode")) out.put("input_mode", prefs.getInt("inputMode", 1).coerceIn(1, 3))
        if (has("ccMap")) {
            val a = JSONArray()
            for (r in CcMap.decode(prefs.getString("ccMap", "") ?: "")) a.put(JSONArray().put(r.channel).put(r.cc).put(r.target))
            out.put("cc_map", a)
        }
        if (has("rippleAmount") || has("rippleWavelength")) {
            val c = CcMap.controlDefaults.toMutableMap()
            if (has("rippleAmount")) c[7] = prefs.getInt("rippleAmount", 0)
            if (has("rippleWavelength")) c[8] = prefs.getInt("rippleWavelength", 32)
            val a = JSONArray(); for ((k, v) in c.toSortedMap()) a.put(JSONArray().put(k).put(v))
            out.put("controls", a)
        }
        return out.toString()
    }

    companion object {
        private const val TAG = "sumi-shell"
        fun safeName(n: String): String {
            val s = n.map { if ("/\\:\"<>|?*".contains(it)) '_' else it }.joinToString("").trim()
            return s.ifEmpty { "preset" }
        }
    }
}

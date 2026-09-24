package com.vibetuned.midisink

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import kotlinx.coroutines.delay
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.pow
import kotlin.math.roundToInt

/**
 * Step 45b (DECISIONS_5 #79): the settings sheet — the iPad's pages
 * (SumiApp.swift / SettingsPages.swift) with the desktop window's names and
 * ranges. Foundation-only Compose (no Material): continuous values are
 * « ‹ value › » step rows at the desktop sliders' useful grain, choices are
 * radio lists, colours are sRGB channel steps over the linear model.
 * Everything the session holds reads from and writes to [SessionStore];
 * the shell's own switches come in through [SheetHost].
 */
interface SheetHost {
    val playMode: Boolean
    val playEffective: Boolean
    fun setPlayMode(on: Boolean)
    val velocityFromTouchSize: Boolean
    fun setVelocityFromTouchSize(on: Boolean)
    val showStrip: Boolean
    fun setShowStrip(on: Boolean)
    val sustainToggle: Boolean
    fun setSustainToggle(on: Boolean)
    val outUsb: Boolean
    val outVirtual: Boolean
    val outBle: Boolean
    fun setTransports(usb: Boolean, virt: Boolean, ble: Boolean)
    val usbStatus: String
    val bleStatus: String
    val virtualClients: Int
    val selfTestResult: String
    fun resync()
    fun panic()
    fun storm()
    fun selfTest()
    fun pairBluetooth()
    fun dip(keep: Boolean)
    fun exportPreset(name: String)
    fun importPreset()
    fun sharePreset(name: String)
    fun exportPrint(id: Int, choice: Int, alpha: Boolean)
    fun saveNewestPrint()
    fun dismiss()
}

private enum class Page { MAIN, PALETTE, SUBSTRATE, PRESETS, PRINTS, OPERATORS }

private val white = Color.White
private val dim = Color(0xCCFFFFFF)
private val faint = Color(0x77FFFFFF)

@Composable private fun Title(t: String) =
    BasicText(t, style = TextStyle(color = Color(0x88FFFFFF), fontSize = 12.sp), modifier = Modifier.padding(top = 14.dp, bottom = 4.dp))
@Composable private fun Note(t: String) =
    BasicText(t, style = TextStyle(color = faint, fontSize = 12.sp), modifier = Modifier.padding(top = 2.dp, bottom = 4.dp))
@Composable private fun Action(t: String, color: Color = dim, enabled: Boolean = true, onClick: () -> Unit) =
    BasicText(t, style = TextStyle(color = if (enabled) color else Color(0x44FFFFFF), fontSize = 15.sp),
        modifier = Modifier.fillMaxWidth().clickable(enabled = enabled) { onClick() }.padding(vertical = 9.dp))
@Composable private fun Toggle(t: String, on: Boolean, onClick: () -> Unit) =
    BasicText((if (on) "●  " else "○  ") + t, style = TextStyle(color = if (on) white else dim, fontSize = 15.sp),
        modifier = Modifier.fillMaxWidth().clickable { onClick() }.padding(vertical = 8.dp))
/** A radio list: (value, label). */
@Composable private fun Choice(opts: List<Pair<Int, String>>, cur: Int, onPick: (Int) -> Unit) =
    opts.forEach { (v, l) -> Toggle(l, v == cur) { onPick(v) } }
@Composable private fun Nav(t: String, onClick: () -> Unit) =
    BasicText("$t  ›", style = TextStyle(color = white, fontSize = 15.sp),
        modifier = Modifier.fillMaxWidth().clickable { onClick() }.padding(vertical = 10.dp))
@Composable private fun Cycle(label: String, value: String, onNext: () -> Unit) =
    Row(Modifier.fillMaxWidth().clickable { onNext() }.padding(vertical = 9.dp)) {
        BasicText(label, style = TextStyle(color = dim, fontSize = 15.sp), modifier = Modifier.weight(1f))
        BasicText("$value  ›", style = TextStyle(color = white, fontSize = 15.sp))
    }
/** « ‹ value › » — k = ±1 fine, ±big coarse. */
@Composable private fun Step(label: String, value: String, big: Int = 5, enabled: Boolean = true, onStep: (Int) -> Unit) {
    val st = TextStyle(color = if (enabled) dim else Color(0x44FFFFFF), fontSize = 15.sp)
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        BasicText(label, style = st, modifier = Modifier.weight(1f))
        for ((sym, k) in listOf(" «" to -big, "‹" to -1)) BasicText(sym, style = st, modifier = Modifier.clickable(enabled = enabled) { onStep(k) }.padding(horizontal = 7.dp))
        BasicText(value, style = TextStyle(color = if (enabled) white else Color(0x44FFFFFF), fontSize = 15.sp), modifier = Modifier.padding(horizontal = 6.dp))
        for ((sym, k) in listOf("›" to 1, "» " to big)) BasicText(sym, style = st, modifier = Modifier.clickable(enabled = enabled) { onStep(k) }.padding(horizontal = 7.dp))
    }
}
/** A float param of the session: range, fine step, format. */
@Composable private fun FParam(s: SessionStore, label: String, key: String, lo: Float, hi: Float, step: Float, fmt: String, big: Int = 5) {
    val v = s.f(key)
    Step(label, fmt.format(v), big) { k -> s.setParam(key, (((v + k * step).coerceIn(lo, hi)) * 1e5f).roundToInt() / 1e5f) }
}
@Composable private fun UParam(s: SessionStore, label: String, key: String, lo: Int, hi: Int, fmt: String = "%d") {
    val v = s.u(key)
    Step(label, fmt.format(v), 1) { k -> s.setParam(key, (v + k).coerceIn(lo, hi)) }
}
/** A routed control 0..127, disabled while no CC routes it. */
@Composable private fun ControlParam(s: SessionStore, label: String, ctl: Int) {
    val v = s.control(ctl)
    Step(label, "$v", 8, enabled = s.routeFor(ctl) != null) { k -> s.setControl(ctl, v + k) }
}

// linear <-> sRGB, the pickers speak sRGB, the core linear (Session.swift)
private fun linToSrgb(v: Float): Float { val c = v.coerceIn(0f, 1f); return if (c <= 0.0031308f) 12.92f * c else 1.055f * c.toDouble().pow(1.0 / 2.4).toFloat() - 0.055f }
private fun srgbToLin(v: Float): Float { val c = v.coerceIn(0f, 1f); return if (c <= 0.04045f) c / 12.92f else ((c + 0.055f) / 1.055f).toDouble().pow(2.4).toFloat() }
private fun swatch(r: Float, g: Float, b: Float) = Color(linToSrgb(r), linToSrgb(g), linToSrgb(b))

/** Three sRGB steps (0..255) over a linear triple; `onSet` gets the new linear triple. */
@Composable private fun RgbRows(label: String, rgb: FloatArray, onSet: (FloatArray) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(top = 6.dp)) {
        BasicText(label, style = TextStyle(color = dim, fontSize = 15.sp), modifier = Modifier.weight(1f))
        Box(Modifier.size(28.dp, 18.dp).background(swatch(rgb[0], rgb[1], rgb[2])).border(1.dp, Color(0x55FFFFFF)))
    }
    for ((c, name) in listOf(0 to "  red", 1 to "  green", 2 to "  blue")) {
        val sv = (linToSrgb(rgb[c]) * 255f).roundToInt()
        Step(name, "$sv", 16) { k ->
            val out = rgb.copyOf(); out[c] = srgbToLin(((sv + k).coerceIn(0, 255)) / 255f); onSet(out)
        }
    }
}

private fun arr3(a: JSONArray?): FloatArray = FloatArray(3) { (a?.optDouble(it, 0.0) ?: 0.0).toFloat() }
private fun jarr(vararg f: Float) = JSONArray().apply { for (x in f) put(x.toDouble()) }

@Composable
fun SettingsSheet(s: SessionStore, host: SheetHost) {
    val page = remember { mutableStateOf(Page.MAIN) }
    val status = remember { mutableStateOf("") }
    LaunchedEffect(Unit) { while (true) { status.value = NativeBridge.nativeStatusLine(); delay(1000) } }
    Dialog(onDismissRequest = { if (page.value != Page.MAIN) page.value = Page.MAIN else host.dismiss() }) {
        Column(Modifier.background(Color(0xEE18143A)).padding(20.dp).fillMaxWidth().verticalScroll(rememberScrollState())) {
            if (!s.ready.value) { Note("Starting…"); return@Column }
            s.json.value   // recompose on every session change
            if (page.value != Page.MAIN) Action("‹  Settings", white) { page.value = Page.MAIN }
            when (page.value) {
                Page.MAIN -> MainPage(s, host, page, status.value)
                Page.PALETTE -> PalettePage(s)
                Page.SUBSTRATE -> SubstratePage(s)
                Page.PRESETS -> PresetsPage(s, host)
                Page.PRINTS -> PrintsPage(host)
                Page.OPERATORS -> OperatorsPage(s)
            }
        }
    }
}

private val layouts = listOf(
    0 to "Circle of fifths", 1 to "Chromatic grid (playable)", 2 to "Jankó (playable)", 3 to "Piano roll (left)",
    4 to "Piano roll (top)", 5 to "Piano grid (playable)", 6 to "Piano roll (right)", 7 to "Piano roll (bottom)")

@Composable
private fun MainPage(s: SessionStore, host: SheetHost, page: MutableState<Page>, statusLine: String) {
    val anod = s.anod
    BasicText("midi-sink", style = TextStyle(color = white, fontSize = 18.sp))

    // Canvas first: the most-used control (#77 of Part IV, #78). QOL §6: dip and clear, worded apart.
    Title("CANVAS")
    Action("Dip the paper — keep the print") { host.dip(true) }
    Action("Clear the canvas — discard", Color(0xCCFFB4A2)) { host.dip(false) }
    Note("Dip lifts the sheet as a print into the ledger — re-export it at any size — then lays fresh paper. " +
        "Clear lays fresh paper and keeps nothing. The sustain pedal does neither in Play mode: it is a musical control there.")
    val n = NativeBridge.nativeLedgerList().firstOrNull() ?: 0
    Nav("Prints — " + (if (n == 0) "none yet" else "$n this session")) { page.value = Page.PRINTS }

    Title("MEDIUM & LOOK")
    Choice(listOf(0 to "Sumi — ink on washi", 1 to "Anod — strain-glow"), s.u("medium")) { s.setParam("medium", it) }
    Note(if (anod) "Anod: the accumulated strain glows like ionized gas — the same deformation history re-read as a discharge record. Switching is live."
         else "Sumi: ink phase bands on paper. Switching is live; each medium brings its own palettes and its default expression routing.")
    Nav("Palette — " + paletteName(s)) { page.value = Page.PALETTE }
    Nav(if (anod) "Substrate — glass & glow" else "Substrate — paper") { page.value = Page.SUBSTRATE }
    Nav("Presets") { page.value = Page.PRESETS }
    Nav("Operators — Chladni, burst, spark, Chirikov") { page.value = Page.OPERATORS }
    FParam(s, "Viscosity", "fluid_viscosity", 0f, 1f, 0.01f, "%.2f")
    FParam(s, "Ink feed (pressure)", "expansion_rate", 0.1f, 4f, 0.05f, "%.2f")

    Title("LAYOUT")
    val lay = s.u("pitch_layout")
    Choice(layouts, lay) { s.setParam("pitch_layout", it) }
    if (lay == 3 || lay == 4 || lay == 6 || lay == 7) {
        FParam(s, "Tempo (BPM)", "bpm", 20f, 300f, 1f, "%.0f")
        FParam(s, "Roll speed", "roll_speed", 0.02f, 0.25f, 0.0025f, "%.4f")
        Note("Canvas lengths per beat. 1/16 keeps 4 bars of 4/4 on screen.")
    }

    Title("MODE")
    val playable = lay == 1 || lay == 2 || lay == 5
    Choice(listOf(0 to "Marble", 1 to "Play"), if (host.playMode && playable) 1 else 0) { if (playable) host.setPlayMode(it == 1) }
    Note(if (!playable) "Play mode is available on the Chromatic grid, Jankó and Piano grid layouts."
         else if (host.playMode) "Play: each touch is an MPE joystick on the lattice; the S-Pen plays legato."
         else if (anod) "Marble on the glass: tap = the strike, drag = comb, twist = torsion vortex, pinch = burst, long press = torsion feed (hold / push) and the Chladni stir (pull), pen = wake."
         else "Marble: tap = drop, drag = tine, twist = vortex, pinch = fold, long press = feed (hold / push) and swirl (pull), pen = wake.")
    if (host.playMode && playable) {
        Toggle("Velocity from touch size", host.velocityFromTouchSize) { host.setVelocityFromTouchSize(!host.velocityFromTouchSize) }
        Note("Glass has no force sensor: finger velocity is synthesized (96 fixed, or coarse touch-size modulation). The S-Pen's tip pressure is real.")
        Title("CONTROL STRIP")
        Toggle("Show the control strip", host.showStrip) { host.setShowStrip(!host.showStrip) }
        Toggle("Sustain button latches (toggle)", host.sustainToggle) { host.setSustainToggle(!host.sustainToggle) }
        Note("The strip floats top-left over the full lattice. Pitch springs back to center on release; Mod and the two " +
            "assignable wheels latch. Long-press an assignable wheel to change its CC (kept in the session). All strip traffic rides the MPE master channel.")
    }

    Title("INPUT")
    Choice(listOf(1 to "MPE", 2 to "Classic keyboard", 3 to "Wind"), s.inputMode) { s.patch(JSONObject().put("input_mode", it)) }
    Note(when (s.inputMode) {
        3 -> "Wind: one voice, played exactly as MPE — each note a strike, breath (CC 2 / 7 / 11 or channel pressure) the unbounded feed, plus a wake from note to note on every legato change."
        2 -> "Classic: every note is its own voice on any channel; bend is the global shear and the mod wheel the vortex."
        else -> "MPE (default): per-note bend, pressure and CC 74 on the member channels; a plain keyboard on channel 1 still plays chords. CC 64 never touches the canvas."
    })

    Title("EXPRESSION ROUTING")
    Cycle("Per-note bend", bendName(s.u("bend_mode"))) {
        val order = listOf(255, 0, 1, 2, 3, 4); val nx = order[(order.indexOf(s.u("bend_mode")).coerceAtLeast(0) + 1) % order.size]
        if (nx == 1) s.setParams("bend_mode" to 1, "ripple_bake" to 1) else if (nx == 0) s.setParams("bend_mode" to 0, "ripple_bake" to 0) else s.setParam("bend_mode", nx)
    }
    Cycle("Channel pressure", pressName(s.u("press_mode"))) {
        val order = listOf(255, 0, 1, 2); s.setParam("press_mode", order[(order.indexOf(s.u("press_mode")).coerceAtLeast(0) + 1) % order.size])
    }
    Cycle("Slide (CC 74)", slideName(s.u("slide_mode"))) {
        val order = listOf(255, 0, 1, 2); s.setParam("slide_mode", order[(order.indexOf(s.u("slide_mode")).coerceAtLeast(0) + 1) % order.size])
    }
    if (s.u("slide_mode") == 1) Choice(listOf(0 to "Saddle", 1 to "Crossed tines"), s.u("pinch_variant")) { s.setParam("pinch_variant", it) }
    Note(if (anod) "Anod's defaults: the bend stirs the Chladni cells (its distance the rate, its sign the sense), pressure feeds the torsion sweep, " +
            "the slide sets the spark's frequency, poly pressure the torsion's and spark's wavenumbers, the mod wheel throws the Chirikov map."
         else "Sumi's defaults: the bend drags the drop (glide), pressure feeds it, the slide sets its hue, poly pressure stirs the swirl, the mod wheel the vortex.")
    Title("VORTEX")
    Choice(listOf(0 to "Exponential", 1 to "Rankine", 3 to "Torsion"), s.u("vortex_profile")) { s.setParam("vortex_profile", it) }
    Toggle("Torsion sweep on note-on", s.u("torsion_sweep") == 1) { s.setParam("torsion_sweep", if (s.u("torsion_sweep") == 1) 0 else 1) }
    Note("The CC-routed vortex and the two-finger twist use the profile. Torsion: rings of alternating angular shear, exact at any amplitude (wavelength and phase on CC 104 / 105).")

    Title("RIPPLE")
    ControlParam(s, "Amount", 7)
    ControlParam(s, "Wavelength", 8)
    val ang = s.f("ripple_angle") * 57.29578f
    Step("Angle", "%.0f°".format(ang), 15) { k -> s.setParam("ripple_angle", ((ang + k).coerceIn(0f, 180f)) / 57.29578f) }
    val a7 = s.routeFor(7); val a8 = s.routeFor(8)
    Note(if (a7 == null || a8 == null) "Route a CC to the ripple dimensions in the CC map to use these."
         else "Sent as CC $a7 / CC $a8 through the MIDI path (the same route a controller would use).")

    Title("STYLUS WAKE")
    Choice(listOf(0 to "Inviscid doublet", 1 to "Viscous stroke"), s.u("wake_profile")) { s.setParam("wake_profile", it) }
    if (s.u("wake_profile") == 1) FParam(s, "Spread (l/a)", "wake_spread", 1.5f, 12f, 0.1f, "%.1f")
    Note(if (s.u("wake_profile") == 1) "The pen's stroke is an impulse in a viscous layer (the 2-D Stokeslet): small spread is sharp and close, large is soft and far-reaching."
         else "The pen's stroke is the exact potential flow around a rigid tip.")

    CcMapSection(s)

    Title("MIDI")
    Action("Pair Bluetooth MIDI instrument…") { host.pairBluetooth() }
    Note("Wired and virtual MIDI inputs connect automatically.")
    Title("OUTBOUND MIDI (PLAY MODE)")
    Toggle("USB-MIDI to the host computer (primary)", host.outUsb) { host.setTransports(!host.outUsb, host.outVirtual, host.outBle) }
    Note(host.usbStatus)
    Toggle("Virtual device (on-device DAWs)", host.outVirtual) { host.setTransports(host.outUsb, !host.outVirtual, host.outBle) }
    Note("\"midi-sink Play Surface\" in any Android DAW's MIDI input list — " + (if (host.virtualClients > 0) "${host.virtualClients} client(s) connected." else "no client connected."))
    Toggle("Bluetooth (BLE-MIDI) advertise", host.outBle) { host.setTransports(host.outUsb, host.outVirtual, !host.outBle) }
    Note(host.bleStatus)
    Action("Re-sync DAW (MCM + bend range)") { host.resync() }
    Action("Stop all notes (panic)", Color(0xFFFF8A80)) { host.panic() }
    Action("Run 60 s storm test (10 voices)") { host.storm() }
    Action("Run on-device hostmpe + normalizer suites") { host.selfTest() }
    if (host.selfTestResult.isNotEmpty()) Note(host.selfTestResult)

    Title("SESSION")
    BasicText(statusLine.ifEmpty { "—" }, style = TextStyle(color = dim, fontSize = 12.sp))
    Title("ABOUT")
    val core = remember { NativeBridge.nativeCoreVersion() }
    BasicText("midi-sink ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE}) · ${BuildConfig.BUILD_DESCRIBE}", style = TextStyle(color = dim, fontSize = 13.sp))
    BasicText("libsumi ${core shr 16}.${(core shr 8) and 0xFF}.${core and 0xFF} · AGPL-3.0 · midi-sink.vibetuned.com", style = TextStyle(color = Color(0x99FFFFFF), fontSize = 12.sp))
}

private fun bendName(m: Int) = when (m) { 255 -> "Medium default"; 0 -> "Glide (drag the drop)"; 1 -> "Ripple amplitude"; 2 -> "Torsion wavelength"; 3 -> "Spark frequency"; 4 -> "Chladni stir"; else -> "?" }
private fun pressName(m: Int) = when (m) { 255 -> "Medium default"; 0 -> "Ink feed"; 1 -> "Lamb–Oseen swirl"; 2 -> "Torsion sweep feed"; else -> "?" }
private fun slideName(m: Int) = when (m) { 255 -> "Medium default"; 0 -> "Hue"; 1 -> "Pinch"; 2 -> "Spark frequency"; else -> "?" }

private fun paletteName(s: SessionStore): String {
    val id = s.u("active_palette_id")
    if (id >= 3) return "Custom"
    val t = NativeBridge.nativePalettePreset(if (s.anod) 1 else 0, id)
    return if (t.isEmpty()) "?" else JSONObject(t).optString("name", "?")
}

@Composable
private fun CcMapSection(s: SessionStore) {
    Title("CC MAP")
    val routes = s.routes()
    routes.forEach { r ->
        Row(Modifier.fillMaxWidth().padding(vertical = 5.dp)) {
            BasicText("CC %3d  %s".format(r.cc, if (r.channel == 0xFF) "any" else "ch ${r.channel + 1}"),
                style = TextStyle(color = dim, fontSize = 14.sp), modifier = Modifier.weight(1f))
            BasicText(CcMap.ctlName(r.target), style = TextStyle(color = dim, fontSize = 14.sp), modifier = Modifier.weight(1.3f))
            BasicText("✕", style = TextStyle(color = Color(0xFFFF8A80), fontSize = 14.sp),
                modifier = Modifier.clickable { s.setRoutes(routes.filter { it != r }) }.padding(horizontal = 8.dp))
        }
    }
    val newCC = remember { mutableStateOf(74) }
    val newCh = remember { mutableStateOf(0xFF) }
    val newT = remember { mutableStateOf(0) }
    Step("New route: CC", "${newCC.value}", 10) { k -> newCC.value = (newCC.value + k).coerceIn(0, 127) }
    Cycle("Channel", if (newCh.value == 0xFF) "any" else "${newCh.value + 1}") {
        newCh.value = if (newCh.value == 0xFF) 0 else if (newCh.value >= 15) 0xFF else newCh.value + 1
    }
    Cycle("Dimension", CcMap.ctlName(newT.value)) { newT.value = (newT.value + 1) % CcMap.ctlCount }
    Action("Add route") {
        val kept = routes.filter { !(it.cc == newCC.value && it.channel == newCh.value) }
        s.setRoutes(kept + CcMap.Route(newCh.value, newCC.value, newT.value))
    }
    Action("Restore default map") { s.setRoutes(CcMap.defaults) }
    Note("Defaults: mod wheel → vortex strength; breath, volume and expression → ink flow; the Airwave's hands (Raise / Glide / Slide stir, " +
        "Grasp pinches, Tilt ripples); CC 102 / 103 → the ripple; CC 104–109 → the Phase-6 operators' handles.")
}

// -- the palette (QOL §1) ---------------------------------------------------------------

@Composable
private fun PalettePage(s: SessionStore) {
    val medium = if (s.anod) 1 else 0
    val count = NativeBridge.nativePalettePresetCount(medium)
    val names = (0 until count).map { i -> NativeBridge.nativePalettePreset(medium, i).let { if (it.isEmpty()) "?" else JSONObject(it).optString("name") } }
    Title("PALETTE")
    Choice((0 until minOf(3, count)).map { it to names[it] } + (3 to "Custom"), s.u("active_palette_id")) { s.setParam("active_palette_id", it) }
    Note("The medium's three built-in palettes and the custom slot. A palette-morph CC travels the ring from the active one.")
    Title("LIBRARY")
    val pick = remember { mutableStateOf(0) }
    Cycle("Preset", names.getOrElse(pick.value) { "?" }) { pick.value = (pick.value + 1) % maxOf(1, count) }
    Action("Load into custom") {
        val t = NativeBridge.nativePalettePreset(medium, pick.value)
        if (t.isNotEmpty()) s.patch(JSONObject().put("palette", JSONObject(t).getJSONObject("palette")).put("params", JSONObject().put("active_palette_id", 3)))
    }
    Note("The curated library: the built-ins and the colour-blind-considerate presets (Cobalt & amber — the Okabe–Ito pair — Viridis and Cividis). Loading one fills the custom slot as a starting point.")
    if (s.u("active_palette_id") < 3) return
    val pal = s.palette
    val stops = pal.optJSONArray("stops") ?: JSONArray()
    val sc = stops.length().coerceIn(2, 8)
    fun stop(i: Int) = stops.optJSONArray(i) ?: JSONArray().put(0).put(0).put(0).put(0)
    fun withStops(a: JSONArray) = JSONObject(pal.toString()).put("stops", a)
    val anod = s.anod
    Title(if (anod) "THE GLOW: DIM CHARGE TO BURNING CHARGE" else "THE INK: THIN TO POOLED")
    for (i in 0 until sc) {
        val st = stop(i)
        val label = if (i == 0) (if (anod) "Dim" else "Thin") else if (i == sc - 1) (if (anod) "Burning" else "Pooled") else "Stop ${i + 1}"
        RgbRows(label, floatArrayOf(st.optDouble(0).toFloat(), st.optDouble(1).toFloat(), st.optDouble(2).toFloat())) { rgb ->
            val a = JSONArray(stops.toString()); a.put(i, JSONArray().put(rgb[0].toDouble()).put(rgb[1].toDouble()).put(rgb[2].toDouble()).put(st.optDouble(3)))
            s.setPalette(withStops(a))
        }
        if (i > 0 && i < sc - 1) {
            val pos = st.optDouble(3).toFloat()
            Step("  at", "%.2f".format(pos), 10) { k ->
                val a = JSONArray(stops.toString()); val q = JSONArray(st.toString()); q.put(3, ((pos + k * 0.01f).coerceIn(0f, 1f)).toDouble()); a.put(i, q)
                s.setPalette(withStops(a))
            }
        }
    }
    Row {
        Action("Add stop", enabled = sc < 8) {
            // insert before the last stop, midway between its neighbours (the desktop's rule)
            val last = stop(sc - 1); val prev = stop(sc - 2); val a = JSONArray()
            for (i in 0 until sc - 1) a.put(stop(i))
            a.put(JSONArray().put((prev.optDouble(0) + last.optDouble(0)) / 2).put((prev.optDouble(1) + last.optDouble(1)) / 2)
                .put((prev.optDouble(2) + last.optDouble(2)) / 2).put((prev.optDouble(3) + 1.0) / 2))
            a.put(last); s.setPalette(withStops(a))
        }
    }
    Action("Remove stop", enabled = sc > 2) {
        val a = JSONArray(); for (i in 0 until sc - 2) a.put(stop(i)); a.put(stop(sc - 1)); s.setPalette(withStops(a))
    }
    Title("DEPTH & DRIFT")
    fun palF(key: String) = pal.optDouble(key, 0.0).toFloat()
    for ((label, key, range) in listOf(Triple("Depth curve", "depth_gamma", 0.25f to 4f), Triple("Depth floor", "depth_floor", 0f to 1f), Triple("Hue drift", "hue_drift", 0f to 1f))) {
        val v = palF(key)
        Step(label, "%.2f".format(v), 10) { k -> s.setPalette(JSONObject(pal.toString()).put(key, ((v + k * 0.01f).coerceIn(range.first, range.second)).toDouble())) }
    }
    Note(if (anod) "Depth: how the glow walks the ramp (below 1 the charge burns early). Drift: every drop takes a slightly different hue toward the colour below; in Anod the charge phase bands the filament between the ramp and it."
         else "Depth: how the ink's thickness walks the ramp (below 1 thin ink is already deep). Drift: every drop takes a slightly different hue toward the colour below.")
    RgbRows(if (anod) "Drift toward (the halo)" else "Drift toward", arr3(pal.optJSONArray("accent_rgb"))) { rgb ->
        s.setPalette(JSONObject(pal.toString()).put("accent_rgb", jarr(*rgb)))
    }
    if (!anod) RgbRows("Clear water", arr3(pal.optJSONArray("clear_rgb"))) { rgb -> s.setPalette(JSONObject(pal.toString()).put("clear_rgb", jarr(*rgb))) }
}

// -- the substrate (QOL §2) -------------------------------------------------------------

private val tints = listOf("Cream (washi)" to floatArrayOf(0.900f, 0.868f, 0.790f), "White" to floatArrayOf(0.955f, 0.950f, 0.935f), "Toned" to floatArrayOf(0.760f, 0.690f, 0.560f))
private val papers = listOf(Triple("Smooth", 0.25f, 1.4f), Triple("Washi", 0.5f, 1.0f), Triple("Coarse", 0.8f, 0.7f))

@Composable
private fun SubstratePage(s: SessionStore) {
    if (s.anod) {
        Title("THE GLASS")
        FParam(s, "Glass darkness", "anod_dark", 0f, 1f, 0.01f, "%.2f")
        Note("The vacuum glass under the discharge: 1 black, 0.5 the step-42 near-black, 0 twice as bright.")
        FParam(s, "Phosphor grain", "anod_grain", 0f, 1f, 0.01f, "%.2f")
        Note("The speckle in the glass, screen-locked.")
        Title("THE GLOW")
        FParam(s, "Glow bloom", "anod_bloom", 0f, 3f, 0.05f, "%.2f")
        UParam(s, "Glow reach (octaves)", "anod_bloom_levels", 1, 5)
        Note("The discharge blooms like a photograph in a lens; 0 is the plain composite. Prints and exports bloom the same.")
        FParam(s, "Glow scale", "anod_glow", 0.2f, 5f, 0.05f, "%.2f")
        Note("The strain a texel needs to glow: smaller = hotter, sooner.")
        val pitch = s.f("anod_pitch"); val lines = if (pitch > 0f) (1f / pitch).roundToInt() else 0
        Step("Grid lines", if (lines < 8) "Off" else "$lines", 16) { k ->
            val nx = (if (lines < 8) 0 else lines) + k * 8
            s.setParam("anod_pitch", if (nx < 8) 0f else 1f / nx.coerceAtMost(256))
        }
        Note("How many grid lines the displaced water would show across the canvas height; Off leaves the water glass.")
        FParam(s, "Strike charge", "anod_drop", 0.1f, 1f, 0.01f, "%.2f×")
        Note("The drop a strike seeds, as a fraction of the Sumi drop; the spark shear keeps the full size, so a small charge is torn into long streamers.")
    } else {
        Title("PAPER TINT")
        val cur = arr3(s.params.optJSONArray("paper_tint"))
        val ti = tints.indexOfFirst { t -> (0..2).all { kotlin.math.abs(t.second[it] - cur[it]) < 1e-4f } }
        Choice(tints.mapIndexed { i, t -> i to t.first } + (3 to "Custom"), if (ti < 0) 3 else ti) { i -> if (i < 3) s.patch(JSONObject().put("params", JSONObject().put("paper_tint", jarr(*tints[i].second)))) }
        RgbRows("Custom tint", cur) { rgb -> s.patch(JSONObject().put("params", JSONObject().put("paper_tint", jarr(*rgb)))) }
        Note("The washi's base tone; the mottle, grain and fibers ride on it.")
        Title("PAPER")
        val r = s.f("paper_roughness"); val fs = s.f("fiber_scale")
        val pi = papers.indexOfFirst { kotlin.math.abs(it.second - r) < 1e-4f && kotlin.math.abs(it.third - fs) < 1e-4f }
        Choice(papers.mapIndexed { i, p -> i to p.first } + (3 to "Custom"), if (pi < 0) 3 else pi) { i -> if (i < 3) s.setParams("paper_roughness" to papers[i].second, "fiber_scale" to papers[i].third) }
        FParam(s, "Roughness", "paper_roughness", 0f, 1f, 0.01f, "%.2f")
        FParam(s, "Fiber scale", "fiber_scale", 0.5f, 2f, 0.01f, "%.2f×")
        Note("Roughness is the strength of the mottle, grain and fiber strands (a CC can ride it live); fiber scale their fineness — below 1 longer, coarser strands.")
    }
}

// -- presets (QOL §3) ---------------------------------------------------------------------

@Composable
private fun PresetsPage(s: SessionStore, host: SheetHost) {
    val name = remember { mutableStateOf("") }
    val status = remember { mutableStateOf("") }
    val confirm = remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) { s.refreshPresetNames() }
    Title("PRESETS")
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        BasicTextField(name.value, { name.value = it }, singleLine = true, textStyle = TextStyle(color = white, fontSize = 15.sp),
            cursorBrush = SolidColor(white),
            modifier = Modifier.weight(1f).border(1.dp, Color(0x55FFFFFF)).padding(8.dp),
            decorationBox = { inner -> if (name.value.isEmpty()) BasicText("Name", style = TextStyle(color = faint, fontSize = 15.sp)); inner() })
        Spacer(Modifier.width(10.dp))
        BasicText("Save", style = TextStyle(color = if (name.value.isBlank()) faint else white, fontSize = 15.sp),
            modifier = Modifier.clickable(enabled = name.value.isNotBlank()) {
                val n = name.value.trim(); status.value = if (s.savePreset(n)) "Saved '$n'" else "Could not write '$n'"; name.value = ""
            }.padding(8.dp))
    }
    Note("A preset is the whole session but the ink: every parameter, the input dialect, the custom palette, the CC map, " +
        "the control values and the strip's wheel assignments. The last session is saved on every change and restored at launch.")
    Title("SAVED")
    if (s.presetNames.value.isEmpty()) Note("No saved presets yet.")
    for (n in s.presetNames.value) {
        Row(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
            BasicText(n, style = TextStyle(color = white, fontSize = 15.sp), modifier = Modifier.weight(1f).clickable { confirm.value = n }.padding(vertical = 6.dp))
            BasicText("share", style = TextStyle(color = dim, fontSize = 13.sp), modifier = Modifier.clickable { host.sharePreset(n) }.padding(8.dp))
            BasicText("✕", style = TextStyle(color = Color(0xFFFF8A80), fontSize = 14.sp), modifier = Modifier.clickable { s.deletePreset(n) }.padding(8.dp))
        }
        if (confirm.value == n) {
            Row {
                Action("Load '$n' — the settings are replaced, the ink stays", white) {
                    status.value = if (s.loadPreset(n)) "Loaded '$n'" else "Could not read '$n'"; confirm.value = null
                }
            }
        }
    }
    Title("FILES")
    Action("Import a preset…") { host.importPreset() }
    Action("Export this session…") { host.exportPreset(name.value.ifBlank { "midi-sink session" }) }
    Note("The same JSON loads on the desktop, the iPad and the web (presets/SCHEMA.md). Import and export go through the system file picker; an imported preset is applied and added to the list.")
    if (status.value.isNotEmpty()) Note(status.value)
}

// -- the print ledger (QOL §4) --------------------------------------------------------------

data class LedgerRow(val id: Int, val fw: Int, val fh: Int, val anod: Boolean, val printSeen: Boolean, val tw: Int, val th: Int, val whenSec: Int, val pw: Int, val ph: Int)

fun ledgerRows(): List<LedgerRow> {
    val a = NativeBridge.nativeLedgerList(); val n = a.firstOrNull() ?: 0
    return (0 until n).map { i -> val o = 1 + i * 10; LedgerRow(a[o], a[o + 1], a[o + 2], a[o + 3] == 1, a[o + 4] == 1, a[o + 5], a[o + 6], a[o + 7], a[o + 8], a[o + 9]) }
}

/** The iPad's PrintLedger.size: Screen, then 2K / 4K / 8K wide at the field's aspect, capped at 8192 a side. */
fun exportSize(choice: Int, fw: Int, fh: Int): Pair<Int, Int> {
    if (choice <= 0 || fw <= 0) return fw to fh
    var w = if (choice == 1) 2048 else if (choice == 2) 4096 else 8192
    var h = (w.toDouble() * fh / fw).roundToInt()
    if (h > 8192) { h = 8192; w = (h.toDouble() * fw / fh).roundToInt() }
    return w to maxOf(h, 1)
}

private val sizes = listOf("Screen", "2K wide", "4K wide", "8K wide")

@Composable
private fun PrintsPage(host: SheetHost) {
    val rows = remember { mutableStateOf(ledgerRows()) }
    val status = remember { mutableStateOf("") }
    val size = remember { mutableStateOf(2) }
    val alpha = remember { mutableStateOf(false) }
    LaunchedEffect(Unit) { while (true) { rows.value = ledgerRows(); status.value = NativeBridge.nativeLedgerStatus(); delay(500) } }
    Title("PRINTS")
    Choice(sizes.mapIndexed { i, t -> i to t }, size.value) { size.value = it }
    Toggle("Anod over alpha", alpha.value) { alpha.value = !alpha.value }
    Note("Every dip lands here with the sheet it printed. The field is resolution-independent, so a dip re-renders at any size from the same field — " +
        "up to 8192 a side; detail below a field texel is interpolation. Anod over alpha writes the glow and the grid over transparent glass. " +
        "Exports go to Pictures/midi-sink (the gallery) and the share sheet. Six sheets or 256 MB are kept; the oldest go first.")
    Action("Save the newest print to the gallery", enabled = rows.value.any { it.printSeen }) { host.saveNewestPrint() }
    Title("THIS SESSION")
    if (rows.value.isEmpty()) Note("No dips yet this session.")
    for (e in rows.value.reversed()) {
        Row(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
            val bmp = remember(e.id, e.printSeen) {
                if (!e.printSeen || e.tw <= 0) null else NativeBridge.nativeLedgerPixels(e.id, 0)?.let { px ->
                    val b = android.graphics.Bitmap.createBitmap(e.tw, e.th, android.graphics.Bitmap.Config.ARGB_8888)
                    b.copyPixelsFromBuffer(java.nio.ByteBuffer.wrap(px)); b.asImageBitmap()
                }
            }
            if (bmp != null) Image(bmp, contentDescription = null, modifier = Modifier.width(160.dp).height((160f * e.th / maxOf(1, e.tw)).dp))
            else Box(Modifier.size(160.dp, 100.dp).background(Color(0x33FFFFFF)))
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                val t = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US).format(java.util.Date(e.whenSec * 1000L))
                BasicText("$t · ${if (e.anod) "Anod" else "Sumi"}", style = TextStyle(color = white, fontSize = 14.sp))
                val (w, h) = exportSize(size.value, e.fw, e.fh)
                BasicText("field ${e.fw}×${e.fh} → ${w}×${h}" + (if (alpha.value && e.anod) " over alpha" else ""), style = TextStyle(color = faint, fontSize = 12.sp))
                Action("Export PNG", white) { host.exportPrint(e.id, size.value, alpha.value) }
            }
        }
    }
    if (status.value.isNotEmpty()) Note(status.value)
}

// -- the Phase-6 operators (MEDIUM §2) -------------------------------------------------------

@Composable
private fun OperatorsPage(s: SessionStore) {
    Title("CHLADNI")
    ControlParam(s, "Stir (A)", 16)
    ControlParam(s, "Balance (B)", 17)
    Note("Every key's disc turns as an eddy (still at the core and the rim) while the water between the keys rests. In Anod the per-note bend plays the stir. " +
        "Balance: 0 neighbours counter-rotate, ½ every other cell rests, 1 all turn the same way.")
    Choice(listOf(0 to "Discs (exact)", 1 to "Inverse Chladni"), s.u("chladni_mode")) { s.setParam("chladni_mode", it) }
    FParam(s, "Cell size", "chladni_cell", 0.5f, 1.5f, 0.01f, "%.2f")
    Note("Discs: exact rotation inside each key, capped at the key. Inverse Chladni: the eddies are summed into one flow, may grow past the keys (to 1.5) and stir the water between them.")
    Title("BURST")
    FParam(s, "Age", "burst_age", 1.5f, 12f, 0.1f, "%.1f× core")
    FParam(s, "Life", "burst_life", 0f, 4f, 0.05f, "%.2f s")
    UParam(s, "Order (m)", "burst_order", 2, 8)
    Note("The viscous multipole burst (a gesture: the pinch in Anod; the strike no longer fires it).")
    Title("SPARK")
    FParam(s, "Shear", "spark_shear", 0f, 2f, 0.05f, "%.2f× r")
    FParam(s, "Decay", "spark_tau", 0.05f, 2f, 0.05f, "%.2f s")
    UParam(s, "Octaves", "spark_stack", 1, 4)
    Choice(listOf(0 to "Triangle", 1 to "Noise"), s.u("spark_profile")) { s.setParam("spark_profile", it) }
    ControlParam(s, "Frequency (k)", 18)
    Note("The Anod strike's jagged shear episode. The slide (CC 74) drives the frequency under the Anod default.")
    Title("CHIRIKOV")
    FParam(s, "K max", "chirikov_kmax", 0f, 2f, 0.01f, "%.2f")
    UParam(s, "Periods", "chirikov_periods", 1, 8)
    FParam(s, "Drift", "chirikov_eps", 0.05f, 1f, 0.01f, "%.2f")
    ControlParam(s, "Throw", 19)
    Note("The standard map: a throw of the mod wheel (the Anod default) or this row is a kick; Greene's threshold 0.97 separates smooth sheets from filaments and island chains.")
    if (listOf(16, 17, 18, 19).any { s.routeFor(it) == null }) Note("A row without a CC route is disabled — route one in the CC map (104–109 by default).")
}

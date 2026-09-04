package com.vibetuned.midisink

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.os.SystemClock
import android.util.Log
import android.view.Choreographer
import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

/**
 * Play-mode overlay (PHASE4_SPEC §6, §7): the faint two-tone cell lattice,
 * per-touch joystick indicators, the S-Pen path (legato, wake, CC74/pinch,
 * pressure, hover ghost) and the saturation HUD blink — rendered natively
 * over the GLES surface, never inside the core. The iOS PlayOverlayView's
 * sibling, event for event.
 *
 * Geometry has ONE source of truth: the lattice is a SWEEP of
 * sumi_layout_probe (DECISIONS_3 #9) and every hit-test is a probe call —
 * no lattice math lives in Kotlin. Touches arrive on the UI thread; all
 * MIDI goes through NativeBridge onto the MIDI thread (the single producer).
 *
 * Why a View and not Compose pointerInput (DECISIONS_3 #42): Compose's
 * pointer API exposes pressure and tool type but not AXIS_TILT /
 * AXIS_ORIENTATION, which the S-Pen booster and posture gate need; the iOS
 * reference is a UIView for the same reason. Compose hosts it via AndroidView.
 */
private const val TAG = "sumi-shell"

class PlayOverlayView(context: Context) : View(context) {

    /** §4 Android finger row: synthesized velocity, touch-size modulation behind a setting. */
    var velocityFromTouchSize = false
    /** params.slide_mode mirror: 1 = the pen's Δy drives the pinch, CC74 outbound only (#38). */
    var slideMode = 0
    /** Fired when the pen's barrel button moved the sustain state, so the host
     *  can pull the strip's display mirror back into agreement. */
    var onSustainChanged: (() -> Unit)? = null

    private val density = resources.displayMetrics.density

    private class Cell(val note: Int, val cx: Float, val cy: Float, val r: Float)
    private class ActiveTouch(val ox: Float, val oy: Float, val rMaxCH: Float,
                              val note: Int, val voice: Int) {
        var effX = 0f
        var effY = 0f
    }
    // §7: the pen abandons the joystick — absolute-position play.
    private class ActivePen(val voice: Int, val anchorX: Float, val anchorY: Float,
                            val rMaxCH: Float, var lastNote: Int,
                            var lastX: Float, var lastY: Float,
                            var lastOrientation: Float, var lastTilt: Float) {
        var lastEff = 0f          // CC74/pinch delta state
        var boost = 0f            // #40 azimuth tail-stir booster: bend ×(1 + boost), 0..2
        var lastOffset = 0f       // for decay re-emission of the bend
        var lastVel = 96
    }

    private val touches = HashMap<Int, ActiveTouch>()   // by pointer id
    private val pens = HashMap<Int, ActivePen>()
    private var cells = ArrayList<Cell>()
    private var latticeW = 0
    private var latticeH = 0
    private var latticeDirty = true
    private val pathNaturals = Path()
    private val pathAccidentals = Path()

    private val probeOut = FloatArray(7)
    private val effOut = FloatArray(2)

    private var hover = false
    private var hoverX = 0f
    private var hoverY = 0f
    private var blinkUntil = 0L
    /** The S-Pen's barrel button, held state. It drives the SAME strip sustain
     *  engine as the palette's pad (§8): one implementation, so the pad's
     *  display, the momentary/toggle setting, the master-channel discipline
     *  and the never-dropped class all come for free, and a panic clears the
     *  pen's pedal too. */
    private var penButtonDown = false

    // #41 two-tone lattice: paper-cream halo UNDER the dark ring keeps cells
    // legible over dense ink; accidental rings draw LAST (on top), darker —
    // black keys sitting on the keybed.
    private val paintHalo = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.55f * 255).toInt(), 245, 240, 227)
        strokeWidth = 3f * density
    }
    private val paintNat = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.18f * 255).toInt(), 0, 0, 0)
        strokeWidth = 1f * density
    }
    private val paintAcc = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.30f * 255).toInt(), 0, 0, 0)
        strokeWidth = 1.2f * density
    }
    private val paintRing = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.35f * 255).toInt(), 0, 0, 0)
        strokeWidth = 1f * density
    }
    private val paintThumb = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.55f * 255).toInt(), 0, 0, 0)
    }
    private val paintHeld = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.08f * 255).toInt(), 0, 0, 0)
    }
    private val paintHover = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.25f * 255).toInt(), 0, 0, 0)
        strokeWidth = 1f * density
    }
    private val paintHoverDot = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb((0.30f * 255).toInt(), 0, 0, 0)
    }
    private val paintBlink = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = Color.argb((0.6f * 255).toInt(), 220, 40, 40)
        strokeWidth = 6f * density
    }

    init {
        setWillNotDraw(false)
    }

    private fun nowS(): Double = System.nanoTime() / 1e9

    // -- lattice (probe sweep — no geometry duplication) ----------------------

    /** The layout (or any params write) changed: rebuild the lattice, end held voices. */
    fun layoutChanged() {
        releaseAll()
        latticeDirty = true
        rebuildLatticeIfNeeded()
        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        latticeDirty = true
        // Sweep here rather than inside onDraw: it is 26,400 probe calls, and
        // a draw pass is the wrong place to spend them.
        rebuildLatticeIfNeeded()
    }

    private fun rebuildLatticeIfNeeded() {
        if (width <= 0 || height <= 0) return
        if (!latticeDirty && latticeW == width && latticeH == height) return
        latticeDirty = false
        latticeW = width
        latticeH = height
        val aspect = width.toFloat() / height.toFloat()
        // Sweep finely enough that no cell is skipped (Jankó columns are the
        // narrowest feature); the native side dedupes by (note, center).
        val flat = NativeBridge.nativeLatticeSweep(aspect, 220, 120)
        cells = ArrayList(flat.size / 4)
        pathNaturals.reset()
        pathAccidentals.reset()
        var i = 0
        while (i + 3 < flat.size) {
            val c = Cell(flat[i].toInt(), flat[i + 1], flat[i + 2], flat[i + 3])
            cells.add(c)
            val pc = c.note % 12
            val black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10
            // PHASE4 §6: a cell is drawn ROUND, at exactly the radius of the
            // joystick it generates (R_max). The lattice is a picture of the
            // knobs, not of the hit regions — which is why an accidental's
            // circle is smaller than its playable width (#59) and why a
            // natural's has always been narrower than its key.
            (if (black) pathAccidentals else pathNaturals)
                .addCircle(c.cx * width, c.cy * height, c.r * height, Path.Direction.CW)
            i += 4
        }
    }

    // -- velocity (§4 truth table) ---------------------------------------------

    /** iOS finger: no force API — 96 fixed or coarse touch-size modulation. */
    private fun synthVelocity(e: MotionEvent, i: Int): Int {
        if (!velocityFromTouchSize) return 96
        val mrPt = e.getTouchMajor(i) / 2f / density   // radius in dp ≈ iOS points
        val v = 48f + (mrPt - 8f) / 18f * 70f
        return v.coerceIn(40f, 120f).toInt()
    }

    /**
     * S-Pen: getPressure() is real (Wacom EMR, 0..1). Calibrated like the
     * Pencil (#38 corrected): a baseline tap = 96 (the finger default), a
     * hard press = 127; touch-down pressure is sampled before contact force
     * builds, so sub-baseline readings clamp UP to 96 — the pen never
     * whispers by accident, pressing in only adds. DECISIONS_3 #43.
     */
    private fun penVelocity(pressure: Float): Int {
        val t = ((pressure - PEN_P_BASE) / (PEN_P_HARD - PEN_P_BASE)).coerceIn(0f, 1f)
        return (96f + t * 31f).toInt().coerceIn(96, 127)
    }

    private fun isStylus(e: MotionEvent, i: Int): Boolean {
        val t = e.getToolType(i)
        return t == MotionEvent.TOOL_TYPE_STYLUS || t == MotionEvent.TOOL_TYPE_ERASER
    }

    // -- the S-Pen's barrel button -> sustain (§8's pad, driven by the pen) ----

    private fun isStylusSource(e: MotionEvent): Boolean {
        for (i in 0 until e.pointerCount) if (isStylus(e, i)) return true
        return e.isFromSource(InputDevice.SOURCE_STYLUS)
    }

    private fun stylusButtonHeld(e: MotionEvent): Boolean {
        val mask = MotionEvent.BUTTON_STYLUS_PRIMARY or MotionEvent.BUTTON_STYLUS_SECONDARY
        return (e.buttonState and mask) != 0
    }

    /** Idempotent: only a CHANGE reaches the engine, so the explicit
     *  ACTION_BUTTON_* path and the buttonState fallback below cannot
     *  double-fire one press. */
    private fun setPenButton(down: Boolean) {
        if (down == penButtonDown) return
        penButtonDown = down
        if (down) NativeBridge.nativeStripSustainDown() else NativeBridge.nativeStripSustainUp()
        Log.i(TAG, "[pen] barrel button ${if (down) "pressed -> sustain on" else "released -> sustain off"}")
        onSustainChanged?.invoke()
    }

    /** Simulated press, for the `--es penButton` debug intent. */
    fun simulatePenButton(down: Boolean) = setPenButton(down)

    /** True when the event WAS a button transition (nothing else to do with it). */
    private fun handleStylusButton(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_BUTTON_PRESS -> if (isStylusSource(e)) {
                setPenButton(true)
                return true
            }
            MotionEvent.ACTION_BUTTON_RELEASE -> if (isStylusSource(e)) {
                setPenButton(false)
                return true
            }
        }
        // Fallback: an OEM stack that updates buttonState without dispatching
        // ACTION_BUTTON_* is still honoured — a transition on any stylus event
        // counts. (The explicit actions above have returned already, so the
        // two paths can never disagree.)
        if (isStylusSource(e)) setPenButton(stylusButtonHeld(e))
        return false
    }

    // -- touches ---------------------------------------------------------------

    /** Button events while the pen HOVERS arrive here rather than in
     *  onTouchEvent — clicking without touching the glass must work too. */
    override fun onGenericMotionEvent(e: MotionEvent): Boolean {
        if (handleStylusButton(e)) return true
        return super.onGenericMotionEvent(e)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (handleStylusButton(e)) {
            invalidate()
            return true
        }
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> begin(e, e.actionIndex)
            MotionEvent.ACTION_MOVE -> for (i in 0 until e.pointerCount) move(e, i)
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> end(e.getPointerId(e.actionIndex))
            MotionEvent.ACTION_CANCEL -> releaseAll()
        }
        invalidate()
        return true
    }

    private fun begin(e: MotionEvent, i: Int) {
        if (width <= 0 || height <= 0) return
        val id = e.getPointerId(i)
        val x = e.getX(i)
        val y = e.getY(i)
        val aspect = width.toFloat() / height.toFloat()
        // Probe the cell under the contact (instance-free, UI thread — #2).
        if (!NativeBridge.nativeLayoutProbe(x / width, y / height, aspect, probeOut)) return
        val note = probeOut[0].toInt()
        val rMax = probeOut[3]
        val tDown = nowS()
        if (isStylus(e, i)) {
            // §7: absolute-position play — the strike anchors the note.
            val vel = penVelocity(e.getPressure(i))
            val voice = NativeBridge.nativePenBegin(tDown, note, vel)
            if (voice < 0) {
                blink()
                return
            }
            pens[id] = ActivePen(voice, x, y, rMax, note, x, y,
                                 e.getOrientation(i), e.getAxisValue(MotionEvent.AXIS_TILT, i))
            armTicker()
            return
        }
        // Finger: center bend + Note On on the allocated channel (sync hop for
        // the voice id). The pitch gradient is the probe's semitone axis
        // (DECISIONS_3 #18: horizontal on both playable layouts; vertical is
        // the bipolar pressure/swirl axis).
        val step = probeOut[6]
        val voice = NativeBridge.nativeTouchBegin(tDown, note, synthVelocity(e, i), rMax,
                                                  probeOut[4] / step, probeOut[5] / step)
        if (voice < 0) {
            // Saturation (§5.1): silent drop + HUD blink — never steal.
            blink()
            return
        }
        touches[id] = ActiveTouch(x, y, rMax, note, voice)
    }

    private fun move(e: MotionEvent, i: Int) {
        val id = e.getPointerId(i)
        pens[id]?.let { movePen(e, i, it); return }
        val at = touches[id] ?: return
        val h = height.toFloat()
        // Δ in canvas-height units — the probe's metric (§2 units).
        val dx = (e.getX(i) - at.ox) / h
        val dy = (e.getY(i) - at.oy) / h
        NativeBridge.nativeJoystickEff(dx, dy, at.rMaxCH, effOut)
        at.effX = effOut[0]
        at.effY = effOut[1]
        // hostmpe takes the RAW screen delta; deadband/knee/bipolar Y are its
        // job (one implementation, zero drift).
        NativeBridge.nativeTouchUpdate(at.voice, dx, dy)
    }

    private fun end(id: Int) {
        pens.remove(id)?.let { NativeBridge.nativePenEnd(it.voice, 64) }
        touches.remove(id)?.let { NativeBridge.nativeTouchEnd(it.voice, 64) }
    }

    /** §7 pen move: wake on every stroke segment (physical, never MIDI), then
     *  legato, azimuth booster, CC74/pinch, pressure. */
    private fun movePen(e: MotionEvent, i: Int, pen: ActivePen) {
        val w = width.toFloat()
        val h = height.toFloat()
        val x = e.getX(i)
        val y = e.getY(i)
        // Tip travel this event — the gesture discriminator (#40): azimuth
        // swings naturally while drawing, so lean deltas count as a gesture
        // only while the tip is planted (< 3 dp/event).
        val tipMove = hypot(x - pen.lastX, y - pen.lastY)
        val pressure = e.getPressure(i).coerceIn(0f, 1f)
        // Dipolar wake rides the stroke: tip radius from real pressure (§4.3(4)).
        NativeBridge.nativeAddWake(pen.lastX / w, pen.lastY / h, x / w, y / h,
                                   0.006f + 0.030f * pressure)
        pen.lastX = x
        pen.lastY = y

        val aspect = w / h
        // Legato glissando (#39, all playable layouts): probe the cell UNDER
        // the pen; a crossing is a same-channel retrigger at the CURRENT
        // pressure's velocity; inside the cell the offset from its center
        // (along its own semitone axis) is the bend. Dead zones: no call —
        // the last pitch sustains.
        if (NativeBridge.nativeLayoutProbe(x / w, y / h, aspect, probeOut)) {
            val dxAC = x / h - probeOut[1] * aspect
            val dyAC = y / h - probeOut[2]
            val offset = (dxAC * probeOut[4] + dyAC * probeOut[5]) / probeOut[6]
            val vel = penVelocity(pressure)
            pen.lastNote = probeOut[0].toInt()
            pen.lastOffset = offset
            pen.lastVel = vel
            NativeBridge.nativePenGlide(pen.voice, pen.lastNote, offset,
                                        1f + min(pen.boost, 2f), vel)
        }

        // #40 posture gate: tilting cross-talks into the reported orientation
        // — a posture change is NOT a dial gesture. AXIS_TILT is radians from
        // the surface normal (0 = upright); the azimuth is also ignored near
        // vertical (tilt < 0.37 rad ≙ iOS altitude > 1.2), where the estimate
        // swings wildly.
        val tilt = e.getAxisValue(MotionEvent.AXIS_TILT, i)
        val posture = abs(tilt - pen.lastTilt) > 0.02f
        pen.lastTilt = tilt

        // #40 azimuth GESTURE (final): the in-cell tip vibrato is always live
        // at ×1 — the baseline is ONE, never zero. Stirring the tail with the
        // tip planted multiplies it momentarily, ×1..×3, decaying back (τ =
        // 0.4 s). Per-event floor 0.02 rad keeps lean jitter silent; spike
        // clamp 0.2. The S-Pen has no barrel-roll axis: this fallback is the
        // booster's only feed (#40 / handoff).
        val orientation = e.getOrientation(i)
        val dAz = atan2(sin(orientation - pen.lastOrientation), cos(orientation - pen.lastOrientation))
        pen.lastOrientation = orientation
        if (!posture && tilt > 0.37f && tipMove < 3f * density && abs(dAz) > 0.02f) {
            val step = min(abs(dAz), 0.2f)
            pen.boost = min(pen.boost + step * 2f, 2f)
        }

        // Y (relative to the strike) is the stylus timbre axis (§3.3): knee-
        // shaped, center 64, up = brighter. With slide_mode = 1 the smoothed
        // DELTAS drive the pinch at the pen position, fold axis from the pen's
        // azimuth (§7) — CC74 then goes OUTBOUND ONLY (#38: the loopback would
        // double-pinch through the mapper).
        val dyCH = (y - pen.anchorY) / h
        NativeBridge.nativeJoystickEff(0f, dyCH, pen.rMaxCH, effOut)
        val eff = -effOut[1]
        if (slideMode == 1) {
            val dk = (eff - pen.lastEff) * 0.6f
            if (abs(dk) > 0.002f) {
                // Android orientation: 0 = up, clockwise. atan2 convention (y
                // down): right = 0 -> azimuth = orientation - π/2.
                val az = orientation - (Math.PI / 2).toFloat()
                NativeBridge.nativeAddPinch(x / w, y / h, dk, az)
            }
            NativeBridge.nativePenSlide(pen.voice, eff, true)
        } else {
            NativeBridge.nativePenSlide(pen.voice, eff, false)
        }
        pen.lastEff = eff

        // True tip force -> channel pressure (the stylus's real Z).
        NativeBridge.nativePenPressure(pen.voice, pressure)
    }

    // -- #40 gesture decay: value ×= exp(−dt/0.4 s) each frame while pens live --

    private var tickerArmed = false
    private var lastTickNs = 0L
    private val ticker = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            if (lastTickNs != 0L) {
                val dt = (frameTimeNanos - lastTickNs) / 1e9
                if (dt > 0) {
                    val k = exp(-dt / GESTURE_TAU).toFloat()
                    for (pen in pens.values) {
                        if (pen.boost <= 0f) continue
                        pen.boost *= k
                        if (pen.boost < 0.01f) pen.boost = 0f
                        // Re-emit the bend at the decaying scale (change-only
                        // downstream); it settles exactly at ×1.
                        NativeBridge.nativePenGlide(pen.voice, pen.lastNote, pen.lastOffset,
                                                    1f + min(pen.boost, 2f), pen.lastVel)
                    }
                }
            }
            lastTickNs = frameTimeNanos
            if (pens.isNotEmpty()) {
                Choreographer.getInstance().postFrameCallback(this)
            } else {
                tickerArmed = false
            }
        }
    }

    private fun armTicker() {
        if (tickerArmed) return
        tickerArmed = true
        lastTickNs = 0L
        Choreographer.getInstance().postFrameCallback(ticker)
    }

    // -- hover ghost (S-Pen hover, §7) -----------------------------------------

    override fun onHoverEvent(e: MotionEvent): Boolean {
        if (handleStylusButton(e)) {
            invalidate()
            return true
        }
        when (e.actionMasked) {
            MotionEvent.ACTION_HOVER_ENTER, MotionEvent.ACTION_HOVER_MOVE -> {
                hover = true
                hoverX = e.x
                hoverY = e.y
            }
            MotionEvent.ACTION_HOVER_EXIT -> {
                hover = false
                // The pen has left the digitizer: a momentary pedal it was
                // holding cannot stay down (in TOGGLE mode the engine ignores
                // the release, so a latched sustain survives — as it should).
                if (penButtonDown && pens.isEmpty()) setPenButton(false)
            }
        }
        invalidate()
        return true
    }

    private fun blink() {
        blinkUntil = SystemClock.uptimeMillis() + 350
        postInvalidateDelayed(400)
    }

    /** Leaving Play mode (or a layout change): end every held voice cleanly —
     *  pressure 0 + Note Off through the same path, no stuck notes. */
    fun releaseAll() {
        for (at in touches.values) NativeBridge.nativeTouchEnd(at.voice, 64)
        for (pen in pens.values) NativeBridge.nativePenEnd(pen.voice, 64)
        touches.clear()
        pens.clear()
        // Leaving Play mode with the barrel button held must not strand the
        // pedal either (toggle mode: engine-side no-op, so a latch survives).
        if (penButtonDown) setPenButton(false)
        invalidate()
    }

    // -- drawing -----------------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        rebuildLatticeIfNeeded()
        // Lattice: halo under ring, naturals first, accidentals on top (#41).
        canvas.drawPath(pathNaturals, paintHalo)
        canvas.drawPath(pathNaturals, paintNat)
        canvas.drawPath(pathAccidentals, paintHalo)
        canvas.drawPath(pathAccidentals, paintAcc)

        // Saturation HUD blink (§5.1): a brief border flash, no note stolen.
        if (SystemClock.uptimeMillis() < blinkUntil) {
            val inset = 3f * density
            canvas.drawRect(inset, inset, width - inset, height - inset, paintBlink)
        }
        // §7 hover ghost cursor.
        if (hover) {
            canvas.drawCircle(hoverX, hoverY, 9f * density, paintHover)
            canvas.drawCircle(hoverX, hoverY, 2f * density, paintHoverDot)
        }
        if (touches.isEmpty()) return
        val h = height.toFloat()
        val w = width.toFloat()
        // Jankó echo highlight (§6): all rows of a touched note are the same
        // note — say so.
        val held = HashSet<Int>()
        for (at in touches.values) held.add(at.note)
        for (c in cells) {
            if (c.note in held) canvas.drawCircle(c.cx * w, c.cy * h, c.r * h, paintHeld)
        }
        for (at in touches.values) {
            val rPx = at.rMaxCH * h
            // Hairline circle at the touch origin, radius R_max: the joystick's
            // travel bound, exactly as the math computes it.
            canvas.drawCircle(at.ox, at.oy, rPx, paintRing)
            // Thumb dot at Δ_eff — sits AT the origin inside the deadband.
            canvas.drawCircle(at.ox + at.effX * rPx, at.oy + at.effY * rPx, 5f * density, paintThumb)
        }
    }

    companion object {
        private const val GESTURE_TAU = 0.4
        /** S-Pen velocity calibration (DECISIONS_3 #43): normalized pressure at a baseline tap / a hard press. */
        const val PEN_P_BASE = 0.15f
        const val PEN_P_HARD = 0.75f
    }
}

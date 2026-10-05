package com.vibetuned.midisink

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View

/**
 * The fingering panel (Phase 9 step 64 — the iPad's FingeringPanelView of step 63, DECISIONS_8 #14–#16,
 * one for one): the trumpet's three valves and the trombone's slide as LARGE controls, in one of two
 * forms — a tall panel at the side of the sheet at mid-height, under the hand that does not play the
 * partials, or a wide one along the bottom edge. The strip keeps the wheels, the pedal, Next and Panic
 * at the opposite corner. Vertical, the valves stack top to bottom (1, 2, 3: a hand reaching from the
 * side rests its three fingers on them) and the slide is a vertical track, the 1st position at the top,
 * the 7th at the bottom — down is out, lower. Horizontal, the pads lie side by side with 1 under the
 * index finger and the slide's 1st position is at the hand's near side. The panel's own rotate button
 * (the corner towards the sheet) flips the form and persists it — the settings' toggle is the same
 * switch. All VALUE state lives in hostmpe_strip_t on the MIDI thread (the valve and slide engines);
 * this view keeps display mirrors only — the slide's mirror quantised exactly as the engine sends it
 * (round(position · 127) / 127), so the probe the overlay runs under the mirror sees the bytes' value.
 * A valve stays down until its finger lifts, wherever the finger wanders — real valves. Every message
 * rides the master channel.
 *
 * The S Pen and the fingers (step 64's fix round, DECISIONS_8 #21): Android's input dispatcher lets
 * ONE input device be active at a time in a window and prefers the stylus — the pen coming into hover
 * range cancels the fingers' gestures in that window and drops their presses until it leaves. So the
 * host shows this view in a WINDOW OF ITS OWN over the frame on the brass layouts (a split-touch,
 * non-modal sub-window: MainActivity.syncPanelWindow), and the fingers' stream here never meets the
 * pen's on the partials. A cancel releases as a lift would; the pen hovering over the panel itself is
 * the one way to get one.
 */
class FingeringPanelView(context: Context) : View(context) {

    enum class Mode { NONE, VALVES, SLIDE }
    enum class Orientation { VERTICAL, HORIZONTAL }

    var mode = Mode.NONE
        private set
    var orientation = Orientation.VERTICAL
        private set
    private var mirrored = false   // left-handed: the index finger's pad and the 1st position at the hand's near side
    /** The display mirrors (the engine on the MIDI thread is the truth). */
    var valves = 0
        private set
    var slide = 0f
        private set
    private val grabs = HashMap<Int, Int>()   // pointer id -> the valve it holds (0..2), or -1 for the slide
    private var darkTheme = false

    /** The host: the engines through NativeBridge, the mirror to the overlay, the orientation's persistence. */
    var onValve: ((Int, Boolean) -> Unit)? = null
    var onSlide: ((Float) -> Unit)? = null
    var onRotate: (() -> Unit)? = null

    private val density = resources.displayMetrics.density
    private val gap = 12f * density
    private val headerH = 30f * density   // the band above the pads that holds the rotate button

    private val paintBg = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = Color.argb((0.42f * 255).toInt(), 255, 255, 255) }
    private val paintBorder = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = Color.argb((0.15f * 255).toInt(), 0, 0, 0); strokeWidth = 0.5f * density }
    private val paintPadOn = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = Color.argb((0.45f * 255).toInt(), 0, 0, 0) }
    private val paintPadOff = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = Color.argb((0.40f * 255).toInt(), 0, 0, 0); strokeWidth = 1.5f * density }
    private val paintTrack = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = Color.argb((0.12f * 255).toInt(), 0, 0, 0) }
    private val paintTick = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = Color.argb((0.30f * 255).toInt(), 0, 0, 0) }
    private val paintThumb = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = Color.argb((0.55f * 255).toInt(), 0, 0, 0) }
    private val paintText = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb((0.75f * 255).toInt(), 0, 0, 0); textSize = 11f * density; textAlign = Paint.Align.CENTER }
    private val paintNumber = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb((0.75f * 255).toInt(), 0, 0, 0); textSize = 26f * density; textAlign = Paint.Align.CENTER }
    private val paintNumberOn = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb((0.75f * 255).toInt(), 255, 255, 255); textSize = 26f * density; textAlign = Paint.Align.CENTER }
    private val paintGlyph = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = Color.argb((0.6f * 255).toInt(), 0, 0, 0); strokeWidth = 1.6f * density; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val paintGlyphFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = Color.argb((0.6f * 255).toInt(), 0, 0, 0) }

    init { setWillNotDraw(false) }

    /** The panel's natural size in dp (the host clamps it to the sheet): tall at the side, wide at the bottom. */
    val preferredDp: Pair<Float, Float>
        get() = when {
            mode == Mode.SLIDE && orientation == Orientation.VERTICAL -> 104f to 460f
            mode == Mode.SLIDE -> 460f to 104f
            orientation == Orientation.VERTICAL -> 140f to 420f
            else -> 400f to 150f
        }

    fun setMode(m: Mode) { if (m == mode) return; mode = m; endGestures(); invalidate() }
    fun setOrientation(o: Orientation) { if (o == orientation) return; orientation = o; endGestures(); invalidate() }
    fun setMirror(on: Boolean) { if (on == mirrored) return; mirrored = on; invalidate() }
    /** The engine's state (nativeStripState) to the mirrors. */
    fun syncFingering(v: Int, s: Float) { valves = v; slide = s; invalidate() }
    fun setDarkTheme(dark: Boolean) {
        if (dark == darkTheme) return
        darkTheme = dark
        val ink = if (dark) 255 else 0
        paintBg.color = if (dark) Color.argb((0.45f * 255).toInt(), 0, 0, 0) else Color.argb((0.42f * 255).toInt(), 255, 255, 255)
        paintBorder.color = if (dark) Color.argb((0.22f * 255).toInt(), 255, 255, 255) else Color.argb((0.15f * 255).toInt(), 0, 0, 0)
        paintPadOn.color = Color.argb((0.45f * 255).toInt(), ink, ink, ink)
        paintPadOff.color = Color.argb((0.40f * 255).toInt(), ink, ink, ink)
        paintTrack.color = Color.argb((0.12f * 255).toInt(), ink, ink, ink)
        paintTick.color = Color.argb((0.30f * 255).toInt(), ink, ink, ink)
        paintThumb.color = Color.argb((0.55f * 255).toInt(), ink, ink, ink)
        paintText.color = Color.argb((0.75f * 255).toInt(), ink, ink, ink)
        paintNumber.color = Color.argb((0.75f * 255).toInt(), ink, ink, ink)
        paintNumberOn.color = Color.argb((0.75f * 255).toInt(), 255 - ink, 255 - ink, 255 - ink)
        paintGlyph.color = Color.argb((0.6f * 255).toInt(), ink, ink, ink)
        paintGlyphFill.color = Color.argb((0.6f * 255).toInt(), ink, ink, ink)
        invalidate()
    }

    // -- geometry --------------------------------------------------------------

    // the rotate button: the top corner towards the sheet (the right; the left when mirrored)
    private fun rotateRect(): RectF {
        val s = 26f * density
        val x = if (mirrored) 6f * density else width - s - 6f * density
        return RectF(x, 4f * density, x + s, 4f * density + s)
    }
    // valve k's pad: stacked top to bottom under the header band, or side by side — 1 under the index
    // finger, so right to left when the hand comes from the right (mirrored)
    private fun padRect(k: Int): RectF {
        if (orientation == Orientation.VERTICAL) {
            val h = (height - headerH - 3 * gap) / 3f
            val y = headerH + k * (h + gap)
            return RectF(gap, y, width - gap, y + h)
        }
        val w = (width - 4 * gap) / 3f
        val i = if (mirrored) 2 - k else k
        val x = gap + i * (w + gap)
        return RectF(x, headerH, x + w, height - gap)
    }
    private fun trackRect(): RectF =
        if (orientation == Orientation.VERTICAL) RectF(width * 0.3f, 40f * density, width * 0.7f, height - 40f * density)
        else RectF(34f * density, height * 0.3f, width - 34f * density, height * 0.7f)
    // the slide's position 0..1 along the track: the 1st position at the top, or at the hand's near side
    // (the left; the right when mirrored), the 7th away from it
    private fun slidePosition(x: Float, y: Float): Float {
        val t = trackRect()
        if (orientation == Orientation.VERTICAL) {
            if (t.height() <= 0f) return 0f
            return ((y - t.top) / t.height()).coerceIn(0f, 1f)
        }
        if (t.width() <= 0f) return 0f
        val f = ((x - t.left) / t.width()).coerceIn(0f, 1f)
        return if (mirrored) 1f - f else f
    }
    private fun trackPoint(pos: Float): Pair<Float, Float> {   // where a position sits on the track
        val t = trackRect()
        if (orientation == Orientation.VERTICAL) return t.centerX() to (t.top + t.height() * pos)
        val f = if (mirrored) 1f - pos else pos
        return (t.left + t.width() * f) to t.centerY()
    }
    /** The engine's quantisation of a sent position (hostmpe_strip_slide_value: round(p · 127) / 127). */
    private fun quantised(pos: Float): Float = Math.round(pos.coerceIn(0f, 1f) * 127f) / 127f

    // -- touches ---------------------------------------------------------------

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = e.actionIndex
                val x = e.getX(i); val y = e.getY(i)
                val id = e.getPointerId(i)
                val rr = rotateRect(); rr.inset(-6f * density, -6f * density)
                if (mode != Mode.NONE && rr.contains(x, y)) { onRotate?.invoke(); invalidate(); return true }   // no grab
                when (mode) {
                    Mode.VALVES -> {
                        for (k in 0 until 3) {
                            val r = padRect(k); r.inset(-gap / 2, -gap / 2)
                            if (r.contains(x, y)) {
                                grabs[id] = k
                                valves = valves or (1 shl k)
                                onValve?.invoke(k, true)
                                break
                            }
                        }
                    }
                    Mode.SLIDE -> {
                        grabs[id] = -1
                        val pos = quantised(slidePosition(x, y))
                        slide = pos
                        onSlide?.invoke(pos)
                    }
                    Mode.NONE -> {}
                }
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until e.pointerCount) {
                    val g = grabs[e.getPointerId(i)] ?: continue
                    if (g >= 0) continue   // a valve stays where its finger first landed
                    val pos = quantised(slidePosition(e.getX(i), e.getY(i)))
                    if (pos != slide) { slide = pos; onSlide?.invoke(pos) }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> release(e.getPointerId(e.actionIndex))
            MotionEvent.ACTION_CANCEL -> for (id in grabs.keys.toList()) release(id)   // as lifts; the slide keeps its position
        }
        invalidate()
        return true
    }

    private fun release(id: Int) {
        val g = grabs.remove(id) ?: return
        if (g >= 0) releaseValve(g)
    }
    private fun releaseValve(k: Int) {
        valves = valves and (1 shl k).inv()
        onValve?.invoke(k, false)
    }

    /** Every grab released through the engine (a mode or form change). */
    private fun endGestures() { for (id in grabs.keys.toList()) release(id) }

    // -- drawing ---------------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val corner = 18f * density
        canvas.drawRoundRect(0f, 0f, width.toFloat(), height.toFloat(), corner, corner, paintBg)
        canvas.drawRoundRect(0f, 0f, width.toFloat(), height.toFloat(), corner, corner, paintBorder)
        when (mode) {
            Mode.VALVES -> {
                for (k in 0 until 3) {
                    val r = padRect(k)
                    val down = (valves and (1 shl k)) != 0
                    canvas.drawRoundRect(r, 14f * density, 14f * density, if (down) paintPadOn else paintPadOff)
                    val p = if (down) paintNumberOn else paintNumber
                    canvas.drawText("${k + 1}", r.centerX(), r.centerY() - (p.descent() + p.ascent()) / 2, p)
                }
            }
            Mode.SLIDE -> {
                val t = trackRect()
                val vertical = orientation == Orientation.VERTICAL
                if (vertical) canvas.drawRect(t.centerX() - 2f * density, t.top, t.centerX() + 2f * density, t.bottom, paintTrack)
                else canvas.drawRect(t.left, t.centerY() - 2f * density, t.right, t.centerY() + 2f * density, paintTrack)
                for (k in 0 until SLIDE_POSITIONS) {
                    val (px, py) = trackPoint(k.toFloat() / (SLIDE_POSITIONS - 1))
                    if (vertical) {
                        canvas.drawRect(t.left, py - 0.5f * density, t.right, py + 0.5f * density, paintTick)
                        canvas.drawText("${k + 1}", t.left / 2f, py + 4f * density, paintText)
                    } else {
                        canvas.drawRect(px - 0.5f * density, t.top, px + 0.5f * density, t.bottom, paintTick)
                        canvas.drawText("${k + 1}", px, t.bottom + 16f * density, paintText)
                    }
                }
                val (px, py) = trackPoint(slide)
                val thumb = if (vertical) RectF(t.left - 6f * density, py - 11f * density, t.right + 6f * density, py + 11f * density)
                            else RectF(px - 11f * density, t.top - 6f * density, px + 11f * density, t.bottom + 6f * density)
                canvas.drawRoundRect(thumb, 8f * density, 8f * density, paintThumb)
                canvas.drawText("Slide", width / 2f, (if (vertical) 22f else 18f) * density, paintText)
            }
            Mode.NONE -> return
        }
        // the rotate button, the same corner in every form: an arrow bending over a small frame
        val r = rotateRect()
        val cx = r.centerX(); val cy = r.centerY(); val s = r.width()
        canvas.drawRoundRect(RectF(cx - s * 0.22f, cy - s * 0.05f, cx + s * 0.22f, cy + s * 0.38f), 2f * density, 2f * density, paintGlyph)
        val arc = Path()
        arc.addArc(RectF(cx - s * 0.34f, cy - s * 0.42f, cx + s * 0.34f, cy + s * 0.26f), 200f, 110f)
        canvas.drawPath(arc, paintGlyph)
        val head = Path()
        head.moveTo(cx + s * 0.30f, cy - s * 0.42f); head.lineTo(cx + s * 0.44f, cy - s * 0.16f); head.lineTo(cx + s * 0.16f, cy - s * 0.18f); head.close()
        canvas.drawPath(head, paintGlyphFill)
    }

    companion object { const val SLIDE_POSITIONS = 7 }   // HOSTMPE_SLIDE_POSITIONS
}

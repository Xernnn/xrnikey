package com.xern.xrnkey.keyboard

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Build
import android.os.SystemClock
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import com.xern.xrnkey.R

/**
 * The keyboard: the strip (or the tool row) and the keys, drawn on one Canvas, with no view per
 * key, so a key costs one hit test and one redraw. Labels are borderless glyphs in MiSans.
 */
@SuppressLint("ViewConstructor")
class KeyboardView(context: Context) : View(context) {

    interface Listener {
        fun onText(text: String) {}
        fun onBackspace() {}
        fun onEnter() {}
        fun onCandidate(slot: Int) {}
        fun onTool(tool: Tool) {}
        fun onPickKeyboard() {}
        fun onStripMenu() {}
        /** The space bar became a cursor pad: the word in progress ends. */
        fun onCursorStart() {}
        /** Move the cursor by characters (dx) and lines (dy). */
        fun onCursor(dx: Int, dy: Int) {}
        fun onCursorEnd() {}
        fun onSearchClose() {}
        fun onSearchPick(emoji: String) {}
    }

    var listener: Listener? = null

    var palette: Palette = Palette.DARK
        set(value) { field = value; invalidate() }
    var page: Page = Page.LETTERS
        set(value) { if (field != value) { field = value; relayout() } }
    var shift: Shift = Shift.OFF
        set(value) { if (field != value) { field = value; relayout() } }
    var numberRow: Boolean = true
        set(value) { if (field != value) { field = value; requestLayout(); relayout() } }
    var strip: Strip = Strip.Tools
        set(value) { field = value; invalidate() }
    var enterAction: EnterAction = EnterAction.NEWLINE
        set(value) { field = value; invalidate() }
    var keyPreview = true
    var holdMillis = 400L
    /** Holding a top-row letter gives its digit. */
    var holdDigits = true
        set(value) { if (field != value) { field = value; relayout() } }
    /** Sliding along the space bar moves the cursor. */
    var spaceSwipe = true
    /** Holding the space bar turns the keys into a cursor pad. */
    var spaceHold = true
    /** Holding the comma picks another keyboard. */
    var commaHold = true

    private val dp = resources.displayMetrics.density
    private val sp = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 1f, resources.displayMetrics)
    private val stripHeight = 48 * dp
    private val rowHeight = 54 * dp
    private val font: Typeface = try {
        resources.getFont(R.font.misans_vf)
    } catch (e: Exception) {
        Typeface.DEFAULT
    }
    private val fontStrong: Typeface =
        if (Build.VERSION.SDK_INT >= 28) Typeface.create(font, 630, false) else font

    private val glyph = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = font; textAlign = Paint.Align.CENTER }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }

    private class Hit(val key: Key, val rect: RectF)
    private var hits: List<Hit> = emptyList()

    /** The letter page's row count sets the height; the other pages share it out. */
    private fun keyRows(): Int = Layouts.rows(Page.LETTERS, numberRow = numberRow).size

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val w = MeasureSpec.getSize(widthMeasureSpec)
        val h = (stripHeight + keyRows() * rowHeight).toInt()
        setMeasuredDimension(w, h)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) = relayout()

    private fun relayout() {
        val rows = Layouts.rows(page, shift, numberRow, holdDigits)
        val w = width.toFloat()
        val rowH = (height - stripHeight) / rows.size
        val out = ArrayList<Hit>()
        rows.forEachIndexed { r, row ->
            val unit = w / 10f
            var x = row.inset * unit
            val top = stripHeight + r * rowH
            for (k in row.keys) {
                out += Hit(k, RectF(x, top, x + k.width * unit, top + rowH))
                x += k.width * unit
            }
        }
        hits = out
        invalidate()
    }

    // ---- drawing ------------------------------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(palette.background)
        fill.color = palette.strip
        canvas.drawRect(0f, 0f, width.toFloat(), stripHeight, fill)
        when (val s = strip) {
            Strip.Tools -> drawTools(canvas)
            is Strip.Candidates -> drawCandidates(canvas, s)
            is Strip.Search -> drawSearch(canvas, s)
        }
        for (h in hits) drawKey(canvas, h, h === pressed && !holdFired)
        pressed?.let { if (keyPreview && it.key.kind == KeyKind.CHAR && !holdFired) drawPreview(canvas, it) }
        if (cursor != Cursor.OFF) drawCursorPad(canvas)
        if (variantsOf != null) drawVariants(canvas)
    }

    /** The cursor pad: the keys fade, the space bar shows the arrows. */
    private fun drawCursorPad(canvas: Canvas) {
        fill.color = palette.background
        fill.alpha = 225
        canvas.drawRect(0f, stripHeight, width.toFloat(), height.toFloat(), fill)
        fill.alpha = 255
        val space = hits.firstOrNull { it.key.kind == KeyKind.SPACE } ?: return
        val cx = space.rect.centerX()
        val cy = space.rect.centerY()
        val s = 7 * dp
        stroke(palette.glyph, 2.2f * dp)
        for (dir in listOf(-1f, 1f)) {
            val x = cx + dir * 22 * dp
            canvas.drawLine(x, cy - s, x + dir * s, cy, line)
            canvas.drawLine(x + dir * s, cy, x, cy + s, line)
        }
        if (cursor == Cursor.PAD) {
            canvas.drawLine(cx - s, cy - 14 * dp, cx, cy - 14 * dp - s, line)
            canvas.drawLine(cx, cy - 14 * dp - s, cx + s, cy - 14 * dp, line)
            canvas.drawLine(cx - s, cy + 14 * dp, cx, cy + 14 * dp + s, line)
            canvas.drawLine(cx, cy + 14 * dp + s, cx + s, cy + 14 * dp, line)
        }
    }

    /** The row of a held letter's forms, above it; the one the finger is on is marked. */
    private fun variantRects(): List<RectF> {
        val h = variantsOf ?: return emptyList()
        val n = h.key.variants.size
        val w = h.rect.width()
        val left = (h.rect.left).coerceIn(0f, maxOf(0f, width - n * w))
        val top = maxOf(0f, h.rect.top - h.rect.height() - 4 * dp)
        return List(n) { RectF(left + it * w, top, left + (it + 1) * w, top + h.rect.height()) }
    }

    private fun drawVariants(canvas: Canvas) {
        val h = variantsOf ?: return
        val rects = variantRects()
        fill.color = palette.preview
        canvas.drawRoundRect(RectF(rects.first().left, rects.first().top, rects.last().right, rects.last().bottom), 10 * dp, 10 * dp, fill)
        h.key.variants.forEachIndexed { i, v ->
            val r = RectF(rects[i]).apply { inset(3 * dp, 3 * dp) }
            if (i == variantChoice) {
                fill.color = palette.accent
                canvas.drawRoundRect(r, 8 * dp, 8 * dp, fill)
            }
            drawLabel(canvas, v, r.centerX(), r.centerY(), 24 * sp, if (i == variantChoice) palette.onAccent else palette.glyph)
        }
    }

    private fun textSize(k: Key): Float = when (k.kind) {
        KeyKind.CHAR -> if (k.label.length > 1) 18 * sp else 26 * sp
        KeyKind.COMMA, KeyKind.PERIOD -> 28 * sp
        else -> 17 * sp
    }

    private fun drawLabel(canvas: Canvas, text: String, cx: Float, cy: Float, size: Float, color: Int, tf: Typeface = font) {
        glyph.typeface = tf
        glyph.textSize = size
        glyph.color = color
        val m = glyph.fontMetrics
        canvas.drawText(text, cx, cy - (m.ascent + m.descent) / 2, glyph)
    }

    private fun drawKey(canvas: Canvas, h: Hit, down: Boolean) {
        val r = h.rect
        val cx = r.centerX()
        val cy = r.centerY()
        if (down && h.key.kind != KeyKind.ENTER && h.key.kind != KeyKind.SPACE) {
            fill.color = palette.pressed
            canvas.drawRoundRect(r.insetBy(3 * dp, 4 * dp), 8 * dp, 8 * dp, fill)
        }
        val k = h.key
        when (k.kind) {
            KeyKind.CHAR -> drawLabel(canvas, k.label, cx, cy, textSize(k), palette.glyph)
            KeyKind.SYMBOLS, KeyKind.MORE_SYMBOLS, KeyKind.LETTERS ->
                drawLabel(canvas, k.label, cx, cy, textSize(k), palette.glyph)
            KeyKind.COMMA -> {
                drawLabel(canvas, ",", cx, cy + 2 * dp, textSize(k), palette.glyph)
                drawGlobe(canvas, cx, r.top + 13 * dp, 6.5f * dp, palette.dim)
            }
            KeyKind.PERIOD -> drawLabel(canvas, ".", cx, cy, textSize(k), palette.glyph)
            KeyKind.SPACE -> {
                fill.color = if (down) palette.pressed else palette.space
                val bar = RectF(r.left + 4 * dp, cy - 18 * dp, r.right - 4 * dp, cy + 18 * dp)
                canvas.drawRoundRect(bar, 8 * dp, 8 * dp, fill)
                drawTile(canvas, cx, cy, 7 * dp)
            }
            KeyKind.SHIFT -> drawShift(canvas, cx, cy, 11 * dp)
            KeyKind.BACKSPACE -> drawBackspace(canvas, cx, cy, 12 * dp)
            KeyKind.ENTER -> {
                fill.color = if (down) palette.accentPressed else palette.accent
                canvas.drawCircle(cx, cy, minOf(r.height(), r.width()) * 0.36f, fill)
                drawEnter(canvas, cx, cy, 9 * dp, palette.onAccent)
            }
        }
    }

    private fun drawPreview(canvas: Canvas, h: Hit) {
        val r = h.rect
        val w = maxOf(r.width() * 1.1f, 44 * dp)
        val bubble = RectF(r.centerX() - w / 2, r.top - 62 * dp, r.centerX() + w / 2, r.top - 4 * dp)
        if (bubble.left < 0) bubble.offset(-bubble.left, 0f)
        if (bubble.right > width) bubble.offset(width - bubble.right, 0f)
        if (bubble.top < 0) bubble.offset(0f, -bubble.top)
        fill.color = palette.preview
        canvas.drawRoundRect(bubble, 10 * dp, 10 * dp, fill)
        drawLabel(canvas, h.key.label, bubble.centerX(), bubble.centerY(), 34 * sp, palette.glyph)
    }

    private fun stroke(color: Int, width: Float = 1.8f * dp) {
        line.color = color
        line.strokeWidth = width
    }

    private fun drawShift(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        val p = Path().apply {
            moveTo(cx, cy - s)
            lineTo(cx + s, cy)
            lineTo(cx + s * 0.45f, cy)
            lineTo(cx + s * 0.45f, cy + s * 0.75f)
            lineTo(cx - s * 0.45f, cy + s * 0.75f)
            lineTo(cx - s * 0.45f, cy)
            lineTo(cx - s, cy)
            close()
        }
        if (shift == Shift.OFF) {
            stroke(palette.glyph)
            canvas.drawPath(p, line)
        } else {
            fill.color = palette.glyph
            canvas.drawPath(p, fill)
        }
        if (shift == Shift.LOCKED) {
            stroke(palette.glyph, 2.2f * dp)
            canvas.drawLine(cx - s * 0.45f, cy + s * 1.15f, cx + s * 0.45f, cy + s * 1.15f, line)
        }
    }

    private fun drawBackspace(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        stroke(palette.glyph)
        val p = Path().apply {
            moveTo(cx - s * 1.15f, cy)
            lineTo(cx - s * 0.5f, cy - s * 0.7f)
            lineTo(cx + s, cy - s * 0.7f)
            lineTo(cx + s, cy + s * 0.7f)
            lineTo(cx - s * 0.5f, cy + s * 0.7f)
            close()
        }
        canvas.drawPath(p, line)
        val x0 = cx + s * 0.25f
        val d = s * 0.3f
        canvas.drawLine(x0 - d, cy - d, x0 + d, cy + d, line)
        canvas.drawLine(x0 - d, cy + d, x0 + d, cy - d, line)
    }

    private fun drawEnter(canvas: Canvas, cx: Float, cy: Float, s: Float, color: Int) {
        stroke(color, 2.2f * dp)
        when (enterAction) {
            EnterAction.NEWLINE -> {
                canvas.drawLine(cx + s * 0.8f, cy - s * 0.7f, cx + s * 0.8f, cy + s * 0.2f, line)
                canvas.drawLine(cx + s * 0.8f, cy + s * 0.2f, cx - s * 0.8f, cy + s * 0.2f, line)
                canvas.drawLine(cx - s * 0.8f, cy + s * 0.2f, cx - s * 0.35f, cy - s * 0.25f, line)
                canvas.drawLine(cx - s * 0.8f, cy + s * 0.2f, cx - s * 0.35f, cy + s * 0.65f, line)
            }
            EnterAction.SEND, EnterAction.GO, EnterAction.NEXT -> {
                canvas.drawLine(cx - s * 0.8f, cy, cx + s * 0.8f, cy, line)
                canvas.drawLine(cx + s * 0.8f, cy, cx + s * 0.25f, cy - s * 0.55f, line)
                canvas.drawLine(cx + s * 0.8f, cy, cx + s * 0.25f, cy + s * 0.55f, line)
            }
            EnterAction.SEARCH -> {
                canvas.drawCircle(cx - s * 0.15f, cy - s * 0.15f, s * 0.55f, line)
                canvas.drawLine(cx + s * 0.25f, cy + s * 0.25f, cx + s * 0.8f, cy + s * 0.8f, line)
            }
            EnterAction.DONE -> {
                canvas.drawLine(cx - s * 0.75f, cy, cx - s * 0.2f, cy + s * 0.55f, line)
                canvas.drawLine(cx - s * 0.2f, cy + s * 0.55f, cx + s * 0.8f, cy - s * 0.55f, line)
            }
        }
    }

    private fun drawGlobe(canvas: Canvas, cx: Float, cy: Float, r: Float, color: Int) {
        stroke(color, 1.2f * dp)
        canvas.drawCircle(cx, cy, r, line)
        canvas.drawOval(cx - r * 0.45f, cy - r, cx + r * 0.45f, cy + r, line)
        canvas.drawLine(cx - r, cy, cx + r, cy, line)
    }

    /** The xrnkey tile, faint, where the reference shows its name. */
    private fun drawTile(canvas: Canvas, cx: Float, cy: Float, r: Float) {
        fill.color = Color.argb(90, Color.red(palette.accent), Color.green(palette.accent), Color.blue(palette.accent))
        canvas.drawRoundRect(RectF(cx - r, cy - r, cx + r, cy + r), r * 0.48f, r * 0.48f, fill)
    }

    // The strip -------------------------------------------------------------------------------

    private fun toolSlots(): Int = Tool.entries.size

    private fun drawTools(canvas: Canvas) {
        val w = width.toFloat() / toolSlots()
        Tool.entries.forEachIndexed { i, t -> drawTool(canvas, t, w * i + w / 2, stripHeight / 2, 10 * dp) }
    }

    private fun drawTool(canvas: Canvas, t: Tool, cx: Float, cy: Float, s: Float) {
        stroke(palette.dim, 1.8f * dp)
        when (t) {
            Tool.THEME -> {
                canvas.drawCircle(cx, cy, s, line)
                fill.color = palette.dim
                canvas.drawArc(cx - s, cy - s, cx + s, cy + s, 90f, 180f, true, fill)
            }
            Tool.SETTINGS -> {
                // A cog: eight flat teeth around a ring.
                val cog = Path()
                for (i in 0 until 32) {
                    val a = Math.toRadians(i * 360.0 / 32 - 90)
                    val tooth = (i / 2) % 2 == 0
                    val rr = if (tooth) s else s * 0.74f
                    val px = cx + (Math.cos(a) * rr).toFloat()
                    val py = cy + (Math.sin(a) * rr).toFloat()
                    if (i == 0) cog.moveTo(px, py) else cog.lineTo(px, py)
                }
                cog.close()
                canvas.drawPath(cog, line)
                canvas.drawCircle(cx, cy, s * 0.32f, line)
            }
            Tool.CLIPBOARD -> {
                canvas.drawRoundRect(RectF(cx - s * 0.75f, cy - s * 0.85f, cx + s * 0.75f, cy + s), 3 * dp, 3 * dp, line)
                canvas.drawRoundRect(RectF(cx - s * 0.35f, cy - s * 1.1f, cx + s * 0.35f, cy - s * 0.65f), 2 * dp, 2 * dp, line)
            }
            Tool.CURSOR -> {
                canvas.drawLine(cx, cy - s, cx, cy + s, line)
                canvas.drawLine(cx - s * 0.3f, cy - s, cx + s * 0.3f, cy - s, line)
                canvas.drawLine(cx - s * 0.3f, cy + s, cx + s * 0.3f, cy + s, line)
                canvas.drawLine(cx - s * 0.65f, cy, cx - s * 1.0f, cy - s * 0.35f, line)
                canvas.drawLine(cx - s * 0.65f, cy, cx - s * 1.0f, cy + s * 0.35f, line)
                canvas.drawLine(cx + s * 0.65f, cy, cx + s * 1.0f, cy - s * 0.35f, line)
                canvas.drawLine(cx + s * 0.65f, cy, cx + s * 1.0f, cy + s * 0.35f, line)
            }
            Tool.EMOJI -> drawSmile(canvas, cx, cy, s)
        }
    }

    private fun drawSmile(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        stroke(palette.dim, 1.8f * dp)
        canvas.drawCircle(cx, cy, s, line)
        fill.color = palette.dim
        canvas.drawCircle(cx - s * 0.35f, cy - s * 0.25f, 1.6f * dp, fill)
        canvas.drawCircle(cx + s * 0.35f, cy - s * 0.25f, 1.6f * dp, fill)
        canvas.drawArc(cx - s * 0.5f, cy - s * 0.3f, cx + s * 0.5f, cy + s * 0.55f, 20f, 140f, false, line)
    }

    /** Where the strip's parts are: the menu, three slots, the emoji button. */
    private fun stripSlots(): List<RectF> {
        val side = 48 * dp
        val w = (width - 2 * side) / 3f
        return List(3) { RectF(side + it * w, 0f, side + (it + 1) * w, stripHeight) }
    }

    private fun drawCandidates(canvas: Canvas, s: Strip.Candidates) {
        val side = 48 * dp
        val cy = stripHeight / 2
        stroke(palette.dim, 1.8f * dp)
        for (dy in listOf(-5f, 0f, 5f)) canvas.drawLine(side / 2 - 8 * dp, cy + dy * dp, side / 2 + 8 * dp, cy + dy * dp, line)
        drawSmile(canvas, width - side / 2, cy, 10 * dp)
        val slots = stripSlots()
        listOf(s.left, s.centre, s.right).forEachIndexed { i, text ->
            if (text == null) return@forEachIndexed
            val r = slots[i]
            val size = 18 * sp
            glyph.textSize = size
            val shown = ellipsize(text, r.width() - 8 * dp)
            drawLabel(canvas, shown, r.centerX(), cy - if (i == 1 && s.more) 2 * dp else 0f, size,
                if (i == 1) palette.glyph else palette.dim, if (i == 1) fontStrong else font)
        }
        if (s.more) {
            fill.color = palette.dim
            val r = slots[1]
            for (dx in listOf(-6f, 0f, 6f)) canvas.drawCircle(r.centerX() + dx * dp, stripHeight - 8 * dp, 1.5f * dp, fill)
        }
    }

    /** The emoji search: a back arrow, the query with its cursor, then what it finds. */
    private fun drawSearch(canvas: Canvas, s: Strip.Search) {
        val side = 48 * dp
        val cy = stripHeight / 2
        stroke(palette.dim, 2f * dp)
        canvas.drawLine(side / 2 - 8 * dp, cy, side / 2 + 8 * dp, cy, line)
        canvas.drawLine(side / 2 - 8 * dp, cy, side / 2 - 2 * dp, cy - 6 * dp, line)
        canvas.drawLine(side / 2 - 8 * dp, cy, side / 2 - 2 * dp, cy + 6 * dp, line)
        val box = RectF(side, 7 * dp, side + width * 0.32f, stripHeight - 7 * dp)
        fill.color = palette.space
        canvas.drawRoundRect(box, 8 * dp, 8 * dp, fill)
        glyph.textSize = 16 * sp
        glyph.typeface = font
        val q = ellipsize(s.query, box.width() - 24 * dp)
        glyph.textAlign = Paint.Align.LEFT
        glyph.color = palette.glyph
        val m = glyph.fontMetrics
        canvas.drawText(q, box.left + 10 * dp, cy - (m.ascent + m.descent) / 2, glyph)
        glyph.textAlign = Paint.Align.CENTER
        val caret = box.left + 10 * dp + glyph.measureText(q) + 2 * dp
        fill.color = palette.accent
        canvas.drawRect(caret, box.top + 8 * dp, caret + 2 * dp, box.bottom - 8 * dp, fill)
        val w = 44 * dp
        searchResults(s).forEachIndexed { i, e ->
            drawLabel(canvas, e, box.right + 8 * dp + i * w + w / 2, cy, 22 * sp, palette.glyph, Typeface.DEFAULT)
        }
    }

    private fun searchResults(s: Strip.Search): List<String> {
        val room = ((width - (48 * dp + width * 0.32f + 8 * dp)) / (44 * dp)).toInt()
        return s.results.take(maxOf(0, room))
    }

    private fun ellipsize(text: String, max: Float): String {
        if (glyph.measureText(text) <= max) return text
        var t = text
        while (t.length > 1 && glyph.measureText("$t…") > max) t = t.dropLast(1)
        return "$t…"
    }

    // ---- touch --------------------------------------------------------------------------------

    private var pressed: Hit? = null
    private var pressedPointer = -1
    private var holdFired = false
    private var lastShiftTap = 0L

    private enum class Cursor { OFF, SWIPE, PAD }

    private var cursor = Cursor.OFF
    private var anchorX = 0f
    private var anchorY = 0f
    private val cursorStep get() = 10 * dp
    private val lineStep get() = 28 * dp
    private val slop = ViewConfiguration.get(context).scaledTouchSlop

    /** The key whose forms are showing, and the one the finger is on. */
    private var variantsOf: Hit? = null
    private var variantChoice = 0

    private val hold = Runnable {
        val h = pressed ?: return@Runnable
        when {
            h.key.kind == KeyKind.COMMA -> if (commaHold) { holdFired = true; listener?.onPickKeyboard() }
            h.key.kind == KeyKind.SPACE -> if (spaceHold && cursor == Cursor.OFF) startCursor(Cursor.PAD)
            h.key.variants.size > 1 -> { holdFired = true; variantsOf = h; variantChoice = 0 }
            h.key.hold != null -> { holdFired = true; typeChar(h.key.hold!!) }
        }
        invalidate()
    }

    private fun startCursor(mode: Cursor) {
        cursor = mode
        holdFired = true
        listener?.onCursorStart()
        invalidate()
    }

    private fun typeChar(text: String) {
        listener?.onText(text)
        if (shift == Shift.ONCE) shift = Shift.OFF
    }

    /** The finger moved while the space bar is held or swiped. */
    private fun moveCursor(x: Float, y: Float) {
        if (cursor == Cursor.OFF) {
            if (!spaceSwipe || Math.abs(x - anchorX) < slop) return
            removeCallbacks(hold)
            startCursor(Cursor.SWIPE)
            anchorX += if (x > anchorX) slop.toFloat() else -slop.toFloat()
        }
        val dx = ((x - anchorX) / cursorStep).toInt()
        val dy = if (cursor == Cursor.PAD) ((y - anchorY) / lineStep).toInt() else 0
        if (dx != 0 || dy != 0) {
            listener?.onCursor(dx, dy)
            anchorX += dx * cursorStep
            anchorY += dy * lineStep
        }
    }

    private val repeat = object : Runnable {
        override fun run() {
            if (pressed?.key?.kind != KeyKind.BACKSPACE) return
            listener?.onBackspace()
            postDelayed(this, 50)
        }
    }

    private fun hitAt(x: Float, y: Float): Hit? = hits.firstOrNull { it.rect.contains(x, y) }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                // A second finger lands before the first lifts: the first key counts.
                if (pressed != null) release(true)
                val i = e.actionIndex
                val x = e.getX(i)
                val y = e.getY(i)
                if (y < stripHeight) {
                    tapStrip(x)
                    return true
                }
                press(hitAt(x, y), e.getPointerId(i), x, y)
            }
            MotionEvent.ACTION_MOVE -> {
                val i = e.findPointerIndex(pressedPointer)
                if (i >= 0 && pressed?.key?.kind == KeyKind.SPACE) {
                    moveCursor(e.getX(i), e.getY(i))
                } else if (i >= 0 && variantsOf != null) {
                    val rects = variantRects()
                    val x = e.getX(i).coerceIn(rects.first().left, rects.last().right - 1)
                    val c = rects.indexOfFirst { x >= it.left && x < it.right }
                    if (c >= 0 && c != variantChoice) { variantChoice = c; invalidate() }
                } else if (i >= 0) {
                    val h = hitAt(e.getX(i), e.getY(i))
                    if (h != null && h !== pressed && pressed?.key?.kind == KeyKind.CHAR && !holdFired) {
                        removeCallbacks(hold)
                        pressed = h
                        if (h.key.hold != null) postDelayed(hold, holdMillis)
                        invalidate()
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP ->
                if (e.getPointerId(e.actionIndex) == pressedPointer) release(true)
            MotionEvent.ACTION_CANCEL -> release(false)
        }
        return true
    }

    private fun press(h: Hit?, pointer: Int, x: Float = 0f, y: Float = 0f) {
        pressed = h
        pressedPointer = pointer
        holdFired = false
        anchorX = x
        anchorY = y
        if (h == null) return
        when (h.key.kind) {
            KeyKind.SPACE -> postDelayed(hold, holdMillis)
            KeyKind.BACKSPACE -> {
                listener?.onBackspace()
                postDelayed(repeat, holdMillis)
            }
            KeyKind.COMMA -> postDelayed(hold, holdMillis)
            else -> if (h.key.hold != null) postDelayed(hold, holdMillis)
        }
        invalidate()
    }

    private fun release(act: Boolean) {
        val h = pressed
        removeCallbacks(hold)
        removeCallbacks(repeat)
        pressed = null
        pressedPointer = -1
        val shown = variantsOf
        variantsOf = null
        when {
            cursor != Cursor.OFF -> {
                cursor = Cursor.OFF
                listener?.onCursorEnd()
            }
            shown != null -> if (act) typeChar(shown.key.variants[variantChoice])
            h != null && act && !holdFired -> tap(h.key)
        }
        holdFired = false
        invalidate()
    }

    /** A tap, as the keyboard acts on it (public for the tests and the service). */
    fun tap(k: Key) {
        val l = listener
        when (k.kind) {
            KeyKind.CHAR -> {
                l?.onText(k.label)
                if (shift == Shift.ONCE) shift = Shift.OFF
            }
            KeyKind.COMMA, KeyKind.PERIOD -> l?.onText(k.text)
            KeyKind.SPACE -> {
                l?.onText(" ")
                // After a symbol and a space, the letters come back on their own.
                if (page != Page.LETTERS) page = Page.LETTERS
            }
            KeyKind.SHIFT -> {
                val now = SystemClock.uptimeMillis()
                shift = when {
                    shift == Shift.ONCE && now - lastShiftTap < 300 -> Shift.LOCKED
                    shift == Shift.OFF -> Shift.ONCE
                    else -> Shift.OFF
                }
                lastShiftTap = now
            }
            KeyKind.BACKSPACE -> Unit
            KeyKind.SYMBOLS -> page = Page.SYMBOLS
            KeyKind.MORE_SYMBOLS -> page = Page.MORE_SYMBOLS
            KeyKind.LETTERS -> page = Page.LETTERS
            KeyKind.ENTER -> l?.onEnter()
        }
    }

    private fun tapStrip(x: Float) {
        when (strip) {
            Strip.Tools -> {
                val i = (x / (width.toFloat() / toolSlots())).toInt().coerceIn(0, toolSlots() - 1)
                listener?.onTool(Tool.entries[i])
            }
            is Strip.Search -> {
                val s = strip as Strip.Search
                val side = 48 * dp
                val start = side + width * 0.32f + 8 * dp
                val i = ((x - start) / (44 * dp)).toInt()
                val shown = searchResults(s)
                when {
                    x < side -> listener?.onSearchClose()
                    x >= start && i in shown.indices -> listener?.onSearchPick(shown[i])
                }
            }
            is Strip.Candidates -> {
                val side = 48 * dp
                when {
                    x < side -> listener?.onStripMenu()
                    x > width - side -> listener?.onTool(Tool.EMOJI)
                    else -> stripSlots().indexOfFirst { x >= it.left && x < it.right }.let {
                        if (it >= 0) listener?.onCandidate(it)
                    }
                }
            }
        }
    }

    private fun RectF.insetBy(dx: Float, dy: Float) = RectF(left + dx, top + dy, right - dx, bottom - dy)
}

/** The keyboard's colours. Dark slate is the default; the accent is the xrnkey tile's purple. */
data class Palette(
    val background: Int,
    val strip: Int,
    val glyph: Int,
    val dim: Int,
    val space: Int,
    val pressed: Int,
    val preview: Int,
    val accent: Int,
    val accentPressed: Int,
    val onAccent: Int,
) {
    companion object {
        val DARK = Palette(
            background = 0xFF263238.toInt(),
            strip = 0xFF1E272C.toInt(),
            glyph = 0xFFECEFF1.toInt(),
            dim = 0xFFB0BEC5.toInt(),
            space = 0xFF37474F.toInt(),
            pressed = 0xFF37474F.toInt(),
            preview = 0xFF455A64.toInt(),
            accent = 0xFF5F3DC4.toInt(),
            accentPressed = 0xFF4C2FA3.toInt(),
            onAccent = 0xFFFFFFFF.toInt(),
        )
        val LIGHT = Palette(
            background = 0xFFECEFF1.toInt(),
            strip = 0xFFE3E7EA.toInt(),
            glyph = 0xFF1F2428.toInt(),
            dim = 0xFF5F6B73.toInt(),
            space = 0xFFFFFFFF.toInt(),
            pressed = 0xFFD5DBDF.toInt(),
            preview = 0xFFFFFFFF.toInt(),
            accent = 0xFF5F3DC4.toInt(),
            accentPressed = 0xFF4C2FA3.toInt(),
            onAccent = 0xFFFFFFFF.toInt(),
        )
    }
}

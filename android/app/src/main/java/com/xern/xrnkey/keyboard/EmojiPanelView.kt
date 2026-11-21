package com.xern.xrnkey.keyboard

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.util.TypedValue
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.view.ViewConfiguration
import android.widget.OverScroller
import com.xern.xrnkey.R
import com.xern.xrnkey.engine.Emoji
import com.xern.xrnkey.engine.SkinTones

/**
 * The emoji panel, in place of the strip and the keys (android/docs/feature-spec.md, "The emoji
 * panel"): the tab row (back to the keys, emoji, emoticons, symbols), the emoji suggested for
 * the words just typed, the grid (recents first), a round search button, and the category bar
 * with Backspace. Drawn on one Canvas, like the keys.
 */
@SuppressLint("ViewConstructor")
class EmojiPanelView(context: Context) : View(context) {

    interface Listener {
        fun onEmoji(text: String) {}
        /** A skin tone was chosen for `base`: show it from now on. */
        fun onSkinTone(base: String, toned: String) {}
        fun onClosePanel() {}
        fun onEmojiSearch() {}
        fun onBackspace() {}
    }

    var listener: Listener? = null
    var palette: Palette = Palette.DARK
        set(value) { field = value; invalidate() }
    var tab: EmojiTab = EmojiTab.EMOJI
        set(value) { if (field != value) { field = value; scrollGrid(0f); rebuild() } }
    /** The index's emoji, less those this phone's font can't draw (newer than its Android). */
    var emoji: List<Emoji> = emptyList()
        set(value) {
            val probe = Paint()
            field = value.filter { probe.hasGlyph(it.text) }
            rebuild()
        }
    var recents: List<String> = emptyList()
        set(value) { field = value; rebuild() }
    var tones: Map<String, String> = emptyMap()
        set(value) { field = value; rebuild() }
    /** Emoji for the words just typed, in the row under the tabs. */
    var suggestions: List<String> = emptyList()
        set(value) { field = value; invalidate() }

    private val dp = resources.displayMetrics.density
    private val sp = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 1f, resources.displayMetrics)
    private val tabHeight = 48 * dp
    private val suggestHeight = 52 * dp
    private val barHeight = 48 * dp
    private val font: Typeface = try { resources.getFont(R.font.misans_vf) } catch (e: Exception) { Typeface.DEFAULT }

    private val glyph = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeWidth = 1.8f * dp
    }

    private var rows: List<GridRow> = emptyList()
    private var offset = 0f
    private val scroller = OverScroller(context)

    private fun rebuild() {
        rows = EmojiGrid.rows(tab, emoji, recents, tones)
        scrollGrid(offset)
        invalidate()
    }

    // ---- geometry -----------------------------------------------------------------------------

    private fun gridTop() = tabHeight + if (tab == EmojiTab.EMOJI) suggestHeight else 0f
    private fun gridBottom() = height - barHeight
    private fun cellWidth() = width.toFloat() / EmojiGrid.columns(tab)
    private fun rowHeight() = if (tab == EmojiTab.EMOTICONS) 56 * dp else cellWidth() * 0.86f
    private fun maxScroll() = maxOf(0f, rows.size * rowHeight() - (gridBottom() - gridTop()))
    private fun scrollGrid(y: Float) { offset = y.coerceIn(0f, maxScroll()) }

    private fun searchButton(): RectF {
        val r = 26 * dp
        val cx = width - 16 * dp - r
        val cy = gridBottom() - 12 * dp - r
        return RectF(cx - r, cy - r, cx + r, cy + r)
    }

    private val tabWidth get() = 64 * dp

    /** The section the top of the grid is in, for the category bar. */
    private fun currentSection(): Int {
        val row = (offset / rowHeight()).toInt().coerceIn(0, maxOf(0, rows.size - 1))
        return rows.getOrNull(row)?.section ?: 0
    }

    // ---- drawing ------------------------------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(palette.background)
        drawTabs(canvas)
        if (tab == EmojiTab.EMOJI) drawSuggestions(canvas)
        drawGrid(canvas)
        if (tab == EmojiTab.EMOJI) drawSearchButton(canvas)
        drawBar(canvas)
        drawTonePopup(canvas)
    }

    private fun label(canvas: Canvas, text: String, cx: Float, cy: Float, size: Float, color: Int, tf: Typeface? = font) {
        glyph.typeface = tf
        glyph.textSize = size
        glyph.color = color
        val m = glyph.fontMetrics
        canvas.drawText(text, cx, cy - (m.ascent + m.descent) / 2, glyph)
    }

    private fun drawTabs(canvas: Canvas) {
        fill.color = palette.strip
        canvas.drawRect(0f, 0f, width.toFloat(), tabHeight, fill)
        val cy = tabHeight / 2
        label(canvas, "ABC", tabWidth / 2, cy, 15 * sp, palette.dim)
        val tabs = listOf(EmojiTab.EMOJI to "☺", EmojiTab.EMOTICONS to ":-)", EmojiTab.SYMBOLS to "Ω")
        tabs.forEachIndexed { i, (t, text) ->
            val cx = tabWidth * (i + 1) + tabWidth / 2
            if (t == EmojiTab.EMOJI) drawSmile(canvas, cx, cy, 10 * dp, if (t == tab) palette.glyph else palette.dim)
            else label(canvas, text, cx, cy, 18 * sp, if (t == tab) palette.glyph else palette.dim)
            if (t == tab) {
                fill.color = palette.accent
                canvas.drawRoundRect(RectF(cx - 18 * dp, tabHeight - 3 * dp, cx + 18 * dp, tabHeight), 2 * dp, 2 * dp, fill)
            }
        }
    }

    private fun drawSmile(canvas: Canvas, cx: Float, cy: Float, s: Float, color: Int) {
        line.color = color
        canvas.drawCircle(cx, cy, s, line)
        fill.color = color
        canvas.drawCircle(cx - s * 0.35f, cy - s * 0.25f, 1.6f * dp, fill)
        canvas.drawCircle(cx + s * 0.35f, cy - s * 0.25f, 1.6f * dp, fill)
        canvas.drawArc(cx - s * 0.5f, cy - s * 0.3f, cx + s * 0.5f, cy + s * 0.55f, 20f, 140f, false, line)
    }

    private fun drawSuggestions(canvas: Canvas) {
        val top = tabHeight
        val w = cellWidth()
        val shown = suggestions.take(EmojiGrid.columns(tab))
        val left = (width - shown.size * w) / 2
        shown.forEachIndexed { i, e -> label(canvas, e, left + i * w + w / 2, top + suggestHeight / 2, w * 0.5f, palette.glyph, null) }
        fill.color = palette.strip
        canvas.drawRect(0f, top + suggestHeight - 1 * dp, width.toFloat(), top + suggestHeight, fill)
    }

    private fun drawGrid(canvas: Canvas) {
        val top = gridTop()
        val bottom = gridBottom()
        val rh = rowHeight()
        val cw = cellWidth()
        canvas.save()
        canvas.clipRect(0f, top, width.toFloat(), bottom)
        val first = (offset / rh).toInt().coerceAtLeast(0)
        val last = ((offset + bottom - top) / rh).toInt() + 1
        for (r in first..minOf(last, rows.size - 1)) {
            val y = top + r * rh - offset
            rows[r].cells.forEachIndexed { c, cell ->
                val cx = c * cw + cw / 2
                if (tab == EmojiTab.EMOJI) label(canvas, cell.text, cx, y + rh / 2, cw * 0.52f, palette.glyph, null)
                else label(canvas, cell.text, cx, y + rh / 2, if (tab == EmojiTab.EMOTICONS) 17 * sp else 22 * sp, palette.glyph)
            }
        }
        canvas.restore()
    }

    private fun drawSearchButton(canvas: Canvas) {
        val b = searchButton()
        fill.color = palette.preview
        canvas.drawCircle(b.centerX(), b.centerY(), b.width() / 2, fill)
        line.color = palette.glyph
        line.strokeWidth = 2.4f * dp
        val s = 8 * dp
        canvas.drawCircle(b.centerX() - s * 0.2f, b.centerY() - s * 0.2f, s * 0.6f, line)
        canvas.drawLine(b.centerX() + s * 0.25f, b.centerY() + s * 0.25f, b.centerX() + s * 0.85f, b.centerY() + s * 0.85f, line)
        line.strokeWidth = 1.8f * dp
    }

    /** The bar's slots: on the emoji tab the recents, the groups, then Backspace. */
    private fun barSlots() = if (tab == EmojiTab.EMOJI) EmojiGrid.groups.size + 2 else 1

    private fun drawBar(canvas: Canvas) {
        val top = gridBottom()
        fill.color = palette.strip
        canvas.drawRect(0f, top, width.toFloat(), height.toFloat(), fill)
        val cy = top + barHeight / 2
        val w = width.toFloat() / maxOf(barSlots(), 8)
        if (tab == EmojiTab.EMOJI) {
            val current = currentSection()
            for (i in 0..EmojiGrid.groups.size) {
                val cx = i * w + w / 2
                if (i == current) {
                    fill.color = palette.space
                    canvas.drawRoundRect(RectF(cx - w / 2 + 3 * dp, top + 6 * dp, cx + w / 2 - 3 * dp, height - 6 * dp), 8 * dp, 8 * dp, fill)
                }
                if (i == 0) drawClock(canvas, cx, cy, 9 * dp) else label(canvas, EmojiGrid.groupIcons[i - 1], cx, cy, 18 * sp, palette.glyph, null)
            }
        }
        drawBackspace(canvas, width - w / 2, cy, 10 * dp)
    }

    private fun drawClock(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        line.color = palette.dim
        canvas.drawCircle(cx, cy, s, line)
        canvas.drawLine(cx, cy, cx, cy - s * 0.6f, line)
        canvas.drawLine(cx, cy, cx + s * 0.45f, cy + s * 0.2f, line)
    }

    private fun drawBackspace(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        line.color = palette.dim
        val p = android.graphics.Path().apply {
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

    // ---- the skin-tone popup -------------------------------------------------------------------

    private var toneBase: String? = null
    private var toneCell: RectF? = null
    private var toneChoice = 0

    private fun toneRects(): List<RectF> {
        val cell = toneCell ?: return emptyList()
        val w = cellWidth()
        val n = 6
        val left = (cell.centerX() - n * w / 2).coerceIn(0f, width - n * w)
        val top = maxOf(0f, cell.top - w * 1.05f)
        return List(n) { RectF(left + it * w, top, left + (it + 1) * w, top + w) }
    }

    private fun drawTonePopup(canvas: Canvas) {
        val base = toneBase ?: return
        val rects = toneRects()
        if (rects.isEmpty()) return
        fill.color = palette.preview
        canvas.drawRoundRect(RectF(rects.first().left, rects.first().top, rects.last().right, rects.last().bottom), 12 * dp, 12 * dp, fill)
        SkinTones.variants(base).forEachIndexed { i, v ->
            if (i == toneChoice) {
                fill.color = palette.space
                canvas.drawRoundRect(rects[i].apply { inset(3 * dp, 3 * dp) }, 10 * dp, 10 * dp, fill)
            }
            label(canvas, v, rects[i].centerX(), rects[i].centerY(), cellWidth() * 0.5f, palette.glyph, null)
        }
    }

    // ---- touch ---------------------------------------------------------------------------------

    private val slop = ViewConfiguration.get(context).scaledTouchSlop
    private var downX = 0f
    private var downY = 0f
    private var lastY = 0f
    private var dragging = false
    private var velocity: VelocityTracker? = null
    private var pressedCell: Pair<Cell, RectF>? = null

    private val hold = Runnable {
        val (cell, rect) = pressedCell ?: return@Runnable
        if (!cell.skinTones) return@Runnable
        toneBase = baseOf(cell.text)
        toneCell = rect
        toneChoice = 0
        invalidate()
    }

    /** The base emoji of a toned one (what the index lists). */
    private fun baseOf(text: String): String {
        var t = text
        for (m in SkinTones.modifiers) t = t.replace(m, "")
        return emoji.firstOrNull { it.text == t || it.text.replace("️", "") == t }?.text ?: t
    }

    private fun cellAt(x: Float, y: Float): Pair<Cell, RectF>? {
        if (y < gridTop() || y >= gridBottom()) return null
        val rh = rowHeight()
        val r = ((y - gridTop() + offset) / rh).toInt()
        val c = (x / cellWidth()).toInt()
        val cell = rows.getOrNull(r)?.cells?.getOrNull(c) ?: return null
        val top = gridTop() + r * rh - offset
        return cell to RectF(c * cellWidth(), top, (c + 1) * cellWidth(), top + rh)
    }

    override fun computeScroll() {
        if (scroller.computeScrollOffset()) {
            scrollGrid(scroller.currY.toFloat())
            postInvalidateOnAnimation()
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                scroller.forceFinished(true)
                downX = e.x; downY = e.y; lastY = e.y
                dragging = false
                velocity = VelocityTracker.obtain().also { it.addMovement(e) }
                pressedCell = cellAt(e.x, e.y)
                if (pressedCell != null) postDelayed(hold, 400)
            }
            MotionEvent.ACTION_MOVE -> {
                velocity?.addMovement(e)
                if (toneBase != null) {
                    val i = toneRects().indexOfFirst { e.x >= it.left && e.x < it.right }
                    if (i >= 0 && i != toneChoice) { toneChoice = i; invalidate() }
                    return true
                }
                if (!dragging && pressedCell != null && Math.abs(e.y - downY) > slop) {
                    dragging = true
                    removeCallbacks(hold)
                }
                if (dragging) {
                    scrollGrid(offset - (e.y - lastY))
                    invalidate()
                }
                lastY = e.y
            }
            MotionEvent.ACTION_UP -> {
                removeCallbacks(hold)
                val base = toneBase
                if (base != null) {
                    val toned = SkinTones.variants(base)[toneChoice]
                    toneBase = null
                    listener?.onSkinTone(base, toned)
                    listener?.onEmoji(toned)
                } else if (dragging) {
                    velocity?.let {
                        it.computeCurrentVelocity(1000)
                        scroller.fling(0, offset.toInt(), 0, -it.yVelocity.toInt(), 0, 0, 0, maxScroll().toInt())
                        postInvalidateOnAnimation()
                    }
                } else {
                    tap(e.x, e.y)
                }
                velocity?.recycle(); velocity = null
                pressedCell = null
                invalidate()
            }
            MotionEvent.ACTION_CANCEL -> {
                removeCallbacks(hold)
                toneBase = null
                pressedCell = null
                velocity?.recycle(); velocity = null
                invalidate()
            }
        }
        return true
    }

    /** A tap where the finger went down and lifted. Public for the tests. */
    fun tap(x: Float, y: Float) {
        val l = listener
        when {
            y < tabHeight -> {
                val slot = (x / tabWidth).toInt()
                when (slot) {
                    0 -> l?.onClosePanel()
                    1 -> tab = EmojiTab.EMOJI
                    2 -> tab = EmojiTab.EMOTICONS
                    3 -> tab = EmojiTab.SYMBOLS
                }
            }
            tab == EmojiTab.EMOJI && y < gridTop() -> {
                val w = cellWidth()
                val shown = suggestions.take(EmojiGrid.columns(tab))
                val left = (width - shown.size * w) / 2
                val i = ((x - left) / w).toInt()
                if (x >= left && i in shown.indices) l?.onEmoji(shown[i])
            }
            y >= gridBottom() -> {
                val w = width.toFloat() / maxOf(barSlots(), 8)
                val i = (x / w).toInt()
                when {
                    x >= width - w -> l?.onBackspace()
                    tab == EmojiTab.EMOJI && i <= EmojiGrid.groups.size -> {
                        val row = EmojiGrid.firstRow(rows, i)
                        if (row >= 0) { scroller.forceFinished(true); scrollGrid(row * rowHeight()); invalidate() }
                    }
                }
            }
            tab == EmojiTab.EMOJI && searchButton().contains(x, y) -> l?.onEmojiSearch()
            else -> cellAt(x, y)?.let { l?.onEmoji(it.first.text) }
        }
    }
}

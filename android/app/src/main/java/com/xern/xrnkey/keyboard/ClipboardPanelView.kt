package com.xern.xrnkey.keyboard

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.util.TypedValue
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.view.ViewConfiguration
import android.widget.OverScroller
import com.xern.xrnkey.R
import com.xern.xrnkey.clip.Clip

/**
 * The clipboard panel, in place of the strip and the keys: a header (back to the keys, clear),
 * then the clips as cards, two to a row, newest first. A tap pastes; holding a card offers pin
 * and delete over it.
 */
@SuppressLint("ViewConstructor")
class ClipboardPanelView(context: Context) : View(context) {

    interface Listener {
        fun onPaste(text: String) {}
        fun onPin(text: String, pinned: Boolean) {}
        fun onDelete(text: String) {}
        fun onClearClips() {}
        fun onClosePanel() {}
    }

    var listener: Listener? = null
    var palette: Palette = Palette.DARK
        set(value) { field = value; invalidate() }
    var clips: List<Clip> = emptyList()
        set(value) { field = value; layouts.clear(); actions = null; scrollGrid(offset); invalidate() }

    private val dp = resources.displayMetrics.density
    private val sp = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 1f, resources.displayMetrics)
    private val headerHeight = 48 * dp
    private val gap = 8 * dp
    private val cardHeight = 84 * dp
    private val font: Typeface = try { resources.getFont(R.font.misans_vf) } catch (e: Exception) { Typeface.DEFAULT }
    private val text = TextPaint(Paint.ANTI_ALIAS_FLAG).apply { typeface = font; textSize = 15 * sp }
    private val glyph = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER; typeface = font }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
        strokeWidth = 1.8f * dp
    }

    private val layouts = HashMap<String, StaticLayout>()
    private var offset = 0f
    private val scroller = OverScroller(context)

    /** The card whose pin and delete are showing. */
    private var actions: Int? = null

    // ---- geometry -----------------------------------------------------------------------------

    private fun cardWidth() = (width - 3 * gap) / 2

    private fun cardRect(i: Int): RectF {
        val row = i / 2
        val col = i % 2
        val left = gap + col * (cardWidth() + gap)
        val top = headerHeight + gap + row * (cardHeight + gap) - offset
        return RectF(left, top, left + cardWidth(), top + cardHeight)
    }

    private fun maxScroll(): Float {
        val rows = (clips.size + 1) / 2
        return maxOf(0f, gap + rows * (cardHeight + gap) - (height - headerHeight))
    }

    private fun scrollGrid(y: Float) { offset = y.coerceIn(0f, maxScroll()) }

    private fun backRect() = RectF(0f, 0f, 64 * dp, headerHeight)
    private fun clearRect() = RectF(width - 56 * dp, 0f, width.toFloat(), headerHeight)

    private fun pinRect(card: RectF) = RectF(card.centerX() - 52 * dp, card.centerY() - 22 * dp, card.centerX() - 8 * dp, card.centerY() + 22 * dp)
    private fun deleteRect(card: RectF) = RectF(card.centerX() + 8 * dp, card.centerY() - 22 * dp, card.centerX() + 52 * dp, card.centerY() + 22 * dp)

    // ---- drawing ------------------------------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(palette.background)
        canvas.save()
        canvas.clipRect(0f, headerHeight, width.toFloat(), height.toFloat())
        if (clips.isEmpty()) drawClipboard(canvas, width / 2f, headerHeight + (height - headerHeight) / 2, 22 * dp, palette.space)
        clips.forEachIndexed { i, clip ->
            val r = cardRect(i)
            if (r.bottom < headerHeight || r.top > height) return@forEachIndexed
            drawCard(canvas, clip, r, actions == i)
        }
        canvas.restore()
        drawHeader(canvas)
    }

    private fun drawHeader(canvas: Canvas) {
        fill.color = palette.strip
        canvas.drawRect(0f, 0f, width.toFloat(), headerHeight, fill)
        glyph.textSize = 15 * sp
        glyph.color = palette.dim
        val m = glyph.fontMetrics
        canvas.drawText("ABC", 32 * dp, headerHeight / 2 - (m.ascent + m.descent) / 2, glyph)
        drawClipboard(canvas, width / 2f, headerHeight / 2, 9 * dp, palette.glyph)
        if (clips.any { !it.pinned }) drawBin(canvas, clearRect().centerX(), headerHeight / 2, 9 * dp, palette.dim)
    }

    private fun layoutOf(clip: Clip, width: Int): StaticLayout = layouts.getOrPut(clip.text) {
        val shown = clip.text.trim().replace(Regex("\\s*\n\\s*"), " ⏎ ")
        StaticLayout.Builder.obtain(shown, 0, minOf(shown.length, 400), text, width)
            .setMaxLines(3)
            .setEllipsize(TextUtils.TruncateAt.END)
            .setAlignment(Layout.Alignment.ALIGN_NORMAL)
            .build()
    }

    private fun drawCard(canvas: Canvas, clip: Clip, r: RectF, showActions: Boolean) {
        fill.color = palette.space
        canvas.drawRoundRect(r, 12 * dp, 12 * dp, fill)
        val pad = 12 * dp
        val room = (r.width() - 2 * pad - if (clip.pinned) 18 * dp else 0f).toInt()
        text.color = palette.glyph
        canvas.save()
        canvas.translate(r.left + pad, r.top + pad)
        layoutOf(clip, room).draw(canvas)
        canvas.restore()
        if (clip.pinned) drawPin(canvas, r.right - 14 * dp, r.top + 14 * dp, 6 * dp, palette.accent)
        if (showActions) {
            fill.color = palette.preview
            canvas.drawRoundRect(r, 12 * dp, 12 * dp, fill)
            val p = pinRect(r)
            val d = deleteRect(r)
            fill.color = palette.space
            canvas.drawCircle(p.centerX(), p.centerY(), p.width() / 2, fill)
            canvas.drawCircle(d.centerX(), d.centerY(), d.width() / 2, fill)
            drawPin(canvas, p.centerX(), p.centerY(), 8 * dp, if (clip.pinned) palette.accent else palette.glyph)
            drawBin(canvas, d.centerX(), d.centerY(), 9 * dp, palette.glyph)
        }
    }

    private fun drawClipboard(canvas: Canvas, cx: Float, cy: Float, s: Float, color: Int) {
        line.color = color
        canvas.drawRoundRect(RectF(cx - s * 0.8f, cy - s, cx + s * 0.8f, cy + s), s * 0.2f, s * 0.2f, line)
        canvas.drawRoundRect(RectF(cx - s * 0.4f, cy - s * 1.15f, cx + s * 0.4f, cy - s * 0.75f), s * 0.1f, s * 0.1f, line)
    }

    private fun drawBin(canvas: Canvas, cx: Float, cy: Float, s: Float, color: Int) {
        line.color = color
        canvas.drawLine(cx - s, cy - s * 0.7f, cx + s, cy - s * 0.7f, line)
        canvas.drawLine(cx - s * 0.3f, cy - s, cx + s * 0.3f, cy - s, line)
        val body = Path().apply {
            moveTo(cx - s * 0.75f, cy - s * 0.7f)
            lineTo(cx - s * 0.6f, cy + s)
            lineTo(cx + s * 0.6f, cy + s)
            lineTo(cx + s * 0.75f, cy - s * 0.7f)
        }
        canvas.drawPath(body, line)
    }

    private fun drawPin(canvas: Canvas, cx: Float, cy: Float, s: Float, color: Int) {
        fill.color = color
        line.color = color
        canvas.drawCircle(cx, cy - s * 0.3f, s * 0.6f, fill)
        canvas.drawLine(cx, cy - s * 0.3f, cx, cy + s, line)
    }

    // ---- touch ---------------------------------------------------------------------------------

    private val slop = ViewConfiguration.get(context).scaledTouchSlop
    private var downY = 0f
    private var lastY = 0f
    private var dragging = false
    private var held = false
    private var velocity: VelocityTracker? = null
    private var pressed = -1

    private val hold = Runnable {
        if (pressed >= 0) {
            held = true
            actions = pressed
            invalidate()
        }
    }

    private fun cardAt(x: Float, y: Float): Int {
        if (y < headerHeight) return -1
        return clips.indices.firstOrNull { cardRect(it).contains(x, y) } ?: -1
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
                downY = e.y; lastY = e.y
                dragging = false
                held = false
                velocity = VelocityTracker.obtain().also { it.addMovement(e) }
                pressed = if (actions == null) cardAt(e.x, e.y) else -1
                if (pressed >= 0) postDelayed(hold, 400)
            }
            MotionEvent.ACTION_MOVE -> {
                velocity?.addMovement(e)
                if (!dragging && !held && Math.abs(e.y - downY) > slop) {
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
                if (dragging) {
                    velocity?.let {
                        it.computeCurrentVelocity(1000)
                        scroller.fling(0, offset.toInt(), 0, -it.yVelocity.toInt(), 0, 0, 0, maxScroll().toInt())
                        postInvalidateOnAnimation()
                    }
                } else if (!held) {
                    tap(e.x, e.y)
                }
                velocity?.recycle(); velocity = null
                pressed = -1
            }
            MotionEvent.ACTION_CANCEL -> {
                removeCallbacks(hold)
                velocity?.recycle(); velocity = null
                pressed = -1
            }
        }
        return true
    }

    /** Holding a card: shows its pin and delete. Public for the tests. */
    fun hold(index: Int) {
        actions = index.takeIf { it in clips.indices }
        invalidate()
    }

    /** A tap where the finger went down and lifted. Public for the tests. */
    fun tap(x: Float, y: Float) {
        val l = listener
        val shown = actions
        if (shown != null) {
            actions = null
            val clip = clips.getOrNull(shown)
            val r = cardRect(shown)
            when {
                clip == null -> Unit
                pinRect(r).contains(x, y) -> l?.onPin(clip.text, !clip.pinned)
                deleteRect(r).contains(x, y) -> l?.onDelete(clip.text)
            }
            invalidate()
            return
        }
        when {
            backRect().contains(x, y) -> l?.onClosePanel()
            clearRect().contains(x, y) -> if (clips.any { !it.pinned }) l?.onClearClips()
            else -> clips.getOrNull(cardAt(x, y))?.let { l?.onPaste(it.text) }
        }
    }
}

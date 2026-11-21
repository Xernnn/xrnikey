package com.xern.xrnkey.ime

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.inputmethodservice.InputMethodService
import android.text.InputType
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.FrameLayout
import com.xern.xrnkey.SettingsActivity
import com.xern.xrnkey.engine.EmojiIndex
import com.xern.xrnkey.engine.Model
import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.engine.TypingState
import com.xern.xrnkey.keyboard.EmojiPanelView
import com.xern.xrnkey.keyboard.EnterAction
import com.xern.xrnkey.keyboard.KeyboardView
import com.xern.xrnkey.keyboard.Page
import com.xern.xrnkey.keyboard.Palette
import com.xern.xrnkey.keyboard.Shift
import com.xern.xrnkey.keyboard.Strip
import com.xern.xrnkey.keyboard.Tool
import java.io.IOException

/**
 * The keyboard. The models are memory-mapped straight from the APK (uncompressed assets), so
 * they cost no copy and only the pages in use take RAM. Nothing needs a permission.
 *
 * The input view is a frame: the keys, and the panels that take their place (emoji). A panel
 * hides the keys but keeps their height, so the field above doesn't jump.
 */
class XrnkeyService : InputMethodService(), KeyboardView.Listener, EmojiPanelView.Listener {
    private var vi: Model? = null
    private var en: Model? = null
    private var models: Models? = null
    private var emoji: EmojiIndex? = null
    private var state: TypingState? = null
    private lateinit var typist: Typist
    private lateinit var strip: StripMaker
    private var keyboard: KeyboardView? = null
    private var emojiPanel: EmojiPanelView? = null

    /** The emoji search's query, while the keys type into it rather than the field. */
    private var searchQuery: String? = null

    private val editor = object : Editor {
        override fun setComposing(text: String) {
            currentInputConnection?.setComposingText(text, 1)
        }

        override fun commit(text: String) {
            currentInputConnection?.commitText(text, 1)
        }

        override fun finishComposing() {
            currentInputConnection?.finishComposingText()
        }

        override fun backspace() = sendDownUpKeyEvents(KeyEvent.KEYCODE_DEL)

        override fun deleteBefore(codePoints: Int) {
            currentInputConnection?.deleteSurroundingTextInCodePoints(codePoints, 0)
        }

        override fun textBefore(n: Int): String =
            currentInputConnection?.getTextBeforeCursor(n, 0)?.toString() ?: ""

        override fun enter() {
            val info = currentInputEditorInfo
            val action = info?.let { it.imeOptions and EditorInfo.IME_MASK_ACTION } ?: EditorInfo.IME_ACTION_NONE
            val noAction = info == null || info.imeOptions and EditorInfo.IME_FLAG_NO_ENTER_ACTION != 0 ||
                action == EditorInfo.IME_ACTION_NONE || action == EditorInfo.IME_ACTION_UNSPECIFIED
            if (noAction) sendKeyChar('\n') else currentInputConnection?.performEditorAction(action)
        }
    }

    override fun onCreate() {
        super.onCreate()
        vi = openModel("vi.xklm")
        en = openModel("en.xklm")
        models = Models(vi, en)
        models?.correction = Models.CORRECT_FLEXIBLE
        emoji = try {
            assets.open("emoji-index.tsv").use { EmojiIndex.load(it.readBytes()) }
        } catch (e: IOException) {
            null
        }
        state = TypingState().also { it.useModels(models) }
        typist = Typist(state, editor, models)
        strip = StripMaker(models, emoji)
    }

    private fun openModel(name: String): Model? = try {
        assets.openFd(name).use { Model.open(it.parcelFileDescriptor.fd, it.startOffset, it.length) }
    } catch (e: IOException) {
        null
    }

    override fun onDestroy() {
        state?.close()
        models?.close()
        emoji?.close()
        vi?.close()
        en?.close()
        super.onDestroy()
    }

    override fun onCreateInputView(): View {
        val frame = FrameLayout(this)
        val keys = KeyboardView(this).also {
            it.listener = this
            it.palette = savedPalette()
            keyboard = it
        }
        val panel = EmojiPanelView(this).also {
            it.listener = this
            it.palette = keys.palette
            it.visibility = View.GONE
            emojiPanel = it
        }
        frame.addView(keys, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        frame.addView(panel, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        return frame
    }

    /** Never the full-screen editor in landscape. */
    override fun onEvaluateFullscreenMode() = false

    override fun onStartInputView(info: EditorInfo, restarting: Boolean) {
        super.onStartInputView(info, restarting)
        typist.raw = isPassword(info)
        // No auto-correct where words are addresses, not language.
        typist.autoCorrect = !isAddress(info)
        typist.reset()
        searchQuery = null
        showKeys()
        keyboard?.apply {
            page = Page.LETTERS
            shift = Shift.OFF
            enterAction = enterActionOf(info)
            // The number row on Auto: shown in portrait, left out in landscape.
            numberRow = resources.configuration.orientation != Configuration.ORIENTATION_LANDSCAPE
            strip = Strip.Tools
        }
    }

    override fun onFinishInputView(finishingInput: Boolean) {
        typist.finish()
        super.onFinishInputView(finishingInput)
    }

    override fun onUpdateSelection(
        oldSelStart: Int, oldSelEnd: Int, newSelStart: Int, newSelEnd: Int,
        candidatesStart: Int, candidatesEnd: Int,
    ) {
        super.onUpdateSelection(oldSelStart, oldSelEnd, newSelStart, newSelEnd, candidatesStart, candidatesEnd)
        // The cursor left the word in progress (a tap elsewhere): the word ends where it is.
        val inWord = candidatesStart >= 0 && newSelStart == candidatesEnd && newSelEnd == candidatesEnd
        if (typist.shown.isNotEmpty() && !inWord) {
            typist.finish()
            showStrip()
        }
    }

    // ---- the keyboard's keys ------------------------------------------------------------------

    override fun onText(text: String) {
        val q = searchQuery
        if (q != null) {
            search(q + text)
            return
        }
        typist.text(text)
        showStrip()
    }

    override fun onBackspace() {
        val q = searchQuery
        if (q != null) {
            search(q.dropLast(1))
            return
        }
        typist.backspace()
        showStrip()
    }

    override fun onEnter() {
        if (searchQuery != null) {
            onSearchClose()
            return
        }
        typist.enter()
        showStrip()
    }

    override fun onCandidate(slot: Int) {
        val pick = strip.pick(slot) ?: return
        if (pick.emoji) {
            strip.centre?.let { if (typist.shown.isNotEmpty()) typist.pick(it) }
            typist.insert(pick.text)
            memory.addRecent(pick.text)
        } else {
            typist.pick(pick.text)
        }
        showStrip()
    }

    override fun onStripMenu() {
        keyboard?.strip = Strip.Tools
    }

    override fun onPickKeyboard() {
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).showInputMethodPicker()
    }

    override fun onTool(tool: Tool) {
        when (tool) {
            Tool.SETTINGS -> startActivity(
                Intent(this, SettingsActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            )
            Tool.THEME -> {
                val next = if (keyboard?.palette == Palette.LIGHT) Palette.DARK else Palette.LIGHT
                keyboard?.palette = next
                emojiPanel?.palette = next
                prefs().edit().putString("palette", if (next == Palette.LIGHT) "light" else "dark").apply()
            }
            Tool.EMOJI -> showEmoji()
            // The clipboard and the cursor pad come in later steps.
            Tool.CLIPBOARD, Tool.CURSOR -> Unit
        }
    }

    private fun showStrip() {
        keyboard?.strip = strip.make(typist)
    }

    // ---- the emoji panel ----------------------------------------------------------------------

    private fun showKeys() {
        emojiPanel?.visibility = View.GONE
        keyboard?.visibility = View.VISIBLE
    }

    private fun showEmoji() {
        val panel = emojiPanel ?: return
        val index = emoji
        if (panel.emoji.isEmpty()) panel.emoji = index?.all.orEmpty()
        panel.recents = memory.recents()
        panel.tones = memory.tones()
        panel.suggestions = index?.suggest(typist.context().filter { it != "<s>" } + listOfNotNull(strip.centre)).orEmpty()
        // The keys stay laid out (invisible), so the panel keeps their height.
        keyboard?.visibility = View.INVISIBLE
        panel.visibility = View.VISIBLE
    }

    override fun onEmoji(text: String) {
        typist.insert(text)
        memory.addRecent(text)
        emojiPanel?.recents = memory.recents()
    }

    override fun onSkinTone(base: String, toned: String) {
        memory.setTone(base, toned)
        emojiPanel?.tones = memory.tones()
    }

    override fun onClosePanel() {
        showKeys()
        showStrip()
    }

    override fun onEmojiSearch() {
        showKeys()
        search("")
    }

    private fun search(query: String) {
        searchQuery = query
        val found = if (query.isBlank()) memory.recents() else emoji?.search(query).orEmpty()
        keyboard?.strip = Strip.Search(query, found)
    }

    override fun onSearchPick(emoji: String) {
        onEmoji(emoji)
    }

    override fun onSearchClose() {
        searchQuery = null
        showEmoji()
    }

    // ---- settings -----------------------------------------------------------------------------

    /** Device-protected storage: the keyboard runs before the phone's first unlock. */
    private fun prefs() = createDeviceProtectedStorageContext().getSharedPreferences("keyboard", MODE_PRIVATE)

    private val memory by lazy { EmojiMemory(prefs()) }

    private fun savedPalette() = if (prefs().getString("palette", "dark") == "light") Palette.LIGHT else Palette.DARK

    companion object {
        fun isPassword(info: EditorInfo): Boolean {
            val cls = info.inputType and InputType.TYPE_MASK_CLASS
            val variation = info.inputType and InputType.TYPE_MASK_VARIATION
            return when (cls) {
                InputType.TYPE_CLASS_TEXT -> variation == InputType.TYPE_TEXT_VARIATION_PASSWORD ||
                    variation == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD ||
                    variation == InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD
                InputType.TYPE_CLASS_NUMBER -> variation == InputType.TYPE_NUMBER_VARIATION_PASSWORD
                else -> false
            }
        }

        fun isAddress(info: EditorInfo): Boolean {
            if (info.inputType and InputType.TYPE_MASK_CLASS != InputType.TYPE_CLASS_TEXT) return false
            val variation = info.inputType and InputType.TYPE_MASK_VARIATION
            return variation == InputType.TYPE_TEXT_VARIATION_URI ||
                variation == InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS ||
                variation == InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS
        }

        fun enterActionOf(info: EditorInfo): EnterAction {
            if (info.imeOptions and EditorInfo.IME_FLAG_NO_ENTER_ACTION != 0) return EnterAction.NEWLINE
            return when (info.imeOptions and EditorInfo.IME_MASK_ACTION) {
                EditorInfo.IME_ACTION_SEND -> EnterAction.SEND
                EditorInfo.IME_ACTION_SEARCH -> EnterAction.SEARCH
                EditorInfo.IME_ACTION_GO -> EnterAction.GO
                EditorInfo.IME_ACTION_NEXT -> EnterAction.NEXT
                EditorInfo.IME_ACTION_DONE -> EnterAction.DONE
                else -> EnterAction.NEWLINE
            }
        }
    }
}

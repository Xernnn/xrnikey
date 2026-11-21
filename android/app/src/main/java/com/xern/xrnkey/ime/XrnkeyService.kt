package com.xern.xrnkey.ime

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.inputmethodservice.InputMethodService
import android.text.InputType
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import com.xern.xrnkey.SettingsActivity
import com.xern.xrnkey.engine.Model
import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.engine.TypingState
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
 */
class XrnkeyService : InputMethodService(), KeyboardView.Listener {
    private var vi: Model? = null
    private var en: Model? = null
    private var models: Models? = null
    private var state: TypingState? = null
    private lateinit var typist: Typist
    private lateinit var strip: StripMaker
    private var keyboard: KeyboardView? = null

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
        state = TypingState().also { it.useModels(models) }
        typist = Typist(state, editor)
        strip = StripMaker(models)
    }

    private fun openModel(name: String): Model? = try {
        assets.openFd(name).use { Model.open(it.parcelFileDescriptor.fd, it.startOffset, it.length) }
    } catch (e: IOException) {
        null
    }

    override fun onDestroy() {
        state?.close()
        models?.close()
        vi?.close()
        en?.close()
        super.onDestroy()
    }

    override fun onCreateInputView(): View = KeyboardView(this).also {
        it.listener = this
        it.palette = savedPalette()
        keyboard = it
    }

    /** Never the full-screen editor in landscape. */
    override fun onEvaluateFullscreenMode() = false

    override fun onStartInputView(info: EditorInfo, restarting: Boolean) {
        super.onStartInputView(info, restarting)
        typist.raw = isPassword(info)
        typist.reset()
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
        typist.text(text)
        showStrip()
    }

    override fun onBackspace() {
        typist.backspace()
        showStrip()
    }

    override fun onEnter() {
        typist.enter()
        showStrip()
    }

    override fun onCandidate(slot: Int) {
        val pick = strip.pick(slot) ?: return
        typist.pick(pick)
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
                prefs().edit().putString("palette", if (next == Palette.LIGHT) "light" else "dark").apply()
            }
            // The clipboard, cursor pad and emoji panels come in later steps.
            Tool.CLIPBOARD, Tool.CURSOR, Tool.EMOJI -> Unit
        }
    }

    private fun showStrip() {
        keyboard?.strip = strip.make(typist)
    }

    // ---- settings -----------------------------------------------------------------------------

    /** Device-protected storage: the keyboard runs before the phone's first unlock. */
    private fun prefs() = createDeviceProtectedStorageContext().getSharedPreferences("keyboard", MODE_PRIVATE)

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

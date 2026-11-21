package com.xern.xrnkey.ime

import com.xern.xrnkey.engine.KeyResult
import com.xern.xrnkey.engine.TypingState

/**
 * What the keyboard does to the text field, as InputConnection does it: the word in progress
 * is composing text, a commit replaces it.
 */
interface Editor {
    /** Show `text` as the word in progress ("" clears it). */
    fun setComposing(text: String)

    /** Put `text` in the field, in place of the word in progress if there is one. */
    fun commit(text: String)

    /** Keep the word in progress as it stands, ending it. */
    fun finishComposing()

    /** Delete before the cursor, or the selection (the Backspace key itself). */
    fun backspace()

    /** The field's Enter: a newline, or its action (send, search ...). */
    fun enter()

    /** Up to `n` characters before the cursor ("" when the field doesn't say). */
    fun textBefore(n: Int): String
}

/**
 * Turns the keyboard's keys into edits through the shared core: Telex (or VNI) composes the
 * word in progress, and a key that ends the word commits it. In password fields every key goes
 * in as typed. No Android types here, so the tests drive it with a fake editor.
 */
class Typist(private val state: TypingState?, private val editor: Editor) {
    /** Password and similar fields: no Telex, no composing. */
    var raw = false

    private var composing = ""

    /** What was committed lately, for the words before the next one. */
    private val tail = StringBuilder()

    /** Whether [tail] reaches back to the field's start (nothing before it was cut off). */
    private var wholeField = true

    /** The word in progress, as shown. */
    val shown: String get() = composing

    /** The keys of the word in progress, as typed ("dduowcj" while it shows "được"). */
    fun keys(): String = state?.keys() ?: composing

    /**
     * The words before the word in progress, oldest first, as accent restoration takes them:
     * "<s>" when they start a sentence, at most the last three.
     */
    fun context(): List<String> {
        val text = tail.toString()
        val cut = text.indexOfLast { it == '.' || it == '!' || it == '?' || it == '\n' }
        val words = WORD.findAll(text.substring(cut + 1)).map { it.value }.toList()
        // The sentence's start is known: a full stop or new line, or the field's own start.
        val start = (cut >= 0 || wholeField) && words.size <= 3
        return (if (start) listOf("<s>") else emptyList()) + words.takeLast(3)
    }

    /** Typed text: each character is a key. */
    fun text(t: String) {
        for (c in t) key(c)
    }

    private fun key(c: Char) {
        val s = state
        if (raw || s == null) {
            commit(c.toString())
            return
        }
        apply(s.key(c.code)) { commit(c.toString()) }
    }

    fun backspace() {
        val s = state
        if (raw || s == null) {
            editor.backspace()
            return
        }
        apply(s.key(TypingState.BACKSPACE)) {
            editor.backspace()
            if (tail.isNotEmpty()) tail.setLength(tail.length - 1)
        }
    }

    /** Enter ends the word, then reaches the field: one press does both. */
    fun enter() {
        val s = state
        if (!raw && s != null) apply(s.key(TypingState.RETURN))
        editor.enter()
        tail.append('\n')
    }

    /**
     * The cursor moved, or the field is being left: end the word as a boundary would (the core
     * may restore it, "tém" to "terms") and keep it in the field.
     */
    fun finish() {
        val s = state ?: return
        val r = s.flush()
        when {
            r.commit.isNotEmpty() && r.commit != composing -> commit(r.commit)
            composing.isNotEmpty() -> {
                editor.finishComposing()
                tail.append(composing)
            }
        }
        composing = ""
        s.reset()
    }

    /** A new field: nothing is in progress; what is before the cursor is the field's text. */
    fun reset() {
        state?.reset()
        composing = ""
        tail.clear()
        val before = editor.textBefore(TAIL)
        tail.append(before)
        wholeField = before.length < TAIL
        state?.setContext(context())
    }

    /** A candidate from the strip, in place of the word in progress, and a space. */
    fun pick(word: String) {
        commit("$word ")
        composing = ""
        state?.reset()
    }

    private fun commit(text: String) {
        editor.commit(text)
        tail.append(text)
        if (tail.length > TAIL) {
            tail.delete(0, tail.length - TAIL)
            wholeField = false
        }
    }

    private inline fun apply(r: KeyResult, passThrough: () -> Unit = {}) {
        if (r.commit.isNotEmpty()) {
            commit(r.commit)
            composing = ""
        }
        if (r.preedit != composing) {
            editor.setComposing(r.preedit)
            composing = r.preedit
        }
        if (!r.consumed) passThrough()
    }

    private companion object {
        /** How much committed text is kept: enough for three words and where the sentence began. */
        const val TAIL = 120
        val WORD = Regex("[\\p{L}\\p{N}]+")
    }
}

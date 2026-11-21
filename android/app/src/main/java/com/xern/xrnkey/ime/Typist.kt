package com.xern.xrnkey.ime

import com.xern.xrnkey.engine.KeyResult
import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.engine.PersonalWords
import com.xern.xrnkey.engine.Shortcuts
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

    /** Delete `codePoints` characters right before the cursor (no word in progress). */
    fun deleteBefore(codePoints: Int)

    /** The field's Enter: a newline, or its action (send, search ...). */
    fun enter()

    /** Up to `n` characters before the cursor ("" when the field doesn't say). */
    fun textBefore(n: Int): String
}

/**
 * Turns the keyboard's keys into edits through the shared core.
 *
 * A word typed with Telex marks ("dduowcj") is composed by the core and committed as it ends.
 * A word typed without marks ("khong") is restored when Space or punctuation ends it
 * (android/docs/feature-spec.md, "Auto-correct"): its keys and those of the last two such words
 * are decoded together (look-back), so the earlier words may change too ("co gang" becomes
 * "cố gắng"), but only where the field still shows what this keyboard wrote. Backspace right
 * after puts back what was there. In password fields every key goes in as typed. No Android
 * types here, so the tests drive it with a fake editor.
 */
class Typist(
    private val state: TypingState?,
    private val editor: Editor,
    private val models: Models? = null,
) {
    /** Password and similar fields: no Telex, no composing. */
    var raw = false

    /** Restore words typed without marks when they end (off in URL and email fields). */
    var autoCorrect = true

    /** Two spaces after a word give ". " (only where auto-correct is on). */
    var doubleSpace = true

    /**
     * Where the words typed are learned (null: nowhere). Only settled words are: a word is
     * learned once look-back can no longer change it, with the word before it. Never in
     * password, address or incognito fields ([learnHere]), never from pasted text.
     */
    var personal: PersonalWords? = null

    /** This field allows learning (off for incognito fields). */
    var learnHere = true

    /** Abbreviations that expand when their word ends (null: none). */
    var shortcuts: Shortcuts? = null

    /** Committed text whose words are not learned yet, and the word before it. */
    private val pending = StringBuilder()
    private var pendingPrev: String? = "<s>"

    private var composing = ""

    /** What was committed lately, for the words before the next one. */
    private val tail = StringBuilder()

    /** Whether [tail] reaches back to the field's start (nothing before it was cut off). */
    private var wholeField = true

    /** A word restoration may still change: its keys, what the field shows, what followed. */
    private data class RunWord(val typed: String, val shown: String, val sep: String)

    /** The words restored since the last pause, oldest first; look-back may rewrite them. */
    private val run = ArrayList<RunWord>()

    /** What the last restoration changed, for Backspace right after it. */
    private data class Undo(val before: String, val after: String)
    private var undo: Undo? = null

    /** The last key was a Space that ended a word (for the double-Space full stop). */
    private var spaceAfterWord = false

    /** The word in progress, as shown. */
    val shown: String get() = composing

    /** The keys of the word in progress, as typed ("dduowcj" while it shows "được"). */
    fun keys(): String = state?.keys() ?: composing

    /** The word in progress was typed without marks, so restoration has it. */
    fun restorable(): Boolean =
        models != null && autoCorrect && !raw && composing.isNotEmpty() && keys() == composing &&
            composing.all { it in 'a'..'z' || it in 'A'..'Z' }

    /**
     * The words before the word in progress, oldest first, as accent restoration takes them:
     * "<s>" when they start a sentence, at most the last three, leaving out the last `skip`.
     */
    fun context(skip: Int = 0): List<String> {
        val text = tail.toString()
        val cut = text.indexOfLast { it == '.' || it == '!' || it == '?' || it == '\n' }
        val words = WORD.findAll(text.substring(cut + 1)).map { it.value }.toList().dropLast(skip)
        // The sentence's start is known: a full stop or new line, or the field's own start.
        val start = (cut >= 0 || wholeField) && words.size <= 3
        return (if (start) listOf("<s>") else emptyList()) + words.takeLast(3)
    }

    /**
     * Right after a word and a space (or punctuation and a space), with no word in progress:
     * where the strip offers the next word.
     */
    fun afterWord(): Boolean {
        if (composing.isNotEmpty() || raw || !autoCorrect) return false
        val n = tail.length
        return n >= 2 && tail[n - 1] == ' ' && tail[n - 2] != ' ' && !tail[n - 2].isWhitespace()
    }

    /** The expansion of the word in progress, if it is an abbreviation. */
    fun expansion(): String? =
        if (composing.isEmpty() || raw || !autoCorrect) null else shortcuts?.expand(keys())

    /** What Space would commit for the word in progress: its expansion, or its restoration. */
    fun preview(): String? {
        expansion()?.let { return it }
        if (!restorable()) return null
        val prior = run.takeLast(2)
        return models!!.restoreRun(context(prior.size), prior.map { it.typed } + keys())?.last()
    }

    /** Typed text: each character is a key. */
    fun text(t: String) {
        val s = state
        // A letter's Vietnamese form (held on its key) goes in as the keys that make it, so the
        // word goes on taking marks: "vi" + "ê" + "tj" is "việt".
        val keys = if (s != null && !raw && t.length == 1) VietKeys.of(t[0], s.mode) ?: t else t
        for (c in keys) key(c)
    }

    private fun key(c: Char) {
        val wasSpaceAfterWord = spaceAfterWord
        spaceAfterWord = false
        undo = null
        val s = state
        if (raw || s == null) {
            commit(c.toString())
            return
        }
        if (c == ' ' && composing.isEmpty() && wasSpaceAfterWord && doubleSpace && autoCorrect) {
            replaceBefore(" ", ". ")
            run.clear()
            s.setContext(context())
            return
        }
        if (isSeparator(c) && expansion() != null) {
            val shown = composing
            val e = expansion()!!
            commit(e + c)
            composing = ""
            s.reset()
            run.clear()
            undo = Undo(shown + c, e + c)
            spaceAfterWord = c == ' '
            s.setContext(context())
            return
        }
        if (isSeparator(c) && restorable()) {
            endRestored(c)
            spaceAfterWord = c == ' '
            return
        }
        val hadWord = composing.isNotEmpty()
        val r = s.key(c.code)
        apply(r) { commit(c.toString()) }
        // A word the core finished (typed with marks), or a key that is not a letter, ends the
        // run; the letters of the next word do not.
        if (r.commit.isNotEmpty() || !r.consumed || !c.isLetter()) run.clear()
        spaceAfterWord = c == ' ' && hadWord
    }

    /** Space or punctuation ends a word typed without marks: restore it, with look-back. */
    private fun endRestored(c: Char) {
        val typed = keys()
        val prior = run.takeLast(2)
        val next = when (c) {
            ',', ';', ':' -> "<p>"
            '.', '!', '?' -> "</s>"
            else -> null
        }
        val result = models!!.restoreRun(context(prior.size), prior.map { it.typed } + typed, next)
        val word = result?.last() ?: typed
        val sep = c.toString()
        // The word in progress first, as restored.
        commit(word + sep)
        composing = ""
        state?.reset()
        // Then the earlier words of the run, where look-back changed them.
        val oldPrior = prior.joinToString("") { it.shown + it.sep }
        val newPrior = prior.mapIndexed { i, w -> (result?.get(i) ?: w.shown) + w.sep }.joinToString("")
        var priorNow = oldPrior
        if (result != null && newPrior != oldPrior && replaceBefore(oldPrior + word + sep, newPrior + word + sep)) {
            priorNow = newPrior
            prior.forEachIndexed { i, w -> run[run.size - prior.size + i] = w.copy(shown = result[i]) }
        }
        val before = oldPrior + typed + sep
        val after = priorNow + word + sep
        undo = if (before != after) Undo(before, after) else null
        run += RunWord(typed, word, sep)
        while (run.size > 2) run.removeAt(0)
        if (c != ' ') run.clear()  // a pause or a sentence end: what came before stays
        state?.setContext(context())
    }

    fun backspace() {
        spaceAfterWord = false
        val u = undo
        undo = null
        val s = state
        if (raw || s == null) {
            editor.backspace()
            return
        }
        // Right after a restoration: put back what was there.
        if (u != null && composing.isEmpty() && replaceBefore(u.after, u.before)) {
            run.clear()
            s.setContext(context())
            return
        }
        apply(s.key(TypingState.BACKSPACE)) {
            editor.backspace()
            if (tail.isNotEmpty()) tail.setLength(tail.length - 1)
            if (pending.isNotEmpty()) pending.setLength(pending.length - 1)
            run.clear()
        }
    }

    /** Enter ends the word, then reaches the field: one press does both. */
    fun enter() {
        spaceAfterWord = false
        undo = null
        val s = state
        if (!raw && s != null) {
            if (restorable()) {
                val r = models!!.restoreRun(context(), listOf(keys()), "</s>")?.last() ?: composing
                commit(r)
                composing = ""
                s.reset()
            } else {
                apply(s.key(TypingState.RETURN))
            }
        }
        run.clear()
        editor.enter()
        tail.append('\n')
        pending.append('\n')
        learnSettled(all = true)
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
                pending.append(composing)
            }
        }
        composing = ""
        s.reset()
        run.clear()
        undo = null
        spaceAfterWord = false
        learnSettled(all = true)
    }

    /** A new field: nothing is in progress; what is before the cursor is the field's text. */
    fun reset() {
        learnSettled(all = true)
        state?.reset()
        composing = ""
        run.clear()
        undo = null
        spaceAfterWord = false
        tail.clear()
        val before = editor.textBefore(TAIL)
        tail.append(before)
        wholeField = before.length < TAIL
        pending.clear()
        pendingPrev = context().lastOrNull()
        state?.setContext(context())
    }

    /** Text from a panel (an emoji, a clip): the word in progress ends as it is, then the text. */
    fun insert(text: String) {
        if (composing.isNotEmpty()) finish()
        undo = null
        spaceAfterWord = false
        run.clear()
        learnSettled(all = true)
        commit(text, learnable = false)
        pendingPrev = "<p>"
        state?.setContext(context())
    }

    /** A candidate from the strip, in place of the word in progress, and a space. */
    fun pick(word: String) {
        commit("$word ")
        composing = ""
        state?.reset()
        run.clear()
        undo = null
        spaceAfterWord = true
        state?.setContext(context())
    }

    private fun commit(text: String, learnable: Boolean = true) {
        editor.commit(text)
        tail.append(text)
        if (tail.length > TAIL) {
            tail.delete(0, tail.length - TAIL)
            wholeField = false
        }
        if (learnable) {
            pending.append(text)
            learnSettled(all = false)
        }
    }

    /**
     * Learns the words of [pending] that look-back and Backspace can no longer change (all of
     * them at a new line, or when the field is left or the cursor moves), each with the word before
     * it: "<s>" at a sentence's start, "<p>" after other punctuation.
     */
    private fun learnSettled(all: Boolean) {
        val p = personal
        if (p == null || raw || !autoCorrect || !learnHere) {
            pending.clear()
            return
        }
        val text = pending.toString()
        val words = WORD.findAll(text).toList()
        val n = words.size - if (all) 0 else SETTLE
        if (n <= 0) return
        var prev = pendingPrev
        var at = 0
        for (i in 0 until n) {
            val m = words[i]
            val gap = text.substring(at, m.range.first)
            prev = when {
                gap.any { it in SENTENCE_END } -> "<s>"
                gap.any { it != ' ' } -> "<p>"
                else -> prev
            }
            p.learn(prev, m.value)
            prev = m.value
            at = m.range.last + 1
        }
        pending.delete(0, at)
        pendingPrev = prev
    }

    /**
     * Replace `old`, right before the cursor, with `new`, if the field still shows exactly
     * `old` there; otherwise leave the field alone. True when it was replaced.
     */
    private fun replaceBefore(old: String, new: String): Boolean {
        if (editor.textBefore(old.length) != old) return false
        editor.deleteBefore(old.codePointCount(0, old.length))
        if (tail.endsWith(old)) tail.setLength(tail.length - old.length)
        if (pending.endsWith(old)) pending.setLength(pending.length - old.length)
        commit(new)
        return true
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

        fun isSeparator(c: Char) = c == ' ' || c in ",.;:!?"

        const val SENTENCE_END = ".!?\n"

        /** Words kept back from learning: the look-back window (two) and the newest. */
        const val SETTLE = 3
    }
}

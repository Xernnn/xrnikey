package com.xern.xrnkey.ime

import com.xern.xrnkey.engine.EmojiIndex
import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.keyboard.Strip

/**
 * What the strip shows for the word in progress. For a word typed without marks ("duoc"), the
 * centre is exactly what Space would commit (with look-back over the words before it), and the
 * sides are its next readings. A word typed with Telex marks shows as it is. When an emoji goes
 * with the words ("sinh nhật" -> 🎂), it takes the right-hand slot. With no word, the tool row.
 */
class StripMaker(private val models: Models?, private val emoji: EmojiIndex? = null) {
    /** What a slot holds: a word, or an emoji to put after the word in progress. */
    data class Pick(val text: String, val emoji: Boolean = false)

    private var offered: List<Pick?> = emptyList()

    /** The word Space would commit, while one is in progress (for an emoji pick). */
    var centre: String? = null
        private set

    var emojiSuggestions = true

    fun make(t: Typist): Strip {
        val word = t.shown
        if (word.isEmpty()) {
            offered = emptyList()
            centre = null
            return Strip.Tools
        }
        val restored = t.preview()
        val c = restored ?: word
        centre = c
        val others = if (models != null && restored != null) {
            models.restoreWord(t.context(), t.keys(), 6).map { it.text }.filter { it != c }
        } else {
            emptyList()
        }
        val face = emojiFor(t, c)
        val left = others.getOrNull(0)?.let { Pick(it) }
        val right = face?.let { Pick(it, emoji = true) } ?: others.getOrNull(1)?.let { Pick(it) }
        offered = listOf(left, Pick(c), right)
        return Strip.Candidates(left?.text, c, right?.text, more = others.size > 2)
    }

    private fun emojiFor(t: Typist, word: String): String? {
        val index = emoji ?: return null
        if (!emojiSuggestions) return null
        val words = t.context().filter { it != "<s>" } + word
        return index.suggest(words, 1).firstOrNull()
    }

    /** The candidate in slot 0 (left), 1 (centre) or 2 (right). */
    fun pick(slot: Int): Pick? = offered.getOrNull(slot)
}

package com.xern.xrnkey.ime

import com.xern.xrnkey.engine.Models
import com.xern.xrnkey.keyboard.Strip

/**
 * What the strip shows for the word in progress. A word typed without marks ("duoc") gets its
 * accented readings in context: the best in the centre, the next two beside it. A word typed
 * with Telex marks shows as it is. With no word, the tool row.
 */
class StripMaker(private val models: Models?) {
    private var offered: List<String?> = emptyList()

    fun make(t: Typist): Strip {
        val word = t.shown
        if (word.isEmpty()) {
            offered = emptyList()
            return Strip.Tools
        }
        val keys = t.keys()
        val plain = keys == word && keys.all { it in 'a'..'z' || it in 'A'..'Z' }
        if (models == null || !plain) {
            offered = listOf(null, word, null)
            return Strip.Candidates(null, word, null)
        }
        val c = models.restoreWord(t.context(), keys, 6).map { it.text }
        offered = listOf(c.getOrNull(1), c.getOrNull(0), c.getOrNull(2))
        return Strip.Candidates(offered[0], offered[1], offered[2], more = c.size > 3)
    }

    /** The candidate in slot 0 (left), 1 (centre) or 2 (right). */
    fun pick(slot: Int): String? = offered.getOrNull(slot)
}

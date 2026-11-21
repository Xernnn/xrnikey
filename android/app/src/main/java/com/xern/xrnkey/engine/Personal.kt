package com.xern.xrnkey.engine

import java.io.File

/**
 * The owner's personal words (core/learn.cpp): counts of words and word pairs, never sentences,
 * learned and kept on the phone only. A word or pair counts once typed twice.
 */
class PersonalWords : AutoCloseable {
    internal val handle = Native.userNew()

    /** Replaces what is held with the file's; the entries read, or -1. */
    fun load(file: File): Int {
        unsaved = 0
        return Native.userLoad(handle, file.path.toByteArray())
    }

    /** Saves atomically; false when it couldn't (the old file stays). */
    fun save(file: File): Boolean {
        val ok = Native.userSave(handle, file.path.toByteArray()) == 0
        if (ok) unsaved = 0
        return ok
    }

    /** `word` was committed after `prev` ("<s>" at a sentence's start, "<p>" after a comma). */
    fun learn(prev: String?, word: String) {
        Native.userLearn(handle, prev?.toByteArray(Charsets.UTF_8), word.toByteArray(Charsets.UTF_8))
        unsaved++
    }

    /** Words learned since the last load or save. */
    var unsaved = 0
        private set

    fun count(prev: String?, word: String): Double =
        Native.userCount(handle, prev?.toByteArray(Charsets.UTF_8), word.toByteArray(Charsets.UTF_8))

    fun forget(word: String) = Native.userForget(handle, word.toByteArray(Charsets.UTF_8))

    /** Old habits fade: every count times `factor`, dropping what falls below a half. */
    fun decay(factor: Double) = Native.userDecay(handle, factor)

    override fun close() = Native.userFree(handle)
}

/** What an import would do, or did: entries added, already there, replaced, lines skipped. */
data class ImportCounts(val found: Int, val added: Int, val same: Int, val changed: Int, val skipped: Int)

/**
 * Shortcuts (core/shortcuts.cpp): an abbreviation that expands when its word ends, "ko" ->
 * "không", taking the typed case ("Ko" -> "Không").
 */
class Shortcuts : AutoCloseable {
    private val handle = Native.shortcutsNew()

    fun load(file: File): Int = Native.shortcutsLoad(handle, file.path.toByteArray())

    fun save(file: File): Boolean = Native.shortcutsSave(handle, file.path.toByteArray()) == 0

    /**
     * Reads a table made elsewhere ("abbr<TAB>expansion", "abbr,expansion" or
     * "abbr=expansion" per line) and says what it would do; does it only when `apply`.
     * Null when the text isn't a table at all.
     */
    fun import(text: String, apply: Boolean): ImportCounts? {
        val c = IntArray(4)
        val n = Native.shortcutsImport(handle, text.toByteArray(Charsets.UTF_8), apply, c)
        return if (n < 0) null else ImportCounts(n, c[0], c[1], c[2], c[3])
    }

    fun set(abbr: String, expansion: String): Boolean =
        Native.shortcutsSet(handle, abbr.toByteArray(Charsets.UTF_8), expansion.toByteArray(Charsets.UTF_8))

    fun remove(abbr: String) = Native.shortcutsRemove(handle, abbr.toByteArray(Charsets.UTF_8))

    /** Every entry, in order of abbreviation. */
    fun all(): List<Pair<String, String>> =
        Native.shortcutsAll(handle).map { String(it, Charsets.UTF_8) }.chunked(2).map { it[0] to it[1] }

    fun expand(typed: String): String? =
        Native.shortcutsExpand(handle, typed.toByteArray(Charsets.UTF_8))?.let { String(it, Charsets.UTF_8) }

    override fun close() = Native.shortcutsFree(handle)
}

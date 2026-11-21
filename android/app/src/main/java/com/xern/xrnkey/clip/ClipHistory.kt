package com.xern.xrnkey.clip

import java.io.File

/** One clip: its text, when it was copied (ms), and whether it is pinned. */
data class Clip(val text: String, val time: Long, val pinned: Boolean = false)

/**
 * The clipboard history, kept only on the phone: in a file in the keyboard's own
 * credential-encrypted storage, so it can't be read before the first unlock and never leaves
 * the phone. Newest first; pinned clips stay until deleted, the others go after [MAX] newer
 * ones or on [clear].
 */
class ClipHistory(private val file: File?) {
    private val list = mutableListOf<Clip>()

    init {
        if (file != null && file.exists()) {
            file.readLines().mapNotNullTo(list) { decode(it) }
        }
    }

    val clips: List<Clip> get() = list.toList()

    /** A newly copied text: to the top (once), unless it is blank. */
    fun add(text: String, time: Long) {
        if (text.isBlank()) return
        val old = list.firstOrNull { it.text == text }
        list.removeAll { it.text == text }
        list.add(0, Clip(text, time, old?.pinned ?: false))
        var unpinned = 0
        list.removeAll { !it.pinned && ++unpinned > MAX }
        save()
    }

    fun pin(text: String, pinned: Boolean) {
        val i = list.indexOfFirst { it.text == text }
        if (i < 0) return
        list[i] = list[i].copy(pinned = pinned)
        save()
    }

    fun delete(text: String) {
        if (list.removeAll { it.text == text }) save()
    }

    /** Clears every clip that isn't pinned. */
    fun clear() {
        if (list.removeAll { !it.pinned }) save()
    }

    private fun save() {
        val f = file ?: return
        val tmp = File(f.path + ".tmp")
        tmp.writeText(list.joinToString("") { encode(it) + "\n" })
        tmp.renameTo(f)
    }

    companion object {
        const val MAX = 50

        private fun encode(c: Clip) = "${if (c.pinned) 1 else 0}\t${c.time}\t" +
            c.text.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")

        private fun decode(line: String): Clip? {
            val p = line.split('\t', limit = 3)
            if (p.size != 3) return null
            val time = p[1].toLongOrNull() ?: return null
            val text = StringBuilder()
            var i = 0
            val s = p[2]
            while (i < s.length) {
                val c = s[i]
                if (c == '\\' && i + 1 < s.length) {
                    text.append(
                        when (s[i + 1]) {
                            'n' -> '\n'
                            'r' -> '\r'
                            't' -> '\t'
                            else -> s[i + 1]
                        },
                    )
                    i += 2
                } else {
                    text.append(c)
                    i++
                }
            }
            return Clip(text.toString(), time, p[0] == "1")
        }

        /**
         * Whether a copied text may go into the history: never one the app marked sensitive
         * (ClipDescription.EXTRA_IS_SENSITIVE, which password managers and banking apps set),
         * never one copied while a password field had the keyboard, never one that isn't text.
         */
        fun keep(text: CharSequence?, sensitive: Boolean, fromPasswordField: Boolean): Boolean =
            !text.isNullOrBlank() && !sensitive && !fromPasswordField
    }
}

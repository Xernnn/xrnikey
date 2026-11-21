package com.xern.xrnkey.engine

/*
 * The keyboard's view of the shared core. No Android types here, so the JVM tests run it
 * against the engine built for the laptop (android/host).
 */

private fun String.utf8() = toByteArray(Charsets.UTF_8)
private fun ByteArray.text() = String(this, Charsets.UTF_8)
private fun List<String>.utf8() = Array(size) { this[it].utf8() }

/** A language model (.xklm), memory-mapped. */
class Model private constructor(internal val handle: Long) : AutoCloseable {
    override fun close() = Native.lmFree(handle)

    companion object {
        fun open(path: String): Model? = Native.lmOpenPath(path.utf8()).takeIf { it != 0L }?.let(::Model)

        /** A model inside a larger file, as an uncompressed asset is inside the APK. */
        fun open(fd: Int, offset: Long, length: Long): Model? =
            Native.lmOpenFd(fd, offset, length).takeIf { it != 0L }?.let(::Model)
    }
}

/** The models that score words in context; either may be missing. Doesn't own them. */
class Models(vi: Model?, en: Model?) : AutoCloseable {
    internal val handle = Native.modelsNew(vi?.handle ?: 0L, en?.handle ?: 0L)

    /** XK_CORRECT_OFF, _FLEXIBLE or _STRONG. */
    var correction: Int = CORRECT_OFF
        set(value) {
            field = value
            Native.modelsSetCorrection(handle, value)
        }

    /** The best reading of a run of words typed without marks, decoded together. */
    fun restoreRun(context: List<String>, typed: List<String>, next: String? = null): List<String>? =
        Native.restoreRun(handle, context.utf8(), typed.utf8(), next?.utf8())?.map { it.text() }

    /** What one word typed without marks may stand for, best first. */
    fun restoreWord(context: List<String>, typed: String, max: Int = 8): List<Candidate> {
        val types = IntArray(max)
        val texts = Native.restoreWord(handle, context.utf8(), typed.utf8(), max, types)
        return texts.mapIndexed { i, t -> Candidate(t.text(), types[i]) }
    }

    /** The owner's personal words, which scoring leans towards (null: none). Not owned. */
    var user: PersonalWords? = null
        set(value) {
            field = value
            Native.modelsSetUser(handle, value?.handle ?: 0L)
        }

    override fun close() = Native.modelsFree(handle)

    companion object {
        const val CORRECT_OFF = 0
        const val CORRECT_FLEXIBLE = 1
        const val CORRECT_STRONG = 2
    }
}

/** XKCandidate.type: restored, as typed, or corrected. */
data class Candidate(val text: String, val type: Int) {
    companion object {
        const val CORRECTED = 1
        const val RESTORED = 3
        const val AS_TYPED = 4
    }
}

/** What a key did: text to commit, the word in progress, whether the core took the key. */
data class KeyResult(val commit: String, val preedit: String, val consumed: Boolean, val deleteBefore: Int)

/** One field's Telex (or VNI) state in the core. */
class TypingState(val mode: Int = TELEX) : AutoCloseable {
    private val handle = Native.stateNew(mode)
    private val out = IntArray(2)

    fun useModels(models: Models?) = Native.stateSetModels(handle, models?.handle ?: 0L)

    fun setOption(option: Int, value: Boolean) = Native.stateSetOption(handle, option, if (value) 1 else 0)

    /** A key: a character (its code), or one of the keysyms below. */
    fun key(keysym: Int): KeyResult = result(Native.stateKey(handle, keysym, out))

    /** End the word in progress as a boundary would (cursor moved, field left). */
    fun flush(): KeyResult = result(Native.stateFlush(handle, out))

    /** The keys of the word in progress, as typed. */
    fun keys(): String = Native.stateKeys(handle).text()

    fun setContext(words: List<String>) = Native.stateSetContext(handle, words.utf8())

    fun reset() = Native.stateReset(handle)

    private fun result(r: Array<ByteArray>) = KeyResult(r[0].text(), r[1].text(), out[0] != 0, out[1])

    override fun close() = Native.stateFree(handle)

    companion object {
        const val TELEX = 0
        const val VNI = 1
        const val BACKSPACE = 0xFF08
        const val RETURN = 0xFF0D
        const val OPT_AUTO_RESTORE = 1
        const val OPT_MODERN_STYLE = 2
        const val OPT_LEARN = 5
    }
}

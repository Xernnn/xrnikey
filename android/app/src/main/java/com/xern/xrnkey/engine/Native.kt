package com.xern.xrnkey.engine

/**
 * The shared core (core/xrnkey.h) through the JNI bridge (src/main/cpp/jni.cpp). Text crosses
 * as UTF-8 bytes; handles are the core's pointers. Use [Engine]'s classes, not this.
 */
internal object Native {
    init {
        System.loadLibrary("xrnkey_jni")
    }

    external fun lmOpenPath(path: ByteArray): Long
    external fun lmOpenFd(fd: Int, offset: Long, length: Long): Long
    external fun lmFree(lm: Long)

    external fun modelsNew(vi: Long, en: Long): Long
    external fun modelsFree(models: Long)
    external fun modelsSetCorrection(models: Long, level: Int)

    external fun stateNew(mode: Int): Long
    external fun stateFree(state: Long)
    external fun stateSetModels(state: Long, models: Long)
    external fun stateSetOption(state: Long, option: Int, value: Int)
    external fun stateReset(state: Long)

    /** [commit, preedit]; out[0] = consumed, out[1] = delete_before. */
    external fun stateKey(state: Long, keysym: Int, out: IntArray): Array<ByteArray>
    external fun stateFlush(state: Long, out: IntArray): Array<ByteArray>
    external fun stateKeys(state: Long): ByteArray
    external fun stateSetContext(state: Long, words: Array<ByteArray>)

    external fun restoreRun(models: Long, context: Array<ByteArray>, typed: Array<ByteArray>, next: ByteArray?): Array<ByteArray>?
    external fun restoreWord(models: Long, context: Array<ByteArray>, typed: ByteArray, max: Int, types: IntArray): Array<ByteArray>

    external fun emojiLoad(text: ByteArray): Long
    external fun emojiFree(db: Long)
    external fun emojiCount(db: Long): Int
    external fun emojiAt(db: Long, index: Int, skin: IntArray): Array<ByteArray>?
    external fun emojiSearch(db: Long, query: ByteArray, max: Int): Array<ByteArray>
    external fun emojiSuggest(db: Long, words: Array<ByteArray>, max: Int): Array<ByteArray>

    external fun userNew(): Long
    external fun userFree(user: Long)
    external fun userLoad(user: Long, path: ByteArray): Int
    external fun userSave(user: Long, path: ByteArray): Int
    external fun userLearn(user: Long, prev: ByteArray?, word: ByteArray)
    external fun userCount(user: Long, prev: ByteArray?, word: ByteArray): Double
    external fun userForget(user: Long, word: ByteArray)
    external fun userDecay(user: Long, factor: Double)
    external fun modelsSetUser(models: Long, user: Long)
    external fun shortcutsNew(): Long
    external fun shortcutsFree(sc: Long)
    external fun shortcutsLoad(sc: Long, path: ByteArray): Int
    external fun shortcutsSave(sc: Long, path: ByteArray): Int
    external fun shortcutsImport(sc: Long, text: ByteArray, apply: Boolean, counts: IntArray): Int
    external fun shortcutsSet(sc: Long, abbr: ByteArray, expansion: ByteArray): Boolean
    external fun shortcutsRemove(sc: Long, abbr: ByteArray)
    external fun shortcutsAll(sc: Long): Array<ByteArray>
    external fun shortcutsExpand(sc: Long, typed: ByteArray): ByteArray?
    external fun predict(models: Long, context: Array<ByteArray>, max: Int): Array<ByteArray>
}

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
}

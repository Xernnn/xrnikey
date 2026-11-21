package com.xern.xrnkey

import com.xern.xrnkey.engine.Model
import com.xern.xrnkey.engine.Models
import java.io.File

/**
 * The models the tests use: the small ones trained from made-up text (android/host builds
 * them), or the real ones when this machine has them (data/lm/README.md).
 */
object TestModels {
    val testDir = File(System.getProperty("xrnkey.testModels") ?: "build/host/lm")
    val realDir = File(System.getProperty("xrnkey.realModels") ?: "")

    val realAvailable: Boolean get() = File(realDir, "vi-phone.xklm").isFile && File(realDir, "en.xklm").isFile

    class Opened(val vi: Model, val en: Model) : AutoCloseable {
        val models = Models(vi, en)
        override fun close() {
            models.close()
            vi.close()
            en.close()
        }
    }

    fun test(): Opened = Opened(
        Model.open(File(testDir, "vi.xklm").path)!!,
        Model.open(File(testDir, "en.xklm").path)!!,
    )

    fun real(): Opened = Opened(
        Model.open(File(realDir, "vi-phone.xklm").path)!!,
        Model.open(File(realDir, "en.xklm").path)!!,
    )

    /** The real models when present, else the test ones. */
    fun best(): Opened = if (realAvailable) real() else test()
}

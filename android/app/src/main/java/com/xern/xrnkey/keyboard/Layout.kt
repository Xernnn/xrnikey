package com.xern.xrnkey.keyboard

/*
 * The keys, as data: what each row holds, how wide each key is (in letter-key widths, ten to a
 * row) and what holding it gives. KeyboardView draws and hits these; the tests check them.
 * The layout follows the owner's reference keyboard (android/docs/feature-spec.md, "The
 * keyboard"): a number row, QWERTY, Shift and Backspace beside z..m, and a bottom row of ?123,
 * comma, space, full stop and a round Enter.
 */

enum class KeyKind { CHAR, SHIFT, BACKSPACE, SYMBOLS, MORE_SYMBOLS, LETTERS, SPACE, COMMA, PERIOD, ENTER }

data class Key(
    val kind: KeyKind,
    /** What the key shows (a letter in the case it types, a symbol, "?123"). */
    val label: String = "",
    /** Width in letter keys. */
    val width: Float = 1f,
    /** What a tap types, for CHAR, COMMA and PERIOD. */
    val text: String = label,
    /** What holding it types, if anything (top-row letters give their digit). */
    val hold: String? = null,
)

data class Row(val keys: List<Key>, /** Empty space at each end, in letter keys. */ val inset: Float = 0f) {
    val width: Float get() = keys.sumOf { it.width.toDouble() }.toFloat() + 2 * inset
}

enum class Page { LETTERS, SYMBOLS, MORE_SYMBOLS }

enum class Shift { OFF, ONCE, LOCKED }

object Layouts {
    private const val DIGITS = "1234567890"

    private fun chars(s: String, shift: Shift = Shift.OFF, holds: String? = null) =
        s.mapIndexed { i, c ->
            val label = if (shift == Shift.OFF) c.toString() else c.uppercase()
            Key(KeyKind.CHAR, label, hold = holds?.getOrNull(i)?.toString())
        }

    private fun symbols(s: String) = s.split(' ').map { Key(KeyKind.CHAR, it) }

    private fun bottom(switch: Key) = Row(
        listOf(
            switch,
            Key(KeyKind.COMMA, ",", text = ","),
            Key(KeyKind.SPACE, width = 5f, text = " "),
            Key(KeyKind.PERIOD, ".", text = "."),
            Key(KeyKind.ENTER, width = 1.5f),
        ),
    )

    /** The rows of a page, top to bottom. The number row is there when [numberRow]. */
    fun rows(page: Page, shift: Shift = Shift.OFF, numberRow: Boolean = true): List<Row> = when (page) {
        Page.LETTERS -> buildList {
            if (numberRow) add(Row(chars(DIGITS)))
            add(Row(chars("qwertyuiop", shift, holds = DIGITS)))
            add(Row(chars("asdfghjkl", shift), inset = 0.5f))
            add(
                Row(
                    listOf(Key(KeyKind.SHIFT, width = 1.5f)) + chars("zxcvbnm", shift) +
                        Key(KeyKind.BACKSPACE, width = 1.5f),
                ),
            )
            add(bottom(Key(KeyKind.SYMBOLS, "?123", width = 1.5f)))
        }
        Page.SYMBOLS -> listOf(
            Row(chars(DIGITS)),
            Row(symbols("@ # ₫ _ & - + ( ) /")),
            Row(
                listOf(Key(KeyKind.MORE_SYMBOLS, "=\\<", width = 1.5f)) + symbols("* \" ' : ; ! ?") +
                    Key(KeyKind.BACKSPACE, width = 1.5f),
            ),
            bottom(Key(KeyKind.LETTERS, "ABC", width = 1.5f)),
        )
        Page.MORE_SYMBOLS -> listOf(
            Row(symbols("~ ` | • √ π ÷ × ¶ ∆")),
            Row(symbols("£ € $ ¥ ^ ° = { } \\")),
            Row(
                listOf(Key(KeyKind.SYMBOLS, "?123", width = 1.5f)) + symbols("% © ® ™ ✓ [ ]") +
                    Key(KeyKind.BACKSPACE, width = 1.5f),
            ),
            bottom(Key(KeyKind.LETTERS, "ABC", width = 1.5f)),
        )
    }
}

/** The strip's other face: the tool row (no mic: the keyboard is fully offline). */
enum class Tool { THEME, SETTINGS, CLIPBOARD, CURSOR, EMOJI }

/** What the strip above the keys shows. */
sealed interface Strip {
    data object Tools : Strip

    /** Up to three candidates; [centre] is what Space would commit. */
    data class Candidates(val left: String?, val centre: String?, val right: String?, val more: Boolean = false) : Strip

    /** Emoji search: the query typed so far and the emoji it finds. */
    data class Search(val query: String, val results: List<String>) : Strip
}

/** What Enter does in this field, drawn as its icon. */
enum class EnterAction { NEWLINE, SEND, SEARCH, GO, NEXT, DONE }

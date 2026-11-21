package com.xern.xrnkey.keyboard

import com.xern.xrnkey.engine.Emoji

/** The emoji panel's tabs: no fancy text, no stickers (android/docs/feature-spec.md). */
enum class EmojiTab { EMOJI, EMOTICONS, SYMBOLS }

/** One cell of the grid: what it types, and whether holding it offers skin tones. */
data class Cell(val text: String, val skinTones: Boolean = false)

/** A row of the grid, and the section (category) it belongs to. */
data class GridRow(val cells: List<Cell>, val section: Int)

/**
 * What the emoji panel's grid holds, row by row: on the emoji tab, the recents first, then each
 * of Unicode's groups starting on a new row, in Unicode's order; on the other tabs, their
 * lists. Pure Kotlin, so the tests check it without a phone.
 */
object EmojiGrid {
    /** Unicode's groups, in order, as the category bar shows them. */
    val groups = listOf(
        "Smileys & Emotion", "People & Body", "Animals & Nature", "Food & Drink",
        "Travel & Places", "Activities", "Objects", "Symbols", "Flags",
    )

    /** The category bar's icon for each group. */
    val groupIcons = listOf("😀", "👋", "🐻", "🍔", "🚗", "⚽", "💡", "🔣", "🏁")

    /** How many cells a row holds on each tab. */
    fun columns(tab: EmojiTab) = if (tab == EmojiTab.EMOTICONS) 3 else 8

    /**
     * The rows of a tab. On the emoji tab section 0 is the recents (left out when there are
     * none) and section g + 1 is group g; the others have one section, 0.
     * `tones` gives the skin tone chosen for an emoji, shown in its place.
     */
    fun rows(
        tab: EmojiTab,
        emoji: List<Emoji>,
        recents: List<String>,
        tones: Map<String, String> = emptyMap(),
    ): List<GridRow> {
        val cols = columns(tab)
        fun chunk(cells: List<Cell>, section: Int) = cells.chunked(cols).map { GridRow(it, section) }
        return when (tab) {
            EmojiTab.EMOTICONS -> chunk(Emoticons.list.map { Cell(it) }, 0)
            EmojiTab.SYMBOLS -> chunk(Symbols.list.map { Cell(it) }, 0)
            EmojiTab.EMOJI -> buildList {
                addAll(chunk(recents.map { Cell(it) }, 0))
                val byGroup = emoji.groupBy { it.group }
                groups.forEachIndexed { g, name ->
                    val cells = byGroup[name].orEmpty().map { Cell(tones[it.text] ?: it.text, it.skinTones) }
                    addAll(chunk(cells, g + 1))
                }
            }
        }
    }

    /** The first row of a section, or -1. */
    fun firstRow(rows: List<GridRow>, section: Int) = rows.indexOfFirst { it.section == section }
}

/** Text emoticons and kaomoji, as typed in chat. Written for this project. */
object Emoticons {
    val list = listOf(
        ":)", ":))", "=))", ":D", ":v", ";)", ":(", ":((", ":'(", ":P", ":O", ":3",
        "<3", "^^", "^_^", "^.^", "-_-", "T_T", ">_<", "o.O", "xD", ":-)", ":-(", ";-)",
        "(^_^)", "(^o^)", "(T_T)", "(>_<)", "(=^･ω･^=)", "(・ω・)", "(｡•̀ᴗ-)✧", "(◕‿◕)",
        "(•‿•)", "(¬‿¬)", "(ಠ_ಠ)", "(╥﹏╥)", "(っ˘ω˘ς)", "ʕ•ᴥ•ʔ", "(づ｡◕‿‿◕｡)づ", "(ノ◕ヮ◕)ノ*:･ﾟ✧",
        "¯\\_(ツ)_/¯", "(╯°□°）╯︵ ┻━┻", "┬─┬ノ( º _ ºノ)", "(ง •̀_•́)ง", "(｡♥‿♥｡)", "(✿◠‿◠)",
        "(●'◡'●)", "(⌐■_■)", "( ˘︹˘ )", "(︶︹︺)", "( ´ ▽ ` )", "(〃▽〃)", "(￣▽￣)", "(*^▽^*)",
        "ヽ(°〇°)ﾉ", "(°ロ°)", "(；一_一)", "(-_-;)", "(^人^)", "( ˘ ³˘)♥",
    )
}

/** Symbols that the letter and symbol pages leave out, by kind. Written for this project. */
object Symbols {
    val list = listOf(
        // arrows
        "←", "→", "↑", "↓", "↔", "↕", "⇐", "⇒", "⇔", "↩", "↪", "➜",
        // maths
        "±", "×", "÷", "≈", "≠", "≤", "≥", "∞", "√", "∑", "∏", "∫", "∂", "∆", "π", "µ", "°", "‰",
        "¼", "½", "¾", "¹", "²", "³", "ⁿ",
        // money
        "₫", "$", "€", "£", "¥", "₩", "₹", "₽", "฿", "¢",
        // marks and punctuation
        "©", "®", "™", "§", "¶", "†", "‡", "•", "·", "…", "—", "–", "«", "»", "‹", "›", "“", "”",
        "‘", "’", "¿", "¡", "※", "#", "&", "@",
        // shapes and stars
        "★", "☆", "✦", "✧", "♥", "♡", "♦", "♣", "♠", "●", "○", "◆", "◇", "■", "□", "▲", "△",
        "▼", "▽", "◀", "▶", "✓", "✔", "✗", "✘", "☐", "☑", "☒",
        // weather, music, misc
        "☀", "☁", "☂", "☃", "☎", "✉", "✂", "✎", "♪", "♫", "☕", "⚡", "☯", "☮", "♻", "⚠",
    )
}

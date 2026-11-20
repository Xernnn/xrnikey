#include "xrnikey_internal.h"
#include <algorithm>
#include <cctype>

namespace xrnikey {

/* Telex: letters are letters, except that s f r x j set the tone, a e o
 * typed after themselves put a circumflex on them, dd is đ, and w puts the
 * horn or breve on the vowels. A key's effect is kept only while the word can
 * still become a Vietnamese syllable; otherwise the key is just its letter. */

static bool is_vowel(char b) {
    return b == 'a' || b == 'e' || b == 'i' || b == 'o' || b == 'u' || b == 'y';
}

static int tone_of_key(char lc) {
    switch (lc) {
        case 's': return XK_TONE_SAC;
        case 'f': return XK_TONE_HUYEN;
        case 'r': return XK_TONE_HOI;
        case 'x': return XK_TONE_NGA;
        case 'j': return XK_TONE_NANG;
        default: return -1;
    }
}

/* Telex's `w` is a SYLLABLE-level modifier, not a previous-character one:
 * Unikey accepts it anywhere after the nucleus, so "nguoiw" and "duongw" reach
 * ngươi / dương exactly as "nguwowi" does. It acts on the rightmost run of
 * vowels (skipping a coda after it). The choices, in order: the u+o pair
 * (horn on both: ươ), then each vowel that takes a w mark (u -> ư, o -> ơ,
 * a -> ă) from the last one back. The caller takes the first choice that
 * still spells Vietnamese, so "luu" + w is lưu (not luư) and "mua" + w is
 * mưa (not muă). The u of "qu" is part of the onset, so "quo" + w is quơ.
 * Each choice is a list of indexes into w.letters; none when there is no
 * vowel to mark (leading "w", "new"). */
static std::vector<std::vector<int>> w_choices(const Word& w) {
    const std::vector<Letter>& L = w.letters;
    const int onset_u = (L.size() > 1 && L[0].base == 'q' && L[1].base == 'u') ? 1 : -1;
    int last = -1, first = -1;
    for (int k = static_cast<int>(L.size()) - 1; k >= 0; --k) {
        if (is_vowel(L[static_cast<size_t>(k)].base) && k != onset_u) {
            if (last < 0) last = k;
            first = k;
        } else if (last >= 0) {
            break;
        }
    }
    if (last < 0) return {};

    /* Any mark on a, o, u can become the w mark: aaw -> ă, as in Bamboo. */
    auto takes_w = [](const Letter& l) {
        return l.base == 'u' || l.base == 'o' || l.base == 'a';
    };
    std::vector<std::vector<int>> out;
    for (int k = first; k < last; ++k) {
        const Letter& u = L[static_cast<size_t>(k)];
        const Letter& o = L[static_cast<size_t>(k) + 1];
        if (u.base == 'u' && o.base == 'o' && takes_w(u) && takes_w(o)) out.push_back({k, k + 1});
    }
    for (int k = last; k >= first; --k)
        if (takes_w(L[static_cast<size_t>(k)])) out.push_back({k});
    return out;
}

/* The rightmost run of vowels, skipping a final consonant after it. */
static void vowel_run(const Word& w, int* first, int* last) {
    *first = *last = -1;
    for (int k = static_cast<int>(w.letters.size()) - 1; k >= 0; --k) {
        if (is_vowel(w.letters[static_cast<size_t>(k)].base)) {
            if (*last < 0) *last = k;
            *first = k;
        } else if (*last >= 0) {
            break;
        }
    }
}

static bool marked_by(const Word& w, const Letter& l, char key) {
    return l.mark_key >= 0 &&
           std::tolower(static_cast<unsigned char>(w.keys[static_cast<size_t>(l.mark_key)])) == key;
}

void telex_apply_key(Word& w, int i, const Options& o) {
    const char c = w.keys[static_cast<size_t>(i)];
    const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    KeyRec& rec = w.recs[static_cast<size_t>(i)];
    if (w.locked & key_bit(lc)) {
        word_add_letter(w, i);
        return;
    }
    const Word saved = w;
    auto keep_if_ok = [&](int kind) {
        if (word_prefix_ok(w)) {
            w.recs[static_cast<size_t>(i)].kind = static_cast<uint8_t>(kind);
            return true;
        }
        w = saved;
        return false;
    };

    bool has_vowel = false;
    for (const Letter& l : w.letters) has_vowel = has_vowel || is_vowel(l.base);
    /* Free marking (the default): a tone or mark key may follow the final
     * consonant or later vowels -- toans -> toán, dangd -> đang,
     * tienge -> tiêng, khongo -> không. Without it a tone must come before
     * the final consonant and a mark right after its letter. */
    const bool after_coda = !w.letters.empty() && !is_vowel(w.letters.back().base);
    const bool may_reach = o.free_marking || !after_coda;

    int tone = tone_of_key(lc);
    if (tone > 0 && has_vowel && may_reach) {
        if (w.tone == tone) {  /* ass -> as */
            int undone = w.tone_key;
            w.tone = XK_TONE_NONE;
            w.tone_key = -1;
            word_undo(w, i, undone);
            return;
        }
        w.tone = tone;
        w.tone_key = i;
        if (keep_if_ok(K_TONE)) return;
    }
    if (lc == 'z' && w.tone != XK_TONE_NONE) {  /* z takes the tone off */
        w.tone = XK_TONE_NONE;
        w.tone_key = -1;
        rec.kind = K_TONE;
        return;
    }

    if (lc == 'w') {
        std::vector<std::vector<int>> choices;
        if (may_reach) choices = w_choices(w);
        for (const std::vector<int>& t : choices) {
            bool done = true;
            for (int k : t) done = done && marked_by(w, w.letters[static_cast<size_t>(k)], 'w');
            if (done) continue;
            for (int k : t) {
                Letter& l = w.letters[static_cast<size_t>(k)];
                if (marked_by(w, l, 'w')) continue;  /* keep the horn of "uw" */
                l.mark = l.base == 'a' ? MARK_BREVE : MARK_HORN;
                l.mark_key = i;
                /* A single w on "uo" leaves it to the spelling whether the u
                 * shows its horn: ươ before a final (hương, người), uơ when
                 * the syllable ends there after h/kh/th (huơ, thuở). */
                l.soft = t.size() == 2 && l.base == 'u';
            }
            rec.target = w.letters[static_cast<size_t>(t.back())].key;
            if (t.size() == 2) rec.target2 = w.letters[static_cast<size_t>(t[0])].key;
            if (keep_if_ok(K_MARK)) return;
        }
        /* Nothing left to mark: a w typed again takes its marks back
         * (aww -> aw, uoww -> uow). */
        for (const std::vector<int>& t : choices) {
            bool done = true;
            for (int k : t) done = done && marked_by(w, w.letters[static_cast<size_t>(k)], 'w');
            if (!done) continue;
            int undone = -1;
            for (int k : t) {
                Letter& l = w.letters[static_cast<size_t>(k)];
                if (l.mark_key >= 0) w.recs[static_cast<size_t>(l.mark_key)].undone = true;
                undone = std::max(undone, l.mark_key);
                l.mark = MARK_NONE;
                l.mark_key = -1;
                l.soft = false;
            }
            /* "ww": the ư the first w typed goes, the second w is a w. */
            if (t.size() == 1 && w.letters[static_cast<size_t>(t[0])].key == undone)
                w.letters.erase(w.letters.begin() + t[0]);
            word_undo(w, i, undone);
            return;
        }
        /* With nothing to mark, w is ư where ư can go: w -> ư, tw -> tư,
         * nhw -> như (Bamboo's "Telex 2"). "new" stays new. */
        if (o.w_shortcut) {
            word_add_letter(w, i);
            Letter& u = w.letters.back();
            u.base = 'u';
            u.mark = MARK_HORN;
            u.mark_key = i;
            if (keep_if_ok(K_LETTER)) return;
        }
    }

    /* dd is đ; aa, ee, oo put the circumflex on. With free marking the d
     * reaches the onset and a/e/o the same vowel anywhere in the vowel
     * group, so they can follow the final consonant. */
    int target = -1;
    uint8_t mark = MARK_NONE;
    const int last = static_cast<int>(w.letters.size()) - 1;
    if (lc == 'd' && last >= 0) {
        mark = MARK_BAR;
        if (o.free_marking) target = w.letters[0].base == 'd' ? 0 : -1;
        else target = w.letters[static_cast<size_t>(last)].base == 'd' ? last : -1;
    } else if ((lc == 'a' || lc == 'e' || lc == 'o') && last >= 0) {
        mark = MARK_HAT;
        if (o.free_marking) {
            int first, end;
            vowel_run(w, &first, &end);
            for (int k = end; k >= 0 && k >= first; --k)
                if (w.letters[static_cast<size_t>(k)].base == lc) { target = k; break; }
        } else if (w.letters[static_cast<size_t>(last)].base == lc) {
            target = last;
        }
    }
    if (target >= 0) {
        Letter& t = w.letters[static_cast<size_t>(target)];
        if (t.mark == mark && marked_by(w, t, lc)) {
            /* aaa -> aa, ddd -> dd */
            int undone = t.mark_key;
            t.mark = MARK_NONE;
            t.mark_key = -1;
            word_undo(w, i, undone);
            return;
        }
        if (t.mark != mark) {  /* ă/ơ from w become â/ô: awa -> â */
            t.mark = mark;
            t.mark_key = i;
            t.soft = false;
            rec.target = t.key;
            if (keep_if_ok(K_MARK)) return;
        }
    }

    word_add_letter(w, i);
}

} /* namespace xrnikey */

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
 * vowels (skipping a coda after it), treating u+o as one unit that takes a
 * horn on BOTH halves; otherwise on the last vowel of the run that takes a w
 * mark (u -> ư, o -> ơ, a -> ă). Returns the letters it acts on (indexes into
 * w.letters), none when there is no vowel to mark (leading "w", "new"). */
static std::vector<int> w_targets(const Word& w) {
    const std::vector<Letter>& L = w.letters;
    int last = -1, first = -1;
    for (int k = static_cast<int>(L.size()) - 1; k >= 0; --k) {
        if (is_vowel(L[static_cast<size_t>(k)].base)) {
            if (last < 0) last = k;
            first = k;
        } else if (last >= 0) {
            break;
        }
    }
    if (last < 0) return {};

    auto takes_w = [](const Letter& l) {
        return (l.base == 'u' || l.base == 'o' || l.base == 'a') &&
               (l.mark == MARK_NONE || l.mark == MARK_HORN || l.mark == MARK_BREVE);
    };
    for (int k = first; k < last; ++k) {
        const Letter& u = L[static_cast<size_t>(k)];
        const Letter& o = L[static_cast<size_t>(k) + 1];
        if (u.base == 'u' && o.base == 'o' && takes_w(u) && takes_w(o)) return {k, k + 1};
    }
    for (int k = last; k >= first; --k)
        if (takes_w(L[static_cast<size_t>(k)])) return {k};
    return {};
}

static bool marked_by(const Word& w, const Letter& l, char key) {
    return l.mark_key >= 0 &&
           std::tolower(static_cast<unsigned char>(w.keys[static_cast<size_t>(l.mark_key)])) == key;
}

void telex_apply_key(Word& w, int i, const Options&) {
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

    int tone = tone_of_key(lc);
    if (tone > 0 && has_vowel) {
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
        std::vector<int> t = w_targets(w);
        bool all_w = !t.empty();
        for (int k : t) all_w = all_w && marked_by(w, w.letters[static_cast<size_t>(k)], 'w');
        if (all_w) {  /* aww -> aw, uoww -> uow */
            int undone = -1;
            for (int k : t) {
                Letter& l = w.letters[static_cast<size_t>(k)];
                if (l.mark_key >= 0) w.recs[static_cast<size_t>(l.mark_key)].undone = true;
                undone = std::max(undone, l.mark_key);
                l.mark = MARK_NONE;
                l.mark_key = -1;
            }
            word_undo(w, i, undone);
            return;
        }
        if (!t.empty()) {
            for (int k : t) {
                Letter& l = w.letters[static_cast<size_t>(k)];
                if (l.mark == MARK_HORN || l.mark == MARK_BREVE) continue;  /* keep "uw" */
                l.mark = l.base == 'a' ? MARK_BREVE : MARK_HORN;
                l.mark_key = i;
            }
            rec.target = w.letters[static_cast<size_t>(t.back())].key;
            if (t.size() == 2) rec.target2 = w.letters[static_cast<size_t>(t[0])].key;
            if (keep_if_ok(K_MARK)) return;
        }
    }

    if (!w.letters.empty()) {
        Letter& prev = w.letters.back();
        uint8_t mark = MARK_NONE;
        if (lc == 'd' && prev.base == 'd') mark = MARK_BAR;
        if ((lc == 'a' || lc == 'e' || lc == 'o') && prev.base == lc) mark = MARK_HAT;
        if (mark != MARK_NONE && prev.mark == mark && marked_by(w, prev, lc)) {
            /* aaa -> aa, ddd -> dd */
            int undone = prev.mark_key;
            prev.mark = MARK_NONE;
            prev.mark_key = -1;
            word_undo(w, i, undone);
            return;
        }
        if (mark != MARK_NONE && prev.mark == MARK_NONE) {
            prev.mark = mark;
            prev.mark_key = i;
            rec.target = prev.key;
            if (keep_if_ok(K_MARK)) return;
        }
    }

    word_add_letter(w, i);
}

} /* namespace xrnikey */

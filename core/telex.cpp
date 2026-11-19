#include "xrnikey_internal.h"
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
 * ngươi / dương exactly as "nguwowi" does. It marks the rightmost run of
 * vowels (skipping a coda after it), treating u+o as one unit that takes a
 * horn on BOTH halves; otherwise the last vowel of the run that takes a w
 * mark (u -> ư, o -> ơ, a -> ă). Returns the key kind, or -1 when there is no
 * vowel to mark (leading "w", "new"). */
static int apply_w(Word& w, int key) {
    std::vector<Letter>& L = w.letters;
    int last = -1, first = -1;
    for (int k = static_cast<int>(L.size()) - 1; k >= 0; --k) {
        if (is_vowel(L[static_cast<size_t>(k)].base)) {
            if (last < 0) last = k;
            first = k;
        } else if (last >= 0) {
            break;
        }
    }
    if (last < 0) return -1;

    auto takes_w = [](const Letter& l) {
        return (l.base == 'u' || l.base == 'o' || l.base == 'a') &&
               (l.mark == MARK_NONE || l.mark == MARK_HORN || l.mark == MARK_BREVE);
    };
    auto mark_for = [](char base) { return base == 'a' ? MARK_BREVE : MARK_HORN; };
    auto set = [&](Letter& l) {
        bool changed = l.mark != mark_for(l.base);
        l.mark = mark_for(l.base);
        l.mark_key = key;
        return changed;
    };

    for (int k = first; k < last; ++k) {
        Letter& u = L[static_cast<size_t>(k)];
        Letter& o = L[static_cast<size_t>(k) + 1];
        if (u.base == 'u' && o.base == 'o' && takes_w(u) && takes_w(o)) {
            bool changed = set(o);
            changed = set(u) || changed;
            return changed ? K_MARK : K_NOOP;
        }
    }
    for (int k = last; k >= first; --k) {
        Letter& l = L[static_cast<size_t>(k)];
        if (takes_w(l)) return set(l) ? K_MARK : K_NOOP;
    }
    return -1;
}

void telex_apply_key(Word& w, int i, const Options&) {
    const char c = w.keys[static_cast<size_t>(i)];
    const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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
        if (w.tone == tone) { w.recs[static_cast<size_t>(i)].kind = K_NOOP; return; }
        w.tone = tone;
        if (keep_if_ok(K_TONE)) return;
    }

    if (lc == 'w') {
        int kind = apply_w(w, i);
        if (kind == K_NOOP) { w = saved; w.recs[static_cast<size_t>(i)].kind = K_NOOP; return; }
        if (kind == K_MARK && keep_if_ok(K_MARK)) return;
        w = saved;
    }

    if (!w.letters.empty()) {
        Letter& prev = w.letters.back();
        uint8_t mark = MARK_NONE;
        if (lc == 'd' && prev.base == 'd' && prev.mark == MARK_NONE) mark = MARK_BAR;
        if ((lc == 'a' || lc == 'e' || lc == 'o') && prev.base == lc && prev.mark == MARK_NONE)
            mark = MARK_HAT;
        if (mark != MARK_NONE) {
            prev.mark = mark;
            prev.mark_key = i;
            if (keep_if_ok(K_MARK)) return;
        }
    }

    word_add_letter(w, i);
}

} /* namespace xrnikey */

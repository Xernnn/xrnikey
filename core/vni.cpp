#include "xrnikey_internal.h"
#include <cctype>

namespace xrnikey {

/* VNI: digits after the letters they change. 1-5 set the tone (sắc huyền
 * hỏi ngã nặng), 0 removes it, 6 is the circumflex (â ê ô), 7 the horn (ơ ư,
 * both halves of "uo"), 8 the breve (ă), 9 the bar (đ). A digit that has
 * nothing to change is just a digit. */

static bool is_vowel(char b) {
    return b == 'a' || b == 'e' || b == 'i' || b == 'o' || b == 'u' || b == 'y';
}

/* The rightmost run of vowels, skipping a coda after it: [*first, *last]. */
static bool vowel_run(const Word& w, int* first, int* last) {
    *first = *last = -1;
    for (int k = static_cast<int>(w.letters.size()) - 1; k >= 0; --k) {
        if (is_vowel(w.letters[static_cast<size_t>(k)].base)) {
            if (*last < 0) *last = k;
            *first = k;
        } else if (*last >= 0) {
            break;
        }
    }
    return *last >= 0;
}

static int mark_letter(Word& w, Letter& l, uint8_t mark, int key) {
    l.mark = mark;
    l.mark_key = key;
    w.recs[static_cast<size_t>(key)].target = l.key;
    return K_MARK;
}

/* Returns the key kind, or -1 when the digit has nothing to change. */
static int apply_digit(Word& w, int digit, int key) {
    int first, last;
    bool has_run = vowel_run(w, &first, &last);

    if (digit >= 1 && digit <= 5) {
        if (!has_run) return -1;
        w.tone = digit;
        return K_TONE;
    }
    if (digit == 0) {
        if (w.tone == XK_TONE_NONE) return -1;
        w.tone = XK_TONE_NONE;
        return K_TONE;
    }
    if (digit == 9) {
        for (int k = static_cast<int>(w.letters.size()) - 1; k >= 0; --k) {
            Letter& l = w.letters[static_cast<size_t>(k)];
            if (l.base == 'd') return mark_letter(w, l, MARK_BAR, key);
        }
        return -1;
    }
    if (!has_run) return -1;

    if (digit == 7) {
        for (int k = first; k < last; ++k) {
            Letter& u = w.letters[static_cast<size_t>(k)];
            Letter& o = w.letters[static_cast<size_t>(k) + 1];
            if (u.base == 'u' && o.base == 'o') {
                mark_letter(w, u, MARK_HORN, key);
                w.recs[static_cast<size_t>(key)].target2 = u.key;
                return mark_letter(w, o, MARK_HORN, key);
            }
        }
    }
    for (int k = last; k >= first; --k) {
        Letter& l = w.letters[static_cast<size_t>(k)];
        uint8_t mark = MARK_NONE;
        if (digit == 6 && (l.base == 'a' || l.base == 'e' || l.base == 'o')) mark = MARK_HAT;
        if (digit == 7 && (l.base == 'o' || l.base == 'u')) mark = MARK_HORN;
        if (digit == 8 && l.base == 'a') mark = MARK_BREVE;
        if (mark != MARK_NONE) return mark_letter(w, l, mark, key);
    }
    return -1;
}

void vni_apply_key(Word& w, int i, const Options&) {
    const char c = w.keys[static_cast<size_t>(i)];
    if (c >= '0' && c <= '9') {
        const Word saved = w;
        int kind = apply_digit(w, c - '0', i);
        if (kind >= 0 && word_prefix_ok(w)) {
            w.recs[static_cast<size_t>(i)].kind = static_cast<uint8_t>(kind);
            return;
        }
        w = saved;
    }
    word_add_letter(w, i);
}

} /* namespace xrnikey */

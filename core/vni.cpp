#include "xrnkey_internal.h"
#include <cctype>
#include <vector>

namespace xrnkey {

/* VNI: digits after the letters they change. 1-5 set the tone (sắc huyền
 * hỏi ngã nặng), 0 removes it, 6 is the circumflex (â ê ô), 7 the horn (ơ ư,
 * both halves of "uo"), 8 the breve (ă), 9 the bar (đ). A digit that has
 * nothing to change is just a digit, and so is one typed again to undo it
 * (a11 -> a1). The rules mirror Telex's: see telex.cpp. */

static bool is_vowel(char b) {
    return b == 'a' || b == 'e' || b == 'i' || b == 'o' || b == 'u' || b == 'y';
}

/* The rightmost run of vowels, skipping a coda after it: [*first, *last].
 * The u of "qu" belongs to the onset. */
static bool vowel_run(const Word& w, int* first, int* last) {
    const std::vector<Letter>& L = w.letters;
    const int onset_u = (L.size() > 1 && L[0].base == 'q' && L[1].base == 'u') ? 1 : -1;
    *first = *last = -1;
    for (int k = static_cast<int>(L.size()) - 1; k >= 0; --k) {
        if (is_vowel(L[static_cast<size_t>(k)].base) && k != onset_u) {
            if (*last < 0) *last = k;
            *first = k;
        } else if (*last >= 0) {
            break;
        }
    }
    return *last >= 0;
}

static uint8_t mark_of_digit(int digit, char base) {
    if (digit == 6 && (base == 'a' || base == 'e' || base == 'o')) return MARK_HAT;
    if (digit == 7 && (base == 'o' || base == 'u')) return MARK_HORN;
    if (digit == 8 && base == 'a') return MARK_BREVE;
    if (digit == 9 && base == 'd') return MARK_BAR;
    return MARK_NONE;
}

/* The letters a mark digit may act on, best first; each choice is a list of
 * indexes into w.letters. */
static std::vector<std::vector<int>> mark_choices(const Word& w, int digit, bool free) {
    std::vector<std::vector<int>> out;
    const int n = static_cast<int>(w.letters.size());
    if (digit == 9) {
        int k = free ? 0 : n - 1;
        if (n > 0 && w.letters[static_cast<size_t>(k)].base == 'd') out.push_back({k});
        return out;
    }
    int first, last;
    if (!vowel_run(w, &first, &last)) return out;
    if (!free && last != n - 1) return out;  /* the mark must follow its letter */
    if (digit == 7) {
        for (int k = first; k < last; ++k)
            if (w.letters[static_cast<size_t>(k)].base == 'u' &&
                w.letters[static_cast<size_t>(k) + 1].base == 'o')
                out.push_back({k, k + 1});
    }
    for (int k = last; k >= first; --k)
        if (mark_of_digit(digit, w.letters[static_cast<size_t>(k)].base) != MARK_NONE)
            out.push_back({k});
    return out;
}

static bool marked_by(const Word& w, const Letter& l, char key) {
    return l.mark_key >= 0 && w.keys[static_cast<size_t>(l.mark_key)] == key;
}

void vni_apply_key(Word& w, int i, const Options& o) {
    const char c = w.keys[static_cast<size_t>(i)];
    if (c < '0' || c > '9' || (w.locked & key_bit(c))) {
        word_add_letter(w, i);
        return;
    }
    const int digit = c - '0';
    KeyRec& rec = w.recs[static_cast<size_t>(i)];
    const Word saved = w;
    auto keep_if_ok = [&](int kind) {
        if (word_prefix_ok(w)) {
            w.recs[static_cast<size_t>(i)].kind = static_cast<uint8_t>(kind);
            return true;
        }
        w = saved;
        return false;
    };
    const bool after_coda = !w.letters.empty() && !is_vowel(w.letters.back().base);
    int first, last;
    const bool has_vowel = vowel_run(w, &first, &last);

    if (digit >= 1 && digit <= 5) {
        if (has_vowel && (o.free_marking || !after_coda)) {
            if (w.tone == digit) {  /* a11 -> a1 */
                int undone = w.tone_key;
                w.tone = XK_TONE_NONE;
                w.tone_key = -1;
                word_undo(w, i, undone);
                return;
            }
            w.tone = digit;
            w.tone_key = i;
            if (keep_if_ok(K_TONE)) return;
        }
        word_add_letter(w, i);
        return;
    }
    if (digit == 0) {
        if (w.tone != XK_TONE_NONE) {
            w.tone = XK_TONE_NONE;
            w.tone_key = -1;
            rec.kind = K_TONE;
            return;
        }
        word_add_letter(w, i);
        return;
    }

    std::vector<std::vector<int>> choices = mark_choices(w, digit, o.free_marking);
    for (const std::vector<int>& t : choices) {
        bool done = true;
        for (int k : t) done = done && marked_by(w, w.letters[static_cast<size_t>(k)], c);
        if (done) continue;
        for (int k : t) {
            Letter& l = w.letters[static_cast<size_t>(k)];
            if (marked_by(w, l, c)) continue;  /* keep the horn of "u7o" */
            l.mark = mark_of_digit(digit, l.base);
            l.mark_key = i;
            /* As with Telex w: thuo7 is thuơ, huong7 hương. */
            l.soft = t.size() == 2 && l.base == 'u';
        }
        rec.target = w.letters[static_cast<size_t>(t.back())].key;
        if (t.size() == 2) rec.target2 = w.letters[static_cast<size_t>(t[0])].key;
        if (keep_if_ok(K_MARK)) return;
    }
    for (const std::vector<int>& t : choices) {  /* a66 -> a6, d99 -> d9 */
        bool done = true;
        for (int k : t) done = done && marked_by(w, w.letters[static_cast<size_t>(k)], c);
        if (!done) continue;
        int undone = -1;
        for (int k : t) {
            Letter& l = w.letters[static_cast<size_t>(k)];
            w.recs[static_cast<size_t>(l.mark_key)].undone = true;
            if (l.mark_key > undone) undone = l.mark_key;
            l.mark = MARK_NONE;
            l.mark_key = -1;
            l.soft = false;
        }
        word_undo(w, i, undone);
        return;
    }
    word_add_letter(w, i);
}

} /* namespace xrnkey */

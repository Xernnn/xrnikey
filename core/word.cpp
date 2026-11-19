#include "xrnikey_internal.h"
#include <cctype>

namespace xrnikey {

/* The word in progress is kept as the raw keystrokes. Every change replays
 * them from the start, so the composition is a pure function of the keys and
 * the keys themselves are always there to fall back to: that is what lets a
 * word that turns out not to be Vietnamese ("password", "user") come back
 * exactly as typed, instead of the half-transformed "pẳod" / "ủe". */

static char32_t glyph(char base, uint8_t mark) {
    switch (mark) {
        case MARK_HAT:
            if (base == 'a') return U'â';
            if (base == 'e') return U'ê';
            if (base == 'o') return U'ô';
            break;
        case MARK_BREVE:
            if (base == 'a') return U'ă';
            break;
        case MARK_HORN:
            if (base == 'o') return U'ơ';
            if (base == 'u') return U'ư';
            break;
        case MARK_BAR:
            if (base == 'd') return U'đ';
            break;
        default:
            break;
    }
    return static_cast<char32_t>(static_cast<unsigned char>(base));
}

void word_replay(Word& w, int mode, const Options& o) {
    w.letters.clear();
    w.tone = XK_TONE_NONE;
    w.recs.assign(w.keys.size(), KeyRec{});
    for (int i = 0; i < static_cast<int>(w.keys.size()); ++i) {
        if (mode == XK_MODE_VNI) vni_apply_key(w, i, o);
        else telex_apply_key(w, i, o);
    }
}

void word_add_letter(Word& w, int key_index) {
    char c = w.keys[static_cast<size_t>(key_index)];
    Letter l;
    l.base = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    l.upper = std::isupper(static_cast<unsigned char>(c)) != 0;
    l.mark = MARK_NONE;
    l.key = key_index;
    l.mark_key = -1;
    w.letters.push_back(l);
    w.recs[static_cast<size_t>(key_index)].kind = K_LETTER;
}

std::u32string word_letters(const Word& w) {
    std::u32string out;
    for (const Letter& l : w.letters) out.push_back(glyph(l.base, l.mark));
    return out;
}

bool word_prefix_ok(const Word& w) {
    return syllable_prefix_ok(word_letters(w), w.tone);
}

std::string word_render(const Word& w) {
    std::string s;
    char buf[8];
    for (const Letter& l : w.letters) {
        char32_t g = glyph(l.base, l.mark);
        if (l.upper && g < 0x80) g = static_cast<char32_t>(std::toupper(static_cast<int>(g)));
        else if (l.upper && g == U'đ') g = U'Đ';
        int n = utf8_encode(static_cast<uint32_t>(g), buf);
        s.append(buf, static_cast<size_t>(n));
    }
    return nfc_compose(apply_tone_to_syllable(s, w.tone));
}

std::string word_raw(const Word& w) {
    return w.keys;
}

/* Marks other than đ, or a tone: what makes text recognisably Vietnamese.
 * đ alone does not count -- "đc", "đt", "đk" are everyday abbreviations and
 * must stay as typed even though they are not syllables. */
static bool has_vn_marks(const Word& w) {
    if (w.tone != XK_TONE_NONE) return true;
    for (const Letter& l : w.letters)
        if (l.mark != MARK_NONE && l.mark != MARK_BAR) return true;
    return false;
}

std::string word_display(const Word& w, const Options& o) {
    std::string v = word_render(w);
    if (!o.auto_restore || !has_vn_marks(w) || word_prefix_ok(w)) return v;
    /* No more keys can make this Vietnamese ("tẽt"): show what was typed. */
    return word_raw(w);
}

std::string word_final(const Word& w, const Options& o) {
    std::string v = word_render(w);
    if (!o.auto_restore) return v;
    std::string raw = word_raw(w);
    if (v == raw) return v;  /* nothing was transformed */

    std::u32string letters = word_letters(w);
    if (syllable_valid(letters, w.tone)) {
        /* A real syllable, but the keys also spell a common English word and
         * the syllable is not one Vietnamese uses ("terms" -> "tém",
         * "more" -> "mỏe"): the English word was meant. A syllable that IS
         * attested stays Vietnamese even when the keys are English ("of" ->
         * "ò", "is" -> "í"), as in Bamboo: typing Vietnamese never turns
         * into English. */
        if (!lexicon_is_attested(letters, w.tone) && lexicon_is_english(raw)) return raw;
        return v;
    }
    if (lexicon_is_english(raw)) return raw;
    if (has_vn_marks(w)) return raw;
    return v;
}

} /* namespace xrnikey */

#include "xrnkey_internal.h"
#include <cctype>

namespace xrnkey {

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
    w.tone_key = -1;
    w.locked = 0;
    w.recs.assign(w.keys.size(), KeyRec{});
    for (int i = 0; i < static_cast<int>(w.keys.size()); ++i) {
        if (w.literal) word_add_letter(w, i);
        else if (mode == XK_MODE_VNI) vni_apply_key(w, i, o);
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
    l.soft = false;
    w.letters.push_back(l);
    w.recs[static_cast<size_t>(key_index)].kind = K_LETTER;
}

/* "uo" horned by one w is written uơ, not ươ, when the syllable ends right
 * there after h, kh or th: huơ, khuơ, thuở. Everywhere else it is ươ
 * (hương, thương, người, and "ươ" with no onset). */
static bool soft_horn_hidden(const Word& w, size_t i) {
    const std::vector<Letter>& L = w.letters;
    if (i + 2 != L.size() || L[i + 1].base != 'o' || L[i + 1].mark != MARK_HORN) return false;
    std::string onset;
    for (size_t k = 0; k < i; ++k) onset += L[k].base;
    return onset == "h" || onset == "kh" || onset == "th";
}

std::u32string word_letters(const Word& w) {
    std::u32string out;
    for (size_t i = 0; i < w.letters.size(); ++i) {
        const Letter& l = w.letters[i];
        bool plain = l.soft && soft_horn_hidden(w, i);
        out.push_back(glyph(l.base, plain ? uint8_t{MARK_NONE} : l.mark));
    }
    return out;
}

bool word_prefix_ok(const Word& w) {
    return syllable_prefix_ok(word_letters(w), w.tone);
}

/* Index into w.letters of the vowel that carries the tone, or -1. */
static int tone_letter(const Word& w, bool modern) {
    if (w.tone == XK_TONE_NONE) return -1;
    std::u32string letters = word_letters(w);
    int off = find_tone_vowel_index(utf8_of(letters), modern);
    char buf[8];
    int at = 0;
    for (size_t i = 0; i < letters.size(); ++i) {
        if (at == off) return static_cast<int>(i);
        at += utf8_encode(static_cast<uint32_t>(letters[i]), buf);
    }
    return -1;
}

/* Each letter takes the case of the key that typed it, whatever later keys
 * marked or toned it: "As" is Á, "AAn" Ân, "HOAF" HÒA, "Owr" Ở. The old
 * renderer took toned and marked vowels from lowercase-only tables (and
 * lowercased doubled vowels), giving "á", "ân", "HòA", "Ơr". */
std::string word_render(const Word& w, const Options& o) {
    std::u32string lower = word_letters(w);
    const int carrier = tone_letter(w, o.modern_style);
    std::string s;
    char buf[8];
    for (size_t i = 0; i < lower.size(); ++i) {
        char32_t g = lower[i];
        if (static_cast<int>(i) == carrier) g = vn_toned(g, w.tone);
        if (w.letters[i].upper) g = vn_upper(g);
        int n = utf8_encode(static_cast<uint32_t>(g), buf);
        s.append(buf, static_cast<size_t>(n));
    }
    return s;
}

std::string word_raw(const Word& w) {
    return w.keys;
}

std::string word_raw_effective(const Word& w) {
    std::string out;
    for (size_t k = 0; k < w.keys.size(); ++k)
        if (!w.recs[k].undone) out += w.keys[k];
    return out;
}

uint64_t key_bit(char lc) {
    if (lc >= 'a' && lc <= 'z') return uint64_t{1} << (lc - 'a');
    if (lc >= '0' && lc <= '9') return uint64_t{1} << (26 + (lc - '0'));
    return 0;
}

/* Bamboo's rule: pressing an effect key again takes the effect back and
 * types the key as a letter (aaa -> aa, ass -> as, dd d -> dd), and for the
 * rest of the word that key is only a letter (aaaa -> aaa). */
void word_undo(Word& w, int i, int undone) {
    const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(w.keys[static_cast<size_t>(i)])));
    if (undone >= 0) w.recs[static_cast<size_t>(undone)].undone = true;
    word_add_letter(w, i);
    w.recs[static_cast<size_t>(i)].undoes = undone;
    w.locked |= key_bit(lc);
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
    if (w.literal) return word_raw(w);
    std::string v = word_render(w, o);
    if (!o.auto_restore || !has_vn_marks(w) || word_prefix_ok(w)) return v;
    /* No more keys can make this Vietnamese ("tẽt"): show what was typed. */
    return word_raw_effective(w);
}

std::string word_final(const Word& w, const Options& o, const XKStateImpl* s) {
    if (w.literal) return word_raw(w);
    std::string v = word_render(w, o);
    if (!o.auto_restore) return v;
    /* Two readings of the keys as plain text: with keys a repeat undid left
     * out ("ass" -> "as", "pass" -> "pas"), and exactly as typed ("pass").
     * When both are English, the keys as typed win -- a repeated key in
     * the middle of a word ("losses", "basses") or one far from the key it
     * undid ("marker", "casts", "doses") is the English spelling, and the
     * shorter reading used to turn them into "loses", "maker", "cats" --
     * unless the word ends with a key typed again right after itself to
     * take its effect back: that is how Telex types "as" ("ass") and "of"
     * ("off"). */
    std::string eff = word_raw_effective(w);
    std::string raw = word_raw(w);
    if (v == eff && v == raw) return v;  /* nothing was transformed */
    const size_t n = w.keys.size();
    const bool undone_at_end = n >= 2 && w.recs[n - 1].undoes == static_cast<int>(n) - 2;
    auto english = [&]() -> const std::string* {
        const std::string* first = undone_at_end ? &eff : &raw;
        const std::string* second = undone_at_end ? &raw : &eff;
        if (lexicon_is_english(*first)) return first;
        if (lexicon_is_english(*second)) return second;
        return nullptr;
    };

    std::u32string letters = word_letters(w);
    if (syllable_valid(letters, w.tone)) {
        /* A real syllable, but the keys also spell a common English word and
         * the syllable is not one Vietnamese uses ("terms" -> "tém",
         * "more" -> "mỏe"): the English word was meant. */
        const std::string* en = english();
        if (!lexicon_is_attested(letters, w.tone)) return en ? *en : v;
        /* Both are real words: "list" is "lít", "turn" is "tủn". With the
         * language models, the words around decide ("my list", "hai lít").
         * Without them the syllable wins, as in Bamboo: typing Vietnamese
         * never turns into English. */
        if (en && s && bilingual_ready(*s) && bilingual_prefers_english(*s, v, *en)) return *en;
        return v;
    }
    if (const std::string* en = english()) return *en;
    if (has_vn_marks(w)) return eff;
    return v;
}

/* The old BackSpace undid the last transformation instead of deleting a
 * character: "toans" BackSpace gave "toan" and typing n then gave "toann";
 * in a word passed through as English it popped bytes off a buffer that
 * also held the deleted keys' effects, so "window" BackSpace emptied the
 * word and "hello" became "he". Now the last character shown goes, with
 * every key that produced it -- the key that typed the letter, the keys that
 * marked it, and the tone keys if it carried the tone -- and the word is
 * recomposed from the keys that remain: "toán" -> "tóa" (the tone stays and
 * moves where "toa" takes it), "được" -> "đượ", "thuở" -> "thu". */

/* Key `k` (a mark key that marked two letters: one w or 7 on "uo") also
 * marked a letter other than the one typed by key `deleted`, and that letter
 * shows the mark. The key then stays, for that letter: "ngườ" BackSpace is
 * "ngư", not "ngu". (In "thuở" the u's horn is not shown, so "thu".) */
static bool marks_other_shown(const Word& w, int k, const KeyRec& rec, int deleted) {
    const int other = rec.target == deleted ? rec.target2 : rec.target;
    if (other < 0) return false;
    for (size_t i = 0; i < w.letters.size(); ++i) {
        const Letter& l = w.letters[i];
        if (l.key == other) return l.mark_key == k && !(l.soft && soft_horn_hidden(w, i));
    }
    return false;
}

void word_backspace(Word& w, int mode, const Options& o) {
    if (w.empty()) return;
    if (w.literal || word_display(w, o) != word_render(w, o)) {
        /* Shown as typed: BackSpace edits the keys themselves. */
        w.keys = w.literal ? word_raw(w) : word_raw_effective(w);
        w.keys.pop_back();
        w.literal = true;
    } else if (!w.letters.empty()) {
        const int last = static_cast<int>(w.letters.size()) - 1;
        const int key = w.letters[static_cast<size_t>(last)].key;
        const int partner = w.recs[static_cast<size_t>(key)].undoes;  /* "aaa": the 2nd a */
        const bool carries_tone = tone_letter(w, o.modern_style) == last;
        std::string keep;
        for (size_t k = 0; k < w.keys.size(); ++k) {
            const KeyRec& rec = w.recs[k];
            bool drop = static_cast<int>(k) == key || static_cast<int>(k) == partner ||
                        (rec.kind == K_MARK && (rec.target == key || rec.target2 == key) &&
                         !marks_other_shown(w, static_cast<int>(k), rec, key)) ||
                        (rec.kind == K_TONE && carries_tone);
            if (!drop) keep += w.keys[k];
        }
        w.keys = keep;
    }
    if (w.keys.empty()) w.clear();
    word_replay(w, mode, o);
}

} /* namespace xrnkey */

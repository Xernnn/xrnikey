#include "xrnikey_internal.h"
#include <unordered_set>
#include <string>

namespace xrnikey {

static const char* ONSETS[] = {
    "", "b", "c", "ch", "d", "đ", "g", "gh", "gi", "h", "k", "kh", "l",
    "m", "n", "ng", "ngh", "nh", "p", "ph", "qu", "r", "s", "t", "th",
    "tr", "v", "x"
};

static const char* CODAS[] = {
    "", "c", "ch", "m", "n", "ng", "nh", "p", "t"
};

static bool starts_with(const std::string& s, const char* prefix) {
    return s.compare(0, std::strlen(prefix), prefix) == 0;
}

static bool match_syllable_structure(const std::string& word) {
    std::string w = word;
    for (char& c : w) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    for (const char* onset : ONSETS) {
        if (!starts_with(w, onset)) continue;
        std::string rest = w.substr(std::strlen(onset));

        /* The nucleus is the maximal leading run of vowel glyphs (1-3
         * vowels; toned or not) — every syllable coda is a pure consonant,
         * so there's no ambiguity to backtrack over, unlike a fixed nucleus
         * table (which would need every combination enumerated, including
         * "ươ"-family and triphthongs like "iêu" — easy to miss some). */
        size_t vowel_bytes = 0;
        int vowel_count = 0;
        while (vowel_count < 3) {
            int len = 0;
            if (!is_vietnamese_vowel_glyph(rest, vowel_bytes, &len)) break;
            vowel_bytes += static_cast<size_t>(len);
            vowel_count++;
        }
        if (vowel_count == 0) continue;

        std::string tail = rest.substr(vowel_bytes);
        for (const char* coda : CODAS) {
            if (tail == coda) return true;
        }
    }
    return false;
}

/* True while `word` could still GROW INTO a valid Vietnamese syllable.
 *
 * is_valid_vn_syllable() only accepts finished syllables, so it cannot be
 * consulted while a word is being typed. This one accepts partial input and
 * rejects only what no amount of further typing could rescue: an onset
 * Vietnamese does not have ("w", "sh", "st", "br"), a fourth nucleus vowel,
 * or a coda it does not have ("hel", "hold"). That is enough to tell an
 * English word from a Vietnamese one within the first few keystrokes, which
 * is what stops "word" from becoming "wỏd" when the 'r' arrives. */
bool can_begin_vn_syllable(const std::string& word) {
    if (word.empty()) return true;

    /* Lowercase UTF-8-aware. A byte-wise tolower() would leave "Đ" (U+0110)
     * alone, so it never matched the "đ" in ONSETS and every capitalised
     * Vietnamese word starting with Đ was classed as English -- "DDaf" came
     * out "Đaf" because the tone key was then treated as a literal. */
    std::string w;
    w.reserve(word.size());
    for (size_t i = 0; i < word.size(); ) {
        uint32_t cp = 0;
        int len = 1;
        if (utf8_decode(word.c_str() + i, &cp, &len) != 0 || len <= 0) {
            cp = static_cast<unsigned char>(word[i]);
            len = 1;
        }
        if (cp < 0x80) {
            cp = static_cast<uint32_t>(
                std::tolower(static_cast<unsigned char>(cp)));
        } else {
            /* Explicit table, NOT an arithmetic rule: the uppercase form is
             * the even codepoint for Ơ (U+01A0/U+01A1) but the ODD one for
             * Ư (U+01AF/U+01B0), so "+1 if even" silently corrupts ư into a
             * non-letter and every ươ word stops composing. */
            switch (cp) {
                case 0x00C2: cp = 0x00E2; break;  /* Â */
                case 0x00CA: cp = 0x00EA; break;  /* Ê */
                case 0x00D4: cp = 0x00F4; break;  /* Ô */
                case 0x0102: cp = 0x0103; break;  /* Ă */
                case 0x0110: cp = 0x0111; break;  /* Đ */
                case 0x01A0: cp = 0x01A1; break;  /* Ơ */
                case 0x01AF: cp = 0x01B0; break;  /* Ư */
                default: break;
            }
        }
        char enc[8];
        int n = utf8_encode(cp, enc);
        w.append(enc, static_cast<size_t>(n > 0 ? n : 0));
        i += static_cast<size_t>(len);
    }

    /* Onset: the leading run of non-vowel glyphs. */
    size_t i = 0;
    while (i < w.size()) {
        int len = 0;
        if (is_vietnamese_vowel_glyph(w, i, &len)) break;
        uint32_t cp = 0;
        int clen = 1;
        if (utf8_decode(w.c_str() + i, &cp, &clen) != 0 || clen <= 0) clen = 1;
        i += static_cast<size_t>(clen);
    }

    std::string onset = w.substr(0, i);
    bool onset_ok = false;
    for (const char* o : ONSETS) {
        std::string os(o);
        if (os.size() >= onset.size() &&
            os.compare(0, onset.size(), onset) == 0) {
            onset_ok = true;
            break;
        }
    }
    if (!onset_ok) return false;
    if (i >= w.size()) return true;

    /* Nucleus: up to three vowel glyphs. */
    int vowel_count = 0;
    while (vowel_count < 3) {
        int len = 0;
        if (!is_vietnamese_vowel_glyph(w, i, &len)) break;
        i += static_cast<size_t>(len);
        vowel_count++;
    }
    if (vowel_count == 0) return false;
    if (i >= w.size()) return true;

    /* A fourth vowel cannot belong to any Vietnamese nucleus. */
    {
        int len = 0;
        if (is_vietnamese_vowel_glyph(w, i, &len)) return false;
    }

    /* Coda: must be a prefix of one of the real codas. */
    std::string coda = w.substr(i);
    for (const char* cd : CODAS) {
        std::string cs(cd);
        if (cs.size() >= coda.size() &&
            cs.compare(0, coda.size(), coda) == 0) {
            return true;
        }
    }
    return false;
}

bool is_valid_vn_syllable(const std::string& word) {
    if (word.empty() || word.size() > 7) return false;
    return match_syllable_structure(word);
}

} /* namespace xrnikey */

extern "C" {

int xk_is_valid_syllable(const char* word) {
    if (!word) return 0;
    return xrnikey::is_valid_vn_syllable(word) ? 1 : 0;
}

} /* extern "C" */

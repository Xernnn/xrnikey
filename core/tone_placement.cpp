#include "xrnikey_internal.h"
#include <map>
#include <vector>
#include <cstdlib>
#include <cstring>

namespace xrnikey {

/* Every Vietnamese vowel identity (plain and already-modified) needs its own
 * row: â/ă/ê/ô/ơ/ư are distinct letters, not decorations on a/e/o/u, and
 * each combines with a tone into its own precomposed Unicode codepoint —
 * collapsing them to a shared base (as this table used to) drops the
 * circumflex/breve/horn as soon as a tone is applied (e.g. "thê" + sắc must
 * produce "thế", not "thé"). */
enum VowelId {
    V_A = 0, V_AW, V_AA, V_E, V_EE, V_I, V_O, V_OO, V_OW, V_U, V_UW, V_Y,
    V_COUNT
};

struct VowelEntry {
    const char* plain;
    const char* sac;
    const char* huyen;
    const char* hoi;
    const char* nga;
    const char* nang;
};

static const VowelEntry VOWELS[V_COUNT] = {
    /* a */  {"a", "\xC3\xA1", "\xC3\xA0", "\xE1\xBA\xA3", "\xC3\xA3", "\xE1\xBA\xA1"},
    /* ă */  {"\xC4\x83", "\xE1\xBA\xAF", "\xE1\xBA\xB1", "\xE1\xBA\xB3", "\xE1\xBA\xB5", "\xE1\xBA\xB7"},
    /* â */  {"\xC3\xA2", "\xE1\xBA\xA5", "\xE1\xBA\xA7", "\xE1\xBA\xA9", "\xE1\xBA\xAB", "\xE1\xBA\xAD"},
    /* e */  {"e", "\xC3\xA9", "\xC3\xA8", "\xE1\xBA\xBB", "\xE1\xBA\xBD", "\xE1\xBA\xB9"},
    /* ê */  {"\xC3\xAA", "\xE1\xBA\xBF", "\xE1\xBB\x81", "\xE1\xBB\x83", "\xE1\xBB\x85", "\xE1\xBB\x87"},
    /* i */  {"i", "\xC3\xAD", "\xC3\xAC", "\xE1\xBB\x89", "\xC4\xA9", "\xE1\xBB\x8B"},
    /* o */  {"o", "\xC3\xB3", "\xC3\xB2", "\xE1\xBB\x8F", "\xC3\xB5", "\xE1\xBB\x8D"},
    /* ô */  {"\xC3\xB4", "\xE1\xBB\x91", "\xE1\xBB\x93", "\xE1\xBB\x95", "\xE1\xBB\x97", "\xE1\xBB\x99"},
    /* ơ */  {"\xC6\xA1", "\xE1\xBB\x9B", "\xE1\xBB\x9D", "\xE1\xBB\x9F", "\xE1\xBB\xA1", "\xE1\xBB\xA3"},
    /* u */  {"u", "\xC3\xBA", "\xC3\xB9", "\xE1\xBB\xA7", "\xC5\xA9", "\xE1\xBB\xA5"},
    /* ư */  {"\xC6\xB0", "\xE1\xBB\xA9", "\xE1\xBB\xAB", "\xE1\xBB\xAD", "\xE1\xBB\xAF", "\xE1\xBB\xB1"},
    /* y */  {"y", "\xC3\xBD", "\xE1\xBB\xB3", "\xE1\xBB\xB7", "\xE1\xBB\xB9", "\xE1\xBB\xB5"},
};

static bool is_modified_vowel(int id) {
    return id == V_AW || id == V_AA || id == V_EE || id == V_OO ||
           id == V_OW || id == V_UW;
}

static const char* get_toned_vowel(int vowel_id, int tone) {
    if (vowel_id < 0 || vowel_id >= V_COUNT) return nullptr;
    const VowelEntry& v = VOWELS[vowel_id];
    switch (tone) {
        case XK_TONE_SAC:   return v.sac;
        case XK_TONE_HUYEN: return v.huyen;
        case XK_TONE_HOI:   return v.hoi;
        case XK_TONE_NGA:   return v.nga;
        case XK_TONE_NANG:  return v.nang;
        default: return v.plain;
    }
}

/* Identify the vowel (plain or already-modified) at byte offset `pos`.
 * Fills out_vowel_id and out_len (in bytes) on match. */
static bool is_special_vowel(const std::string& s, size_t pos, int* out_vowel_id, int* out_len) {
    if (pos + 1 < s.size()) {
        unsigned char b0 = static_cast<unsigned char>(s[pos]);
        unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
        if (b0 == 0xC4 && b1 == 0x83) { *out_vowel_id = V_AW; *out_len = 2; return true; } /* ă */
        if (b0 == 0xC3 && b1 == 0xA2) { *out_vowel_id = V_AA; *out_len = 2; return true; } /* â */
        if (b0 == 0xC3 && b1 == 0xAA) { *out_vowel_id = V_EE; *out_len = 2; return true; } /* ê */
        if (b0 == 0xC3 && b1 == 0xB4) { *out_vowel_id = V_OO; *out_len = 2; return true; } /* ô */
        if (b0 == 0xC6 && b1 == 0xA1) { *out_vowel_id = V_OW; *out_len = 2; return true; } /* ơ */
        if (b0 == 0xC6 && b1 == 0xB0) { *out_vowel_id = V_UW; *out_len = 2; return true; } /* ư */
    }
    if (pos < s.size()) {
        char c = static_cast<char>(s[pos] | 0x20);
        switch (c) {
            case 'a': *out_vowel_id = V_A; *out_len = 1; return true;
            case 'e': *out_vowel_id = V_E; *out_len = 1; return true;
            case 'i': *out_vowel_id = V_I; *out_len = 1; return true;
            case 'o': *out_vowel_id = V_O; *out_len = 1; return true;
            case 'u': *out_vowel_id = V_U; *out_len = 1; return true;
            case 'y': *out_vowel_id = V_Y; *out_len = 1; return true;
        }
    }
    return false;
}

bool is_vietnamese_vowel_glyph(const std::string& s, size_t pos, int* out_len) {
    int vid, vlen;
    if (is_special_vowel(s, pos, &vid, &vlen)) {
        *out_len = vlen;
        return true;
    }
    /* Also recognize toned glyphs (á, ẩ, ...), so this stays correct even
     * when called on already-toned text. */
    for (const VowelEntry& v : VOWELS) {
        const char* forms[5] = {v.sac, v.huyen, v.hoi, v.nga, v.nang};
        for (const char* f : forms) {
            size_t flen = std::strlen(f);
            if (s.compare(pos, flen, f) == 0) {
                *out_len = static_cast<int>(flen);
                return true;
            }
        }
    }
    return false;
}

int find_tone_vowel_index(const std::string& syllable) {
    /* Rule 1: a modified vowel (â ê ô ơ ă ư) always takes the tone. When two
     * modified vowels are adjacent — the ươ diphthong ("được", "người",
     * "mười", "trường"...) — the tone goes on the second one, not the
     * first. */
    for (size_t i = 0; i < syllable.size(); ) {
        int vid, vlen;
        if (is_special_vowel(syllable, i, &vid, &vlen) && is_modified_vowel(vid)) {
            size_t j = i + static_cast<size_t>(vlen);
            int vid2, vlen2;
            while (is_special_vowel(syllable, j, &vid2, &vlen2) && is_modified_vowel(vid2)) {
                i = j;
                vlen = vlen2;
                j = i + static_cast<size_t>(vlen);
            }
            return static_cast<int>(i);
        }
        uint32_t cp;
        int len;
        utf8_decode(syllable.c_str() + i, &cp, &len);
        i += len;
    }

    /* Collect vowel positions (plain or modified) */
    std::vector<int> positions;
    for (size_t i = 0; i < syllable.size(); ) {
        int vid, vlen;
        if (is_special_vowel(syllable, i, &vid, &vlen)) {
            positions.push_back(static_cast<int>(i));
            i += static_cast<size_t>(vlen);
        } else {
            i++;
        }
    }

    /* "qu" and "gi" are onset digraphs (the u/i is a glide, part of the
     * initial consonant sound /kw/ or /z/), not a tone-bearing vowel —
     * except when it's the only vowel in the syllable ("gì", "gìn"), where
     * it IS the nucleus. E.g. "quà"/"già" take the tone on 'a', not on the
     * u/i right after q/g. */
    if (positions.size() > 1 && positions.front() == 1) {
        char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(syllable[0])));
        char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(syllable[1])));
        if ((c0 == 'q' && c1 == 'u') || (c0 == 'g' && c1 == 'i')) {
            positions.erase(positions.begin());
        }
    }

    if (positions.empty()) return -1;
    if (positions.size() == 1) return positions[0];

    /* Modern Vietnamese orthography (1980 spelling standard) for a plain
     * (unmodified) vowel cluster: tone goes on the LAST vowel when the
     * syllable has a final consonant (closer to the coda, e.g. "toán"),
     * and on the SECOND-TO-LAST vowel when the syllable is open.
     *
     * For the common two-vowel case that is simply the first vowel ("cõi",
     * "của", "chào", "hòa"), which is what this used to return. But an open
     * TRIPHTHONG has three: "oai" must take the tone on its middle vowel --
     * ngoài, xoài, loại, thoại, ngoại -- and returning the first vowel gave
     * "ngòai". Counting from the end is right for both lengths. */
    bool has_coda = false;
    {
        int last_pos = positions.back();
        int vid, vlen;
        is_special_vowel(syllable, static_cast<size_t>(last_pos), &vid, &vlen);
        if (static_cast<size_t>(last_pos + vlen) < syllable.size()) has_coda = true;
    }

    return has_coda ? positions.back() : positions[positions.size() - 2];
}

std::string apply_tone_to_syllable(const std::string& base, int tone) {
    if (tone == XK_TONE_NONE || base.empty()) return base;

    int idx = find_tone_vowel_index(base);
    if (idx < 0) return base;

    int vowel_id = -1, vlen = 0;
    is_special_vowel(base, static_cast<size_t>(idx), &vowel_id, &vlen);

    std::string result = base.substr(0, idx);
    const char* toned = get_toned_vowel(vowel_id, tone);
    if (toned) result += toned;
    else result += base.substr(static_cast<size_t>(idx), static_cast<size_t>(vlen));
    result += base.substr(static_cast<size_t>(idx) + static_cast<size_t>(vlen));
    return result;
}

std::string nfc_compose(const std::string& nfd) {
    return nfd;
}

std::string nfd_decompose_char(uint32_t cp) {
    char buf[8];
    int n = utf8_encode(cp, buf);
    buf[n] = '\0';
    return std::string(buf);
}

static std::vector<uint32_t> to_codepoints(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size(); ) {
        uint32_t cp;
        int len;
        utf8_decode(s.c_str() + i, &cp, &len);
        out.push_back(cp);
        i += static_cast<size_t>(len);
    }
    return out;
}

void diff_commit(const std::string& old_str, const std::string& new_str, XKResult* r) {
    std::vector<uint32_t> a = to_codepoints(old_str);
    std::vector<uint32_t> b = to_codepoints(new_str);

    size_t common = 0;
    while (common < a.size() && common < b.size() && a[common] == b[common]) common++;

    r->delete_before = static_cast<int>(a.size() - common);

    std::string suffix;
    char buf[8];
    for (size_t i = common; i < b.size(); ++i) {
        int n = utf8_encode(b[i], buf);
        suffix.append(buf, static_cast<size_t>(n));
    }
    std::strncpy(r->commit, suffix.c_str(), sizeof(r->commit) - 1);
    r->commit[sizeof(r->commit) - 1] = '\0';
    r->consumed = 1;
}

} /* namespace xrnikey */

extern "C" {

char* xk_place_tone(const char* syllable, int tone) {
    std::string result = xrnikey::apply_tone_to_syllable(syllable, tone);
    result = xrnikey::nfc_compose(result);
    char* out = static_cast<char*>(std::malloc(result.size() + 1));
    if (out) std::strcpy(out, result.c_str());
    return out;
}

} /* extern "C" */

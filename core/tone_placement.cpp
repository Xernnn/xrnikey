#include "vietkey_internal.h"
#include <map>
#include <vector>
#include <cstdlib>
#include <cstring>

namespace vietkey {

/* Precomposed Vietnamese vowels with tone (NFC) */
static const char* tone_map_a[] = {
    "a", "\xC3\xA1", "\xC3\xA0", "\xC3\xA3", "\xC3\xA3", "\xC3\xA1" /* nga uses different */
};
/* Simplified: map base vowel + tone to precomposed */

struct VowelEntry {
    char base;
    const char* plain;
    const char* sac;
    const char* huyen;
    const char* hoi;
    const char* nga;
    const char* nang;
};

static const VowelEntry VOWELS[] = {
    {'a', "a", "\xC3\xA1", "\xC3\xA0", "\xC3\xA3", "\xC3\xA3", "\xE1\xBA\xA1"},
    {'e', "e", "\xC3\xA9", "\xC3\xA8", "\xC1\xBA\xBB", "\xE1\xBA\xBD", "\xE1\xBA\xB9"},
    {'i', "i", "\xC3\xAD", "\xC3\xAC", "\xC4\xA9", "\xC4\xA9", "\xE1\xBB\x8B"},
    {'o', "o", "\xC3\xB3", "\xC3\xB2", "\xC3\xB5", "\xC3\xB5", "\xE1\xBB\x8D"},
    {'u', "u", "\xC3\xBA", "\xC3\xB9", "\xC3\xB9", "\xC3\xB9", "\xE1\xBB\xB9"},
    {'y', "y", "\xC3\xBD", "\xC3\xBD", "\xE1\xBB\xB7", "\xE1\xBB\xB7", "\xE1\xBB\xB5"},
};

static const char* get_toned_vowel(char base, int tone) {
    char lb = static_cast<char>(base | 0x20);
    for (const auto& v : VOWELS) {
        if (v.base == lb) {
            switch (tone) {
                case VK_TONE_SAC:   return v.sac;
                case VK_TONE_HUYEN: return v.huyen;
                case VK_TONE_HOI:   return v.hoi;
                case VK_TONE_NGA:   return v.nga;
                case VK_TONE_NANG:  return v.nang;
                default: return v.plain;
            }
        }
    }
    return nullptr;
}

static bool is_special_vowel(const std::string& s, size_t pos, char* out_base) {
    if (pos + 1 < s.size()) {
        unsigned char b0 = static_cast<unsigned char>(s[pos]);
        unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
        if (b0 == 0xC3 && b1 == 0xA2) { *out_base = 'a'; return true; } /* â */
        if (b0 == 0xC3 && b1 == 0xAA) { *out_base = 'e'; return true; } /* ê */
        if (b0 == 0xC3 && b1 == 0xB4) { *out_base = 'o'; return true; } /* ô */
        if (b0 == 0xC6 && b1 == 0xA1) { *out_base = 'o'; return true; } /* ơ */
        if (b0 == 0xC4 && b1 == 0x83) { *out_base = 'a'; return true; } /* ă */
        if (b0 == 0xC6 && b1 == 0xB0) { *out_base = 'u'; return true; } /* ư */
    }
    if (pos < s.size()) {
        char c = static_cast<char>(s[pos] | 0x20);
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y') {
            *out_base = c;
            return true;
        }
    }
    return false;
}

static int find_tone_vowel_index(const std::string& syllable) {
    /* Rule 1: special vowels â ê ô ơ ă ư get tone */
    for (size_t i = 0; i < syllable.size(); ) {
        char base = 0;
        if (is_special_vowel(syllable, i, &base)) {
            if (base == 'a' && i + 1 < syllable.size()) {
                unsigned char b1 = static_cast<unsigned char>(syllable[i + 1]);
                if (b1 == 0xA2 || b1 == 0x83) return static_cast<int>(i);
            }
            if (base == 'e' || base == 'o') {
                if (i + 1 < syllable.size()) {
                    unsigned char b1 = static_cast<unsigned char>(syllable[i + 1]);
                    if (b1 == 0xAA || b1 == 0xB4 || b1 == 0xA1) return static_cast<int>(i);
                }
            }
            if (base == 'u' && i + 1 < syllable.size() &&
                static_cast<unsigned char>(syllable[i + 1]) == 0xB0)
                return static_cast<int>(i);
        }
        uint32_t cp;
        int len;
        utf8_decode(syllable.c_str() + i, &cp, &len);
        i += len;
    }

    /* Collect vowel positions */
    std::vector<int> positions;
    for (size_t i = 0; i < syllable.size(); ) {
        char base = 0;
        if (is_special_vowel(syllable, i, &base)) {
            positions.push_back(static_cast<int>(i));
            uint32_t cp;
            int len;
            utf8_decode(syllable.c_str() + i, &cp, &len);
            i += len;
        } else {
            i++;
        }
    }

    if (positions.empty()) return -1;
    if (positions.size() == 1) return positions[0];

    /* Rule 3/4: ia ua ưa */
    /* Simplified: tone on second vowel for diphthongs, last for ia/ua/ưa without coda */
    bool has_coda = false;
    if (!positions.empty()) {
        int last_pos = positions.back();
        uint32_t cp;
        int len;
        utf8_decode(syllable.c_str() + last_pos, &cp, &len);
        if (static_cast<size_t>(last_pos + len) < syllable.size()) has_coda = true;
    }

    if (positions.size() >= 2 && !has_coda) {
        return positions[1];
    }
    if (positions.size() >= 2 && has_coda) {
        return positions[0];
    }
    return positions[0];
}

std::string apply_tone_to_syllable(const std::string& base, int tone) {
    if (tone == VK_TONE_NONE || base.empty()) return base;

    int idx = find_tone_vowel_index(base);
    if (idx < 0) return base;

    char vowel_base = 0;
    is_special_vowel(base, static_cast<size_t>(idx), &vowel_base);

    uint32_t cp;
    int len;
    utf8_decode(base.c_str() + idx, &cp, &len);

    std::string result = base.substr(0, idx);
    const char* toned = get_toned_vowel(vowel_base, tone);
    if (toned) result += toned;
    result += base.substr(idx + len);
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

} /* namespace vietkey */

extern "C" {

char* vk_place_tone(const char* syllable, int tone) {
    std::string result = vietkey::apply_tone_to_syllable(syllable, tone);
    result = vietkey::nfc_compose(result);
    char* out = static_cast<char*>(std::malloc(result.size() + 1));
    if (out) std::strcpy(out, result.c_str());
    return out;
}

} /* extern "C" */

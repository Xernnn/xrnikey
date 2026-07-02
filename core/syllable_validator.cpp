#include "vietkey_internal.h"
#include <unordered_set>
#include <string>

namespace vietkey {

static const char* ONSETS[] = {
    "", "b", "c", "ch", "d", "đ", "g", "gh", "gi", "h", "k", "kh", "l",
    "m", "n", "ng", "ngh", "nh", "p", "ph", "qu", "r", "s", "t", "th",
    "tr", "v", "x"
};

static const char* NUCLEI[] = {
    "a", "ă", "â", "e", "ê", "i", "o", "ô", "ơ", "u", "ư", "y",
    "ai", "ao", "au", "ay", "âu", "ây", "eo", "ia", "iê", "iu",
    "oa", "oe", "oi", "oo", "ôa", "ôi", "ơi", "ua", "uâ", "uê",
    "ui", "uo", "ưa", "ưi", "ưu", "uy", "uya", "uyn", "yê"
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
        for (const char* nucleus : NUCLEI) {
            if (!starts_with(rest, nucleus)) continue;
            std::string tail = rest.substr(std::strlen(nucleus));
            for (const char* coda : CODAS) {
                if (tail == coda) return true;
            }
        }
    }
    return false;
}

bool is_valid_vn_syllable(const std::string& word) {
    if (word.empty() || word.size() > 7) return false;
    return match_syllable_structure(word);
}

} /* namespace vietkey */

extern "C" {

int vk_is_valid_syllable(const char* word) {
    if (!word) return 0;
    return vietkey::is_valid_vn_syllable(word) ? 1 : 0;
}

} /* extern "C" */

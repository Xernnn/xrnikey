#include "xrnikey_internal.h"
#include <unordered_set>
#include <string>
#include <vector>

namespace xrnikey {

/* A Vietnamese syllable is onset + rime + tone. The tables below are the
 * spelling rules of modern quốc ngữ; every valid toneless syllable is built
 * from them once, and a trie over those syllables answers both "is this a
 * syllable" and "can this still become one" (the question asked on every
 * keystroke). The old check accepted ANY run of one to three vowels as a
 * nucleus, so "ue", "ou" or "oe" after any consonant passed, and nothing knew
 * that a stop coda (c ch p t) only takes the sắc or nặng tone -- which is how
 * "user" became "ủe" and "text" became "tẽt". */

static const char* const ONSETS[] = {
    "", "b", "c", "ch", "d", "đ", "g", "gh", "gi", "h", "k", "kh", "l", "m",
    "n", "ng", "ngh", "nh", "p", "ph", "qu", "r", "s", "t", "th", "tr", "v", "x",
};

/* Every rime (vowels plus final consonant). Rimes that begin with the glide
 * o/u ("medial") are listed after the others. */
static const char* const RIMES[] = {
    "a", "ac", "ach", "ai", "am", "an", "ang", "anh", "ao", "ap", "at", "au", "ay",
    "ăc", "ăm", "ăn", "ăng", "ăp", "ăt",
    "âc", "âm", "ân", "âng", "âp", "ât", "âu", "ây",
    "e", "ec", "em", "en", "eng", "eo", "ep", "et",
    "ê", "êch", "êm", "ên", "ênh", "êp", "êt", "êu",
    "i", "ia", "ich", "im", "in", "inh", "ip", "it", "iu",
    "iêc", "iêm", "iên", "iêng", "iêp", "iêt", "iêu",
    "y", "yêm", "yên", "yêng", "yêt", "yêu",
    "o", "oc", "oi", "om", "on", "ong", "op", "ot",
    "ô", "ôc", "ôi", "ôm", "ôn", "ông", "ôp", "ôt",
    "ơ", "ơi", "ơm", "ơn", "ơp", "ơt",
    "u", "ua", "uc", "ui", "um", "un", "ung", "up", "ut",
    "uôc", "uôi", "uôm", "uôn", "uông", "uôt",
    "ư", "ưa", "ưc", "ưi", "ưm", "ưng", "ưt", "ưu",
    "ươ", "ươc", "ươi", "ươm", "ươn", "ương", "ươp", "ươt", "ươu",
    /* medial o / u */
    "oa", "oac", "oach", "oai", "oam", "oan", "oang", "oanh", "oap", "oat", "oay",
    "oăc", "oăm", "oăn", "oăng", "oăt",
    "oe", "oen", "oeo", "oet",
    "ooc", "oong",
    "uân", "uâng", "uât", "uây",
    "uê", "uêch", "uênh",
    "uơ",
    "uy", "uya", "uych", "uyên", "uyêt", "uynh", "uyt", "uyu",
};

std::string utf8_of(const std::u32string& s) {
    std::string out;
    char buf[8];
    for (char32_t c : s) {
        int n = utf8_encode(static_cast<uint32_t>(c), buf);
        out.append(buf, static_cast<size_t>(n));
    }
    return out;
}

static std::u32string u32_of(const char* s) {
    std::u32string out;
    for (size_t i = 0; s[i];) {
        uint32_t cp = 0;
        int len = 1;
        utf8_decode(s + i, &cp, &len);
        out.push_back(static_cast<char32_t>(cp));
        i += static_cast<size_t>(len);
    }
    return out;
}

static bool starts_with(const std::u32string& s, const char* prefix) {
    std::u32string p = u32_of(prefix);
    return s.compare(0, p.size(), p) == 0;
}

static bool is_medial_rime(const std::u32string& r) {
    if (r.size() < 2) return false;
    if (r[0] == U'o') return r[1] == U'a' || r[1] == U'ă' || r[1] == U'e' || r[1] == U'o';
    if (r[0] == U'u') return r[1] == U'â' || r[1] == U'ê' || r[1] == U'y' || r[1] == U'ơ';
    return false;
}

/* The spelling rules that tie an onset to the rime after it. */
static bool onset_takes(const std::string& on, const std::u32string& r) {
    const char32_t v = r[0];
    const bool front = v == U'e' || v == U'ê' || v == U'i' || v == U'y';
    const bool medial = is_medial_rime(r);
    if (starts_with(r, "yê")) return on.empty() || on == "qu";  /* yên, quyết */
    if (starts_with(r, "iê") && on.empty()) return false;       /* spelled yê */
    if (starts_with(r, "uơ")) return on == "h" || on == "kh" || on == "th";  /* huơ, thuở */
    if (r == U"ươ") return on.empty();
    if (on == "c") return !front && !medial;                     /* ke, ki, quy */
    if (on == "k") return front;
    if (on == "gh" || on == "ngh") return v == U'e' || v == U'ê' || v == U'i';
    if (on == "g") return v != U'e' && v != U'ê';                /* gi- merges: gì, gìn */
    if (on == "ng") return !front || v == U'y';
    if (on == "qu") {
        return !medial && v != U'u' && v != U'ư' && v != U'o';   /* qua, quê, quốc, quơ */
    }
    if (on == "gi") return v != U'i' && v != U'y' && !medial;
    return true;
}

static bool is_stop_end(const std::u32string& s) {
    if (s.empty()) return false;
    char32_t c = s.back();
    return c == U'c' || c == U'p' || c == U't' ||
           (c == U'h' && s.size() >= 2 && s[s.size() - 2] == U'c');
}

namespace {

struct Node {
    std::vector<std::pair<char32_t, int>> next;
    bool terminal = false;
    bool reach_open = false;  /* some syllable through here ends without a stop */
};

struct Syllables {
    std::unordered_set<std::u32string> all;
    std::vector<Node> trie;

    Syllables() {
        trie.emplace_back();
        for (const char* o : ONSETS) {
            for (const char* rm : RIMES) {
                std::u32string r = u32_of(rm);
                if (!onset_takes(o, r)) continue;
                std::u32string s = u32_of(o) + r;
                if (all.insert(s).second) add(s);
            }
        }
    }

    int child(int n, char32_t c) const {
        for (const auto& e : trie[static_cast<size_t>(n)].next)
            if (e.first == c) return e.second;
        return -1;
    }

    void add(const std::u32string& s) {
        bool open = !is_stop_end(s);
        int n = 0;
        if (open) trie[0].reach_open = true;
        for (char32_t c : s) {
            int k = child(n, c);
            if (k < 0) {
                k = static_cast<int>(trie.size());
                trie[static_cast<size_t>(n)].next.push_back({c, k});
                trie.emplace_back();
            }
            n = k;
            if (open) trie[static_cast<size_t>(n)].reach_open = true;
        }
        trie[static_cast<size_t>(n)].terminal = true;
    }
};

const Syllables& syllables() {
    static const Syllables s;
    return s;
}

/* Letters a later key can still mark: a -> â ă, e -> ê, o -> ô ơ, u -> ư, d -> đ. */
bool markable_to(char32_t typed, char32_t target) {
    switch (typed) {
        case U'a': return target == U'â' || target == U'ă';
        case U'e': return target == U'ê';
        case U'o': return target == U'ô' || target == U'ơ';
        case U'u': return target == U'ư';
        case U'd': return target == U'đ';
        default: return false;
    }
}

bool prefix_walk(const Syllables& sy, int n, const std::u32string& w, size_t i, bool need_open) {
    if (i == w.size()) {
        return need_open ? sy.trie[static_cast<size_t>(n)].reach_open : true;
    }
    for (const auto& e : sy.trie[static_cast<size_t>(n)].next) {
        if (e.first == w[i] || markable_to(w[i], e.first)) {
            if (prefix_walk(sy, e.second, w, i + 1, need_open)) return true;
        }
    }
    return false;
}

bool tone_needs_open(int tone) {
    return tone == XK_TONE_HUYEN || tone == XK_TONE_HOI || tone == XK_TONE_NGA;
}

}  // namespace

bool syllable_valid(const std::u32string& letters, int tone) {
    if (!syllables().all.count(letters)) return false;
    return !(tone_needs_open(tone) && is_stop_end(letters));
}

bool syllable_prefix_ok(const std::u32string& letters, int tone) {
    return prefix_walk(syllables(), 0, letters, 0, tone_needs_open(tone));
}

/* Lowercase form and tone of one Vietnamese letter; false if `cp` is not one. */
static bool split_letter(uint32_t cp, char32_t* letter, int* tone) {
    static const struct { const char* row; char32_t base; } ROWS[] = {
        /* plain, sắc, huyền, hỏi, ngã, nặng; lowercase then uppercase */
        {"aáàảãạAÁÀẢÃẠ", U'a'}, {"ăắằẳẵặĂẮẰẲẴẶ", U'ă'}, {"âấầẩẫậÂẤẦẨẪẬ", U'â'},
        {"eéèẻẽẹEÉÈẺẼẸ", U'e'}, {"êếềểễệÊẾỀỂỄỆ", U'ê'}, {"iíìỉĩịIÍÌỈĨỊ", U'i'},
        {"oóòỏõọOÓÒỎÕỌ", U'o'}, {"ôốồổỗộÔỐỒỔỖỘ", U'ô'}, {"ơớờởỡợƠỚỜỞỠỢ", U'ơ'},
        {"uúùủũụUÚÙỦŨỤ", U'u'}, {"ưứừửữựƯỨỪỬỮỰ", U'ư'}, {"yýỳỷỹỵYÝỲỶỸỴ", U'y'},
    };
    static const int TONE_OF[6] = {XK_TONE_NONE, XK_TONE_SAC, XK_TONE_HUYEN,
                                   XK_TONE_HOI, XK_TONE_NGA, XK_TONE_NANG};
    for (const auto& r : ROWS) {
        std::u32string row = u32_of(r.row);
        for (size_t k = 0; k < row.size(); ++k) {
            if (row[k] == cp) {
                *letter = r.base;
                *tone = TONE_OF[k % 6];
                return true;
            }
        }
    }
    if (cp == 0x0111 || cp == 0x0110) { *letter = U'đ'; *tone = XK_TONE_NONE; return true; }
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')) {
        *letter = static_cast<char32_t>(cp | 0x20);
        *tone = XK_TONE_NONE;
        return true;
    }
    return false;
}

bool syllable_split(const std::string& utf8, std::u32string* letters, int* tone) {
    letters->clear();
    *tone = XK_TONE_NONE;
    for (size_t i = 0; i < utf8.size();) {
        uint32_t cp = 0;
        int len = 1;
        if (utf8_decode(utf8.c_str() + i, &cp, &len) != 0) return false;
        i += static_cast<size_t>(len);
        char32_t l;
        int t;
        if (!split_letter(cp, &l, &t)) return false;
        if (t != XK_TONE_NONE) {
            if (*tone != XK_TONE_NONE) return false;  /* two tones */
            *tone = t;
        }
        letters->push_back(l);
    }
    return true;
}

bool is_valid_vn_syllable(const std::string& word) {
    std::u32string letters;
    int tone;
    if (word.empty() || !syllable_split(word, &letters, &tone)) return false;
    return syllable_valid(letters, tone);
}

} /* namespace xrnikey */

extern "C" {

int xk_is_valid_syllable(const char* word) {
    if (!word) return 0;
    return xrnikey::is_valid_vn_syllable(word) ? 1 : 0;
}

} /* extern "C" */

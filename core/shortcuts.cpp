/* Shortcuts ("gõ tắt", the laptop's macros): an abbreviation that expands
 * when its word ends, "ko" -> "không". Moved here from the Fcitx5 addon so
 * the phone and the laptop match abbreviations and carry case the same way.
 *
 * Abbreviations match without case; the expansion takes the case of what
 * was typed when asked: "ko" -> as written, "Ko" -> first letter capital,
 * "KO" -> all capitals.
 *
 * The table's file is the laptop's macros.txt: one "abbreviation<TAB>
 * expansion" per line, blank lines and lines starting with # ignored. An
 * import also takes a table someone made elsewhere: "abbr,expansion" (CSV,
 * quoted fields allowed) or "abbr=expansion", whichever separator comes
 * first on the line. */
#include "xrnkey.h"
#include "xrnkey_internal.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace xrnkey {

namespace {

constexpr size_t kMaxAbbrev = 32;
constexpr size_t kMaxExpansion = 1024;
constexpr size_t kMaxEntries = 20000;

char32_t vn_lower(char32_t c) {
    if (c >= U'A' && c <= U'Z') return c + 0x20;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;
    switch (c) {
        case 0x0102: return 0x0103;  /* Ă */
        case 0x0110: return 0x0111;  /* Đ */
        case 0x0128: return 0x0129;  /* Ĩ */
        case 0x0168: return 0x0169;  /* Ũ */
        case 0x01A0: return 0x01A1;  /* Ơ */
        case 0x01AF: return 0x01B0;  /* Ư */
        default: break;
    }
    if (c >= 0x1EA0 && c <= 0x1EF9 && !(c & 1)) return c + 1;
    return c;
}

/* Whether `s` is well-formed UTF-8 with no control characters. */
bool clean_utf8(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        int len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (len == 0 || i + static_cast<size_t>(len) > s.size()) return false;
        if (len == 1 && (c < 0x20 || c == 0x7F)) return false;
        for (int k = 1; k < len; ++k)
            if ((static_cast<unsigned char>(s[i + static_cast<size_t>(k)]) & 0xC0) != 0x80) return false;
        i += static_cast<size_t>(len);
    }
    return true;
}

/* `s` with each letter mapped, the first `limit` of them only. */
template <typename F>
std::string map_letters(const std::string& s, F f, size_t limit = std::string::npos) {
    std::string out;
    char buf[8];
    size_t n = 0;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp;
        int len;
        utf8_decode(s.c_str() + i, &cp, &len);
        if (n++ < limit) {
            const int k = utf8_encode(static_cast<uint32_t>(f(static_cast<char32_t>(cp))), buf);
            out.append(buf, static_cast<size_t>(k));
        } else {
            out.append(s, i, static_cast<size_t>(len));
        }
        i += static_cast<size_t>(len);
    }
    return out;
}

std::string lower(const std::string& s) { return map_letters(s, vn_lower); }
std::string upper(const std::string& s) { return map_letters(s, vn_upper); }

size_t letters(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

/* One CSV field from `p`, quoted ("a ""b""") or not; `p` moves past it and
 * its comma. False if a quote is left open. */
bool csv_field(const std::string& line, size_t& p, std::string* out) {
    out->clear();
    while (p < line.size() && line[p] == ' ') ++p;
    if (p < line.size() && line[p] == '"') {
        ++p;
        for (;;) {
            if (p >= line.size()) return false;
            if (line[p] == '"') {
                if (p + 1 < line.size() && line[p + 1] == '"') {
                    out->push_back('"');
                    p += 2;
                    continue;
                }
                ++p;
                break;
            }
            out->push_back(line[p++]);
        }
        while (p < line.size() && line[p] != ',') {
            if (line[p] != ' ') return false;  /* text after a closing quote */
            ++p;
        }
    } else {
        while (p < line.size() && line[p] != ',') out->push_back(line[p++]);
        *out = trim(*out);
    }
    if (p < line.size()) ++p;  /* the comma */
    return true;
}

/* An entry from one line in any of the formats; false if the line is not
 * one (blank and # lines are not errors: `blank` says so). */
bool parse_line(std::string line, std::string* abbr, std::string* expansion, bool* blank) {
    *blank = false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (trim(line).empty() || line[0] == '#') {
        *blank = true;
        return false;
    }
    const size_t tab = line.find('\t');
    const size_t comma = line.find(',');
    const size_t eq = line.find('=');
    if (tab != std::string::npos) {
        *abbr = line.substr(0, tab);
        *expansion = line.substr(tab + 1);
    } else if (line[0] == '"' || (comma != std::string::npos && (eq == std::string::npos || comma < eq))) {
        size_t p = 0;
        if (!csv_field(line, p, abbr) || !csv_field(line, p, expansion)) return false;
        if (p < line.size()) return false;  /* a third field */
    } else if (eq != std::string::npos) {
        *abbr = trim(line.substr(0, eq));
        *expansion = trim(line.substr(eq + 1));
    } else {
        return false;
    }
    if (abbr->empty() || abbr->size() > kMaxAbbrev || expansion->empty() ||
        expansion->size() > kMaxExpansion)
        return false;
    if (abbr->find(' ') != std::string::npos) return false;  /* typed as one word */
    return clean_utf8(*abbr) && clean_utf8(*expansion);
}

}  // namespace

struct Shortcuts {
    std::map<std::string, std::string> table;  /* lowercase abbreviation -> expansion */
};

}  // namespace xrnkey

using xrnkey::Shortcuts;

extern "C" {

XKShortcuts* xk_shortcuts_new(void) { return reinterpret_cast<XKShortcuts*>(new Shortcuts); }

void xk_shortcuts_free(XKShortcuts* sc) { delete reinterpret_cast<Shortcuts*>(sc); }

int xk_shortcuts_import(XKShortcuts* sc, const char* text, int apply, XKShortcutCounts* counts) {
    if (!sc || !text) return -1;
    auto* s = reinterpret_cast<Shortcuts*>(sc);
    XKShortcutCounts n{};
    std::map<std::string, std::string> seen;  /* this import's entries, last one wins */
    std::string body = text;
    if (body.compare(0, 3, "\xEF\xBB\xBF") == 0) body.erase(0, 3);  /* a BOM */
    std::istringstream in(body);
    std::string line, abbr, expansion;
    bool blank;
    while (std::getline(in, line)) {
        if (!xrnkey::parse_line(line, &abbr, &expansion, &blank)) {
            if (!blank) ++n.skipped;
            continue;
        }
        seen[xrnkey::lower(abbr)] = expansion;
    }
    for (const auto& [key, exp] : seen) {
        auto it = s->table.find(key);
        if (it == s->table.end()) ++n.added;
        else if (it->second == exp) ++n.same;
        else ++n.changed;
    }
    if (apply)
        for (const auto& [key, exp] : seen)
            if (s->table.size() < xrnkey::kMaxEntries || s->table.count(key)) s->table[key] = exp;
    if (counts) *counts = n;
    return n.added + n.same + n.changed;
}

int xk_shortcuts_load(XKShortcuts* sc, const char* path) {
    if (!sc || !path) return -1;
    std::ifstream f(path, std::ios::binary);
    if (!f) return -1;
    std::ostringstream all;
    all << f.rdbuf();
    auto* s = reinterpret_cast<Shortcuts*>(sc);
    s->table.clear();
    /* The file is the TAB format; other lines are skipped, as the addon's
     * old reader did. */
    std::istringstream in(all.str());
    std::string line;
    std::string text;
    while (std::getline(in, line)) {
        std::string l = line;
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.find('\t') == std::string::npos) continue;
        text += l;
        text += '\n';
    }
    return xk_shortcuts_import(sc, text.c_str(), 1, nullptr);
}

int xk_shortcuts_save(const XKShortcuts* sc, const char* path) {
    if (!sc || !path) return -1;
    const auto* s = reinterpret_cast<const Shortcuts*>(sc);
    const std::string tmp = std::string(path) + ".part";
    {
        std::ofstream f(tmp, std::ios::trunc | std::ios::binary);
        if (!f) return -1;
        for (const auto& [abbr, exp] : s->table) f << abbr << '\t' << exp << '\n';
        f.flush();
        if (!f) {
            std::remove(tmp.c_str());
            return -1;
        }
    }
    if (std::rename(tmp.c_str(), path) != 0) {
        std::remove(tmp.c_str());
        return -1;
    }
    return 0;
}

int xk_shortcuts_set(XKShortcuts* sc, const char* abbr, const char* expansion) {
    if (!sc || !abbr || !expansion) return 0;
    std::string line = std::string(abbr) + "\t" + expansion;
    std::string a, e;
    bool blank;
    if (!xrnkey::parse_line(line, &a, &e, &blank)) return 0;
    auto* s = reinterpret_cast<Shortcuts*>(sc);
    if (s->table.size() >= xrnkey::kMaxEntries && !s->table.count(xrnkey::lower(a))) return 0;
    s->table[xrnkey::lower(a)] = e;
    return 1;
}

void xk_shortcuts_remove(XKShortcuts* sc, const char* abbr) {
    if (!sc || !abbr) return;
    reinterpret_cast<Shortcuts*>(sc)->table.erase(xrnkey::lower(abbr));
}

int xk_shortcuts_count(const XKShortcuts* sc) {
    return sc ? static_cast<int>(reinterpret_cast<const Shortcuts*>(sc)->table.size()) : 0;
}

int xk_shortcuts_at(const XKShortcuts* sc, int i, const char** abbr, const char** expansion) {
    if (!sc || i < 0) return 0;
    const auto& t = reinterpret_cast<const Shortcuts*>(sc)->table;
    if (static_cast<size_t>(i) >= t.size()) return 0;
    auto it = t.begin();
    std::advance(it, i);
    if (abbr) *abbr = it->first.c_str();
    if (expansion) *expansion = it->second.c_str();
    return 1;
}

char* xk_shortcuts_expand(const XKShortcuts* sc, const char* typed, int match_case) {
    if (!sc || !typed || !*typed) return nullptr;
    const auto& t = reinterpret_cast<const Shortcuts*>(sc)->table;
    const std::string word = typed;
    if (!xrnkey::clean_utf8(word)) return nullptr;
    auto it = t.find(xrnkey::lower(word));
    if (it == t.end()) return nullptr;
    std::string out = it->second;
    const bool has_upper = xrnkey::lower(word) != word;
    const bool has_lower = xrnkey::upper(word) != word;
    if (match_case && has_upper) {
        /* Two or more letters, all capitals: "KO" -> "KHÔNG"; otherwise a
         * capital first letter: "Ko" -> "Không". */
        if (!has_lower && xrnkey::letters(word) > 1) out = xrnkey::upper(out);
        else out = xrnkey::map_letters(out, xrnkey::vn_upper, 1);
    }
    char* r = static_cast<char*>(std::malloc(out.size() + 1));
    if (r) std::memcpy(r, out.c_str(), out.size() + 1);
    return r;
}

}  /* extern "C" */

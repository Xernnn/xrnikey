#include "macro_table.h"

#include <fcitx-utils/utf8.h>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>

namespace fcitx {

namespace {

uint32_t upperCp(uint32_t c) {
    if (c >= 'a' && c <= 'z') return c - 0x20;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20;
    switch (c) {
    case 0x0103: return 0x0102; /* ă */
    case 0x0111: return 0x0110; /* đ */
    case 0x0129: return 0x0128; /* ĩ */
    case 0x0169: return 0x0168; /* ũ */
    case 0x01A1: return 0x01A0; /* ơ */
    case 0x01B0: return 0x01AF; /* ư */
    default: break;
    }
    if (c >= 0x1EA0 && c <= 0x1EF9 && (c & 1)) return c - 1;
    return c;
}

uint32_t lowerCp(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return c + 0x20;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;
    switch (c) {
    case 0x0102: return 0x0103;
    case 0x0110: return 0x0111;
    case 0x0128: return 0x0129;
    case 0x0168: return 0x0169;
    case 0x01A0: return 0x01A1;
    case 0x01AF: return 0x01B0;
    default: break;
    }
    if (c >= 0x1EA0 && c <= 0x1EF9 && !(c & 1)) return c + 1;
    return c;
}

template <typename F>
std::string mapChars(const std::string &s, F f, size_t limit = std::string::npos) {
    std::string out;
    size_t n = 0;
    for (auto iter = s.begin(); iter != s.end();) {
        uint32_t c = 0;
        auto next = utf8::getNextChar(iter, s.end(), &c);
        if (!utf8::isValidChar(c)) {
            out.append(iter, s.end());
            break;
        }
        if (n++ < limit) {
            out += utf8::UCS4ToUTF8(f(c));
        } else {
            out.append(iter, next);
        }
        iter = next;
    }
    return out;
}

bool hasLower(const std::string &s) { return toUpperVi(s) != s; }
bool hasUpper(const std::string &s) { return toLowerVi(s) != s; }

} // namespace

std::string toUpperVi(const std::string &s) { return mapChars(s, upperCp); }
std::string toLowerVi(const std::string &s) { return mapChars(s, lowerCp); }

std::string MacroTable::defaultPath() {
    const char *xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg == '/') return std::string(xdg) + "/xrnkey/macros.txt";
    const char *home = std::getenv("HOME");
    return std::string(home ? home : "") + "/.local/share/xrnkey/macros.txt";
}

MacroTable::MacroTable(std::string path) : path_(std::move(path)) { refresh(); }

void MacroTable::refresh() {
    struct stat st;
    if (stat(path_.c_str(), &st) != 0) {
        table_.clear();
        exists_ = false;
        return;
    }
    if (exists_ && st.st_mtim.tv_sec == mtime_.tv_sec &&
        st.st_mtim.tv_nsec == mtime_.tv_nsec && st.st_size == size_ &&
        st.st_ino == inode_) {
        return;
    }
    exists_ = true;
    mtime_ = st.st_mtim;
    size_ = st.st_size;
    inode_ = st.st_ino;
    table_.clear();
    std::ifstream in(path_);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == 0 || tab == std::string::npos) continue;
        std::string expansion = line.substr(tab + 1);
        if (expansion.empty() || !utf8::validate(line)) continue;
        table_[toLowerVi(line.substr(0, tab))] = std::move(expansion);
    }
}

std::optional<std::string> MacroTable::expand(const std::string &typed,
                                              bool matchCase) const {
    auto iter = table_.find(toLowerVi(typed));
    if (iter == table_.end()) return std::nullopt;
    const std::string &expansion = iter->second;
    if (!matchCase || !hasUpper(typed)) return expansion;
    /* Two or more letters, all capitals: "KO" -> "KHÔNG". */
    if (!hasLower(typed) && utf8::length(typed) > 1) return toUpperVi(expansion);
    /* Otherwise a capital first letter: "Ko" -> "Không". */
    return mapChars(expansion, upperCp, 1);
}

} // namespace fcitx

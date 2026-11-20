#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <unordered_map>

namespace fcitx {

/* The user's abbreviations (Bamboo's "macros"): one per line in
 * $XDG_DATA_HOME/xrnikey/macros.txt as `abbreviation<TAB>expansion`, blank
 * lines and lines starting with # ignored. Abbreviations match without
 * case. The file is re-read whenever it changes, so `xk-macro add` takes
 * effect from the next word. */
class MacroTable {
public:
    static std::string defaultPath();

    explicit MacroTable(std::string path = defaultPath());

    /* Re-read the file if it changed since the last look. */
    void refresh();
    /* The expansion for `typed`, cased like it when `matchCase`:
     * "ko" -> as written, "Ko" -> first letter capitalised, "KO" -> all
     * capitals. */
    std::optional<std::string> expand(const std::string &typed,
                                      bool matchCase) const;
    bool empty() const { return table_.empty(); }

private:
    std::string path_;
    std::unordered_map<std::string, std::string> table_;
    bool exists_ = false;
    timespec mtime_{};
    off_t size_ = -1;
    ino_t inode_ = 0;
};

/* Vietnamese-aware case mapping of UTF-8 text (ASCII, Latin-1, the letters
 * ă đ ĩ ũ ơ ư and U+1EA0-U+1EF9). */
std::string toUpperVi(const std::string &s);
std::string toLowerVi(const std::string &s);

} // namespace fcitx

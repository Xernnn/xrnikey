#pragma once

#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>
#include <xrnikey.h>

namespace fcitx {

/* The user's abbreviations (Bamboo's "macros"): one per line in
 * $XDG_DATA_HOME/xrnikey/macros.txt as `abbreviation<TAB>expansion`, blank
 * lines and lines starting with # ignored. Abbreviations match without
 * case. The file is re-read whenever it changes, so `xk-macro add` takes
 * effect from the next word. Reading and matching are the core's
 * (xk_shortcuts_*), so the phone does the same. */
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
    bool empty() const;

private:
    std::string path_;
    std::unique_ptr<XKShortcuts, decltype(&xk_shortcuts_free)> table_;
    bool exists_ = false;
    timespec mtime_{};
    off_t size_ = -1;
    ino_t inode_ = 0;
};

} // namespace fcitx

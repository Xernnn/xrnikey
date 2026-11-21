#include "macro_table.h"
#include "user_paths.h"

#include <cstdlib>
#include <sys/stat.h>

namespace fcitx {

/* Parsing, matching and case are the core's (xk_shortcuts_*), shared with
 * the phone; this class only finds the file and notices when it changes. */

std::string MacroTable::defaultPath() { return userDataDir() + "/macros.txt"; }

MacroTable::MacroTable(std::string path)
    : path_(std::move(path)), table_(xk_shortcuts_new(), &xk_shortcuts_free) {
    refresh();
}

void MacroTable::refresh() {
    struct stat st;
    if (stat(path_.c_str(), &st) != 0) {
        table_.reset(xk_shortcuts_new());
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
    if (xk_shortcuts_load(table_.get(), path_.c_str()) < 0) table_.reset(xk_shortcuts_new());
}

bool MacroTable::empty() const { return xk_shortcuts_count(table_.get()) == 0; }

std::optional<std::string> MacroTable::expand(const std::string &typed,
                                              bool matchCase) const {
    char *r = xk_shortcuts_expand(table_.get(), typed.c_str(), matchCase ? 1 : 0);
    if (!r) return std::nullopt;
    std::string out = r;
    std::free(r);
    return out;
}

} // namespace fcitx

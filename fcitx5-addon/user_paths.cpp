#include "user_paths.h"

#include <cstdlib>
#include <system_error>

namespace fcitx {

namespace fs = std::filesystem;

namespace {

/* Exists as anything, a dangling symlink included. */
bool present(const fs::path &p) {
    std::error_code ec;
    return fs::exists(fs::symlink_status(p, ec));
}

} // namespace

fs::path adoptOldPath(const fs::path &oldPath, const fs::path &newPath) {
    if (present(newPath) || !present(oldPath)) return newPath;
    /* Both sit in the same folder, so this is a rename(2): atomic, and the
     * same for a file, a folder or a symlink to either. */
    std::error_code ec;
    fs::rename(oldPath, newPath, ec);
    /* Another process may have moved it first. */
    return ec && !present(newPath) ? oldPath : newPath;
}

std::string userDataDir() {
    fs::path base;
    const char *xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg == '/') {
        base = xdg;
    } else {
        const char *home = std::getenv("HOME");
        base = std::string(home ? home : "") + "/.local/share";
    }
    return adoptOldPath(base / "xrnikey", base / "xrnkey").string();
}

} // namespace fcitx

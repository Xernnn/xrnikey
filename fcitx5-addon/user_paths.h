#pragma once

#include <filesystem>
#include <string>

namespace fcitx {

/* Until 1.1.0 the project was called xrnikey, and so were the user's files:
 * ~/.local/share/xrnikey and ~/.config/fcitx5/conf/xrnikey5.conf. */

/* Moves `oldPath` to `newPath` when only the old one exists. Returns the
 * path to use: `newPath`, or `oldPath` if it is still the only one there
 * (the move failed). When both exist, the old one is left alone. */
std::filesystem::path adoptOldPath(const std::filesystem::path &oldPath,
                                   const std::filesystem::path &newPath);

/* The folder of the user's own files (user dictionary, abbreviations):
 * $XDG_DATA_HOME/xrnkey, or ~/.local/share/xrnkey when XDG_DATA_HOME is not
 * an absolute path. The first call after the rename moves the xrnikey
 * folder there. */
std::string userDataDir();

} // namespace fcitx

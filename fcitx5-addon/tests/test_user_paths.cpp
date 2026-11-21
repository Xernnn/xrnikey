/* The user's files from before the rename (xrnikey -> xrnkey) are moved to
 * the new names the first time they are looked for, and nothing else is
 * touched. Every path is under a throwaway directory. */
#include "user_paths.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using fcitx::adoptOldPath;
using fcitx::userDataDir;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        std::string tmpl = std::string(XRNKEY_TEST_TMP) + "/paths-XXXXXX";
        REQUIRE(mkdtemp(tmpl.data()));
        path = tmpl;
    }
    ~TempDir() {
        std::error_code ec;
        fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, ec);
        for (const auto &e : fs::recursive_directory_iterator(path, ec)) {
            fs::permissions(e.path(), fs::perms::owner_all,
                            fs::perm_options::add, ec);
        }
        fs::remove_all(path, ec);
    }
};

void write(const fs::path &p, const std::string &text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
}

std::string read(const fs::path &p) {
    std::ifstream f(p);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

} // namespace

TEST_CASE("the old data folder is moved to the new name", "[paths]") {
    TempDir t;
    write(t.path / "xrnikey/macros.txt", "ko\tkhông\n");
    write(t.path / "xrnikey/user-dict.txt", "từ mới\t9000\n");
    setenv("XDG_DATA_HOME", t.path.c_str(), 1);

    CHECK(userDataDir() == (t.path / "xrnkey").string());
    CHECK(read(t.path / "xrnkey/macros.txt") == "ko\tkhông\n");
    CHECK(read(t.path / "xrnkey/user-dict.txt") == "từ mới\t9000\n");
    CHECK_FALSE(fs::exists(t.path / "xrnikey"));
    /* Once: the next look finds it under the new name. */
    CHECK(userDataDir() == (t.path / "xrnkey").string());
}

TEST_CASE("without XDG_DATA_HOME the folder is in ~/.local/share", "[paths]") {
    TempDir t;
    write(t.path / ".local/share/xrnikey/macros.txt", "vn\tViệt Nam\n");
    setenv("HOME", t.path.c_str(), 1);
    setenv("XDG_DATA_HOME", "relative/is/ignored", 1);

    CHECK(userDataDir() == (t.path / ".local/share/xrnkey").string());
    CHECK(read(t.path / ".local/share/xrnkey/macros.txt") == "vn\tViệt Nam\n");
    CHECK_FALSE(fs::exists(t.path / ".local/share/xrnikey"));
}

TEST_CASE("when both names exist the new one wins and the old one stays",
          "[paths]") {
    TempDir t;
    write(t.path / "xrnikey/macros.txt", "ko\tkhông\n");
    write(t.path / "xrnkey/macros.txt", "dc\tđược\n");
    setenv("XDG_DATA_HOME", t.path.c_str(), 1);

    CHECK(userDataDir() == (t.path / "xrnkey").string());
    CHECK(read(t.path / "xrnkey/macros.txt") == "dc\tđược\n");
    CHECK(read(t.path / "xrnikey/macros.txt") == "ko\tkhông\n");
}

TEST_CASE("with neither name nothing is created", "[paths]") {
    TempDir t;
    setenv("XDG_DATA_HOME", t.path.c_str(), 1);

    CHECK(userDataDir() == (t.path / "xrnkey").string());
    CHECK(fs::is_empty(t.path));
}

TEST_CASE("an old settings file is moved, or read where it is if it can't be",
          "[paths]") {
    TempDir t;
    const fs::path conf = t.path / "fcitx5/conf";
    write(conf / "xrnikey5.conf", "Macro=True\n");

    CHECK(adoptOldPath(conf / "xrnikey5.conf", conf / "xrnkey.conf") ==
          conf / "xrnkey.conf");
    CHECK(read(conf / "xrnkey.conf") == "Macro=True\n");
    CHECK_FALSE(fs::exists(conf / "xrnikey5.conf"));

    if (geteuid() == 0) {
        SKIP("root may rename in a read-only folder");
    }
    const fs::path locked = t.path / "locked";
    write(locked / "xrnikey5.conf", "Macro=True\n");
    fs::permissions(locked, fs::perms::owner_read | fs::perms::owner_exec);
    CHECK(adoptOldPath(locked / "xrnikey5.conf", locked / "xrnkey.conf") ==
          locked / "xrnikey5.conf");
    CHECK(read(locked / "xrnikey5.conf") == "Macro=True\n");
}

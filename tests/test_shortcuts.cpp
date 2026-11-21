// Shortcuts in the core (the laptop's macros, the phone's "gõ tắt"), with
// made-up tables only: the owner's own table is imported on their device.
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "xrnikey.h"

namespace {

std::string expand(XKShortcuts* sc, const char* typed, bool match_case = true) {
    char* r = xk_shortcuts_expand(sc, typed, match_case ? 1 : 0);
    std::string out = r ? r : "<none>";
    std::free(r);
    return out;
}

std::string tmp(const char* name) { return std::string(XRNIKEY_TEST_TMP) + "/" + name; }

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

}  // namespace

TEST_CASE("an abbreviation expands with the case it was typed in", "[shortcuts]") {
    XKShortcuts* sc = xk_shortcuts_new();
    REQUIRE(xk_shortcuts_set(sc, "ko", "không"));
    REQUIRE(xk_shortcuts_set(sc, "vn", "Việt Nam"));
    REQUIRE(xk_shortcuts_set(sc, "DC", "được"));  // abbreviations match without case
    CHECK(expand(sc, "ko") == "không");
    CHECK(expand(sc, "Ko") == "Không");
    CHECK(expand(sc, "KO") == "KHÔNG");
    CHECK(expand(sc, "kO") == "Không");  // not all capitals: the first letter only
    CHECK(expand(sc, "vn") == "Việt Nam");  // written capitals stay
    CHECK(expand(sc, "VN") == "VIỆT NAM");
    CHECK(expand(sc, "dc") == "được");
    CHECK(expand(sc, "Ko", false) == "không");
    CHECK(expand(sc, "kong") == "<none>");
    CHECK(expand(sc, "") == "<none>");
    xk_shortcuts_remove(sc, "KO");
    CHECK(expand(sc, "ko") == "<none>");
    CHECK(xk_shortcuts_count(sc) == 2);
    xk_shortcuts_free(sc);
}

TEST_CASE("an import reads TAB, CSV and = lines and counts what it would do", "[shortcuts]") {
    XKShortcuts* sc = xk_shortcuts_new();
    xk_shortcuts_set(sc, "ko", "không");
    xk_shortcuts_set(sc, "dc", "được");
    const char* table =
        "\xEF\xBB\xBF# a made-up table\r\n"
        "ko\tkhông\r\n"                        // same
        "dc,\"được, rồi\"\n"                   // changed; CSV with a comma inside quotes
        "hn = Hà Nội\n"                        // new, = format
        "sg,Sài Gòn\n"                         // new, CSV
        "\"q\"\"t\",quote \"\"inside\"\"\n"    // new: q"t -> quote ""inside"" (unquoted field kept)
        "tks\tthanks = cảm ơn, nhé\n"          // new; TAB wins over = and ,
        "\n"
        "no separator here\n"                  // skipped
        "two words\tskipped\n"                 // skipped: an abbreviation is one word
        "\"open,quote\n"                       // skipped
        "a,b,c\n"                              // skipped: three fields
        "empty\t\n";                           // skipped
    XKShortcutCounts n{};
    CHECK(xk_shortcuts_import(sc, table, 0, &n) == 6);
    CHECK(n.added == 4);
    CHECK(n.same == 1);
    CHECK(n.changed == 1);
    CHECK(n.skipped == 5);
    CHECK(expand(sc, "hn") == "<none>");  // apply = 0 only counted
    CHECK(expand(sc, "dc") == "được");

    CHECK(xk_shortcuts_import(sc, table, 1, &n) == 6);
    CHECK(expand(sc, "hn") == "Hà Nội");
    CHECK(expand(sc, "sg") == "Sài Gòn");
    CHECK(expand(sc, "dc") == "được, rồi");
    CHECK(expand(sc, "q\"t") == "quote \"\"inside\"\"");
    CHECK(expand(sc, "tks") == "thanks = cảm ơn, nhé");
    CHECK(xk_shortcuts_count(sc) == 6);
    // Importing the same table again changes nothing.
    xk_shortcuts_import(sc, table, 1, &n);
    CHECK(n.added == 0);
    CHECK(n.changed == 0);
    CHECK(n.same == 6);
    xk_shortcuts_free(sc);
}

TEST_CASE("the table is the laptop's macros.txt, saved atomically", "[shortcuts]") {
    XKShortcuts* sc = xk_shortcuts_new();
    xk_shortcuts_set(sc, "ko", "không");
    xk_shortcuts_set(sc, "Hn", "Hà Nội");
    const std::string path = tmp("macros.txt");
    REQUIRE(xk_shortcuts_save(sc, path.c_str()) == 0);
    CHECK(slurp(path) == "hn\tHà Nội\nko\tkhông\n");

    // The file the addon and xk-macro write: TAB lines; others are not the
    // file's format and are skipped.
    std::ofstream(tmp("laptop.txt")) << "# made up\nko\tkhông\nvn\tViệt Nam\nhn=Hà Nội\n";
    XKShortcuts* back = xk_shortcuts_new();
    CHECK(xk_shortcuts_load(back, tmp("laptop.txt").c_str()) == 2);
    CHECK(expand(back, "vn") == "Việt Nam");
    CHECK(expand(back, "hn") == "<none>");
    CHECK(xk_shortcuts_load(back, tmp("missing.txt").c_str()) == -1);

    const std::string bad = tmp("no-such-dir/macros.txt");
    CHECK(xk_shortcuts_save(sc, bad.c_str()) == -1);
    CHECK(std::fopen((bad + ".part").c_str(), "r") == nullptr);

    const char* a = nullptr;
    const char* e = nullptr;
    REQUIRE(xk_shortcuts_at(sc, 0, &a, &e));
    CHECK(std::string(a) == "hn");
    CHECK(xk_shortcuts_at(sc, 2, &a, &e) == 0);
    xk_shortcuts_free(back);
    xk_shortcuts_free(sc);
}

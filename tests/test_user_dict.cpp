// The user's own words (xk-dict add) next to the built-in dictionary.
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include "xrnikey.h"

namespace {

std::vector<std::string> complete(XKDict* d, const char* prefix, int max = 20) {
    std::vector<char*> raw(max, nullptr);
    int n = xk_dict_complete(d, prefix, raw.data(), max);
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) { out.emplace_back(raw[i]); std::free(raw[i]); }
    return out;
}

bool has(const std::vector<std::string>& v, const std::string& w) {
    for (const auto& x : v) if (x == w) return true;
    return false;
}

std::string writeUserFile(const std::string& body) {
    std::string path = std::string(XRNIKEY_TEST_TMP) + "/user-dict-test.txt";
    std::ofstream(path) << body;
    return path;
}

}  // namespace

TEST_CASE("user words complete and look up alongside the dictionary", "[userdict]") {
    XKDict* d = xk_dict_load(XRNIKEY_TEST_DICT);
    REQUIRE(d != nullptr);
    REQUIRE(xk_dict_lookup(d, "xrnzzword") == 0);

    xk_dict_reload_user(d, writeUserFile("# comment\nxrnzzword\t9000\nxrnzzother\t10\n").c_str());
    CHECK(xk_dict_lookup(d, "xrnzzword") == 9000);
    auto c = complete(d, "xrnzz");
    CHECK(has(c, "xrnzzword"));
    CHECK(has(c, "xrnzzother"));
    CHECK(c.front() == "xrnzzword");  // higher frequency first

    // Reloading replaces the old list (xk-dict remove takes effect).
    xk_dict_reload_user(d, writeUserFile("xrnzzother\t10\n").c_str());
    CHECK(xk_dict_lookup(d, "xrnzzword") == 0);
    xk_dict_free(d);
}

TEST_CASE("bad lines in the user file are skipped, not fatal", "[userdict]") {
    XKDict* d = xk_dict_load(XRNIKEY_TEST_DICT);
    REQUIRE(d != nullptr);
    xk_dict_reload_user(d, writeUserFile(
        "xrnzzgood\t50\r\nnotab\n\t7\nxrnzzbad\tabc\nxrnzzhuge\t99999999999999\nxrnzzneg\t-3\n").c_str());
    CHECK(xk_dict_lookup(d, "xrnzzgood") == 50);  // CRLF line still read
    CHECK(xk_dict_lookup(d, "xrnzzbad") == 0);
    CHECK(xk_dict_lookup(d, "xrnzzhuge") == 0);
    CHECK(xk_dict_lookup(d, "xrnzzneg") == 0);
    xk_dict_reload_user(d, "/nonexistent/xrnikey/user-dict.txt");  // missing file: no words, no crash
    CHECK(xk_dict_lookup(d, "xrnzzgood") == 0);
    xk_dict_free(d);
}

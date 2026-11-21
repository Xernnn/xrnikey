// The emoji index (data/emoji/emoji-index.tsv), as the panel, the search
// and the suggestion strip use it.
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "xrnkey.h"

namespace {

struct Index {
    XKEmojiDB* db = xk_emoji_load(XRNKEY_EMOJI_INDEX);
    ~Index() { xk_emoji_free(db); }
};

std::vector<std::string> search(XKEmojiDB* db, const char* q) {
    char* chars[10];
    char* names[10];
    int n = xk_emoji_search(db, q, chars, names, 10);
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(chars[i]);
        std::free(chars[i]);
        std::free(names[i]);
    }
    return out;
}

std::vector<std::string> suggest(XKEmojiDB* db, std::vector<const char*> words) {
    char* out[32];
    int n = xk_emoji_suggest(db, words.data(), static_cast<int>(words.size()), out, 32);
    std::vector<std::string> v;
    for (int i = 0; i < n; ++i) {
        v.emplace_back(out[i]);
        std::free(out[i]);
    }
    return v;
}

bool has(const std::vector<std::string>& v, const std::string& x) {
    for (const auto& s : v)
        if (s == x) return true;
    return false;
}

}  // namespace

TEST_CASE("the panel lists the emoji in Unicode's order with Vietnamese names", "[emoji]") {
    Index x;
    REQUIRE(x.db != nullptr);
    CHECK(xk_emoji_count(x.db) == 1914);  // with the keycap "#️⃣", which starts with '#'
    bool keycap = false;
    for (int i = 0; i < xk_emoji_count(x.db); ++i) {
        const char* k = nullptr;
        xk_emoji_at(x.db, i, &k, nullptr, nullptr, nullptr);
        keycap = keycap || std::string(k) == "#️⃣";
    }
    CHECK(keycap);
    const char *e, *g, *name;
    int skin = -1;
    REQUIRE(xk_emoji_at(x.db, 0, &e, &g, &name, &skin));
    CHECK(std::string(e) == "😀");
    CHECK(std::string(g) == "Smileys & Emotion");
    CHECK(std::string(name) == "mặt cười toe toét");
    CHECK(skin == 0);
    // Skin-tone variants are folded into their base, which says it takes them.
    bool thumbs = false;
    for (int i = 0; i < xk_emoji_count(x.db); ++i) {
        xk_emoji_at(x.db, i, &e, &g, &name, &skin);
        CHECK(std::string(e).find("\U0001F3FD") == std::string::npos);
        if (std::string(e) == "👍") thumbs = skin == 1;
    }
    CHECK(thumbs);
    CHECK(xk_emoji_at(x.db, xk_emoji_count(x.db), &e, &g, &name, &skin) == 0);
    CHECK(xk_emoji_load("/nonexistent/emoji-index.tsv") == nullptr);
}

TEST_CASE("search finds emoji in Vietnamese or English, with or without accents", "[emoji]") {
    Index x;
    REQUIRE(x.db != nullptr);
    CHECK(search(x.db, "cười toe toét").at(0) == "😀");
    CHECK(search(x.db, "cuoi toe toet").at(0) == "😀");
    CHECK(search(x.db, "CA PHE").at(0) == "☕");
    CHECK(search(x.db, "thumbs up").at(0) == "👍");
    // The flag, and the lotus whose CLDR keywords include "Việt Nam".
    CHECK(has(search(x.db, "viet nam"), "🇻🇳"));
    CHECK(has(search(x.db, "viet nam"), "🪷"));
    CHECK(has(search(x.db, "meo"), "🐱"));
    CHECK(search(x.db, "zzqqzz").empty());
    CHECK(search(x.db, "").empty());
}

TEST_CASE("the words just typed suggest emoji, common keywords do not", "[emoji]") {
    Index x;
    REQUIRE(x.db != nullptr);
    CHECK(has(suggest(x.db, {"chúc", "mừng", "sinh", "nhật"}), "🎂"));
    CHECK(has(suggest(x.db, {"uống", "cà", "phê"}), "☕"));
    CHECK(has(suggest(x.db, {"tôi", "yêu"}), "😍"));
    CHECK(has(suggest(x.db, {"Trái", "Tim"}), "❤️"));
    CHECK(suggest(x.db, {"không", "đời", "nào"}).at(0) == "🙅");
    // "ok" is a keyword in both languages: each emoji once.
    auto ok = suggest(x.db, {"ok"});
    for (size_t i = 0; i < ok.size(); ++i)
        for (size_t j = i + 1; j < ok.size(); ++j) CHECK(ok[i] != ok[j]);
    CHECK(suggest(x.db, {"mặt"}).empty());  // too many emoji share it
    CHECK(suggest(x.db, {"bàn", "phím", "qwerty"}).empty());
}

TEST_CASE("the index loads from its text as from its file", "[emoji]") {
    std::ifstream f(XRNKEY_EMOJI_INDEX, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    XKEmojiDB* db = xk_emoji_load_text(text.data(), text.size());
    REQUIRE(db != nullptr);
    CHECK(xk_emoji_count(db) == 1914);
    CHECK(search(db, "cuoi toe toet").at(0) == "😀");
    xk_emoji_free(db);
    CHECK(xk_emoji_load_text("", 0) == nullptr);
    CHECK(xk_emoji_load_text(nullptr, 0) == nullptr);
}

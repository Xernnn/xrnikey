// Personal words, against the small test models: vi-general.xklm lacks the
// five rare words (data/lm/test/vi-rare.txt) that the real model also gets
// wrong after a Vietnamese word, so learning has something to put right.
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "xrnikey.h"

namespace {

struct Setup {
    XKLm* vi = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/vi-general.xklm").c_str());
    XKLm* en = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/en.xklm").c_str());
    XKUser* user = xk_user_new();
    XKModels* models = xk_models_new(vi, en);
    Setup() { xk_models_set_user(models, user); }
    ~Setup() {
        xk_models_free(models);
        xk_user_free(user);
        xk_lm_free(vi);
        xk_lm_free(en);
    }

    /* What the app shows after typing `keys` (space-separated words) into a
     * fresh state, learning from them when `learn`. */
    std::string type(const std::string& keys, bool learn = false) const {
        XKState* s = xk_state_new(XK_MODE_TELEX);
        xk_state_set_models(s, models);
        xk_state_set_option(s, XK_OPT_LEARN, learn ? 1 : 0);
        std::string doc;
        for (char c : keys) doc += xk_process_key(s, static_cast<unsigned char>(c), 0).commit;
        xk_state_free(s);
        return doc;
    }
};

std::string tmp(const char* name) { return std::string(XRNIKEY_TEST_TMP) + "/" + name; }

struct Pair {
    const char* keys;         // what is typed, Vietnamese context first
    const char* english;      // what comes out before learning
    const char* vietnamese;   // what the owner meant
    const char* unambiguous;  // keys for the same words that are not English
};

// The five rows the real model gets wrong (xk-eval, "the 19 words").
const std::vector<Pair> kFive = {
    {"ddooi boots ", "đôi boots ", "đôi bốt ", "ddooi bosot "},
    {"loox runs ", "lỗ runs ", "lỗ rún ", "loox rusn "},
    {"ddungs tips ", "đúng tips ", "đúng típ ", "ddungs tisp "},
    {"ngawns turn ", "ngắn turn ", "ngắn tủn ", "ngawns tunr "},
    {"nius virus ", "níu virus ", "níu víu ", "nius vius "},
};

}  // namespace

TEST_CASE("the five rare pairs come out right once learned", "[learn]") {
    Setup m;
    REQUIRE(m.vi != nullptr);
    REQUIRE(m.en != nullptr);
    for (const Pair& p : kFive) {
        INFO(p.keys);
        REQUIRE(m.type(p.keys) == p.english);         // the model alone: English
        REQUIRE(m.type(p.unambiguous) == p.vietnamese);  // keys that can only be Vietnamese
    }
    // The owner types each pair the unambiguous way, twice, with learning on.
    for (int i = 0; i < 2; ++i)
        for (const Pair& p : kFive) m.type(p.unambiguous, true);
    for (const Pair& p : kFive) {
        INFO(p.keys);
        CHECK(m.type(p.keys) == p.vietnamese);
    }
    // After an English word the same keys stay English: learning is by
    // pair, after the word before. Learned on its own, "bốt" was pushed
    // everywhere, and the real models made "my boots" Vietnamese.
    CHECK(m.type("my boots ") == "my boots ");
    CHECK(m.type("your turn ") == "your turn ");
    CHECK(m.type("the virus ") == "the virus ");
    for (const char* keys : {"two boots ", "it runs ", "two tips ", "it turn ", "two virus "}) {
        INFO(keys);
        std::string want = keys;
        CHECK(m.type(keys) == want);
    }
}

TEST_CASE("one use teaches nothing, and learning is off unless asked", "[learn]") {
    Setup m;
    m.type("ngawns tunr ", true);
    CHECK(xk_user_count(m.user, "ngắn", "tủn") == 1.0);
    CHECK(m.type("ngawns turn ") == "ngắn turn ");  // once is not enough
    m.type("ngawns tunr ");                          // learning off
    m.type("ngawns tunr ");
    CHECK(xk_user_count(m.user, "ngắn", "tủn") == 1.0);
    CHECK(m.type("ngawns turn ") == "ngắn turn ");
}

TEST_CASE("the frontend can teach words directly, as the phone will", "[learn]") {
    Setup m;
    for (int i = 0; i < 2; ++i) xk_user_learn(m.user, "Ngắn", "TỦN");  // stored lowercase
    CHECK(xk_user_count(m.user, "ngắn", "tủn") == 2.0);
    CHECK(xk_user_count(m.user, nullptr, "tủn") == 2.0);
    CHECK(m.type("ngawns turn ") == "ngắn tủn ");
    // Accent restoration uses the same personal words.
    std::vector<const char*> ctx = {"<s>", "ngắn"};
    std::vector<const char*> typed = {"tun"};
    char* out[1] = {nullptr};
    REQUIRE(xk_restore_run(m.models, ctx.data(), 2, typed.data(), 1, nullptr, out) == 1);
    CHECK(std::string(out[0]) == "tủn");
    std::free(out[0]);
    // Numbers, pauses and unknown text are not words to learn.
    xk_user_learn(m.user, nullptr, "<num>");
    xk_user_learn(m.user, nullptr, "123");
    CHECK(xk_user_count(m.user, nullptr, "<num>") == 0.0);
}

TEST_CASE("forgetting a word and fading old counts", "[learn]") {
    Setup m;
    for (int i = 0; i < 3; ++i) xk_user_learn(m.user, "ngắn", "tủn");
    xk_user_learn(m.user, "tủn", "rồi");
    CHECK(m.type("ngawns turn ") == "ngắn tủn ");
    xk_user_forget(m.user, "tủn");
    CHECK(xk_user_count(m.user, "ngắn", "tủn") == 0.0);
    CHECK(xk_user_count(m.user, "tủn", "rồi") == 0.0);
    CHECK(m.type("ngawns turn ") == "ngắn turn ");

    for (int i = 0; i < 4; ++i) xk_user_learn(m.user, "ngắn", "tủn");
    xk_user_decay(m.user, 0.5);
    CHECK(xk_user_count(m.user, "ngắn", "tủn") == 2.0);
    xk_user_decay(m.user, 0.2);  // 0.4: gone
    CHECK(xk_user_count(m.user, "ngắn", "tủn") == 0.0);
}

TEST_CASE("personal words survive a save and a load, and bad lines are skipped", "[learn]") {
    Setup m;
    for (int i = 0; i < 2; ++i) xk_user_learn(m.user, "ngắn", "tủn");
    xk_user_learn(m.user, "<s>", "Linh");
    const std::string path = tmp("learned.txt");
    REQUIRE(xk_user_save(m.user, path.c_str()) == 0);

    XKUser* back = xk_user_new();
    CHECK(xk_user_load(back, path.c_str()) == 4);  // tủn, linh, ngắn tủn, <s> linh
    CHECK(xk_user_count(back, "ngắn", "tủn") == 2.0);
    CHECK(xk_user_count(back, "<s>", "linh") == 1.0);

    std::ofstream(tmp("junk.txt")) << "# comment\nok\t3\n\tnoword\nnocount\t\nneg\t-2\n"
                                      "a b c\t2\nhuge\t1e300\nfine pair\t2\r\n<num>\t5\n";
    CHECK(xk_user_load(back, tmp("junk.txt").c_str()) == 2);  // "ok" and "fine pair"
    CHECK(xk_user_count(back, nullptr, "ok") == 3.0);
    CHECK(xk_user_count(back, "ngắn", "tủn") == 0.0);  // a load replaces what was there
    CHECK(xk_user_load(back, tmp("missing.txt").c_str()) == -1);

    // A save that cannot happen leaves no half-written file behind.
    const std::string bad = tmp("no-such-dir/learned.txt");
    CHECK(xk_user_save(m.user, bad.c_str()) == -1);
    CHECK(std::fopen((bad + ".part").c_str(), "r") == nullptr);
    xk_user_free(back);
}

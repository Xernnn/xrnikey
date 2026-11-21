// Next-word suggestions (xk_predict), against the small test models.
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "xrnkey.h"

namespace {

struct Setup {
    XKLm* vi = xk_lm_open((std::string(XRNKEY_TEST_LM_DIR) + "/vi-general.xklm").c_str());
    XKLm* en = xk_lm_open((std::string(XRNKEY_TEST_LM_DIR) + "/en.xklm").c_str());
    XKUser* user = xk_user_new();
    XKModels* models = xk_models_new(vi, en);
    Setup() { xk_models_set_user(models, user); }
    ~Setup() {
        xk_models_free(models);
        xk_user_free(user);
        xk_lm_free(vi);
        xk_lm_free(en);
    }

    std::vector<std::string> predict(std::vector<const char*> ctx, int max = 5) const {
        std::vector<char*> out(static_cast<size_t>(max));
        const int n = xk_predict(models, ctx.data(), static_cast<int>(ctx.size()), out.data(), max);
        std::vector<std::string> words;
        for (int i = 0; i < n; ++i) {
            words.emplace_back(out[static_cast<size_t>(i)]);
            std::free(out[static_cast<size_t>(i)]);
        }
        return words;
    }
};

bool has(const std::vector<std::string>& v, const char* w) { return std::find(v.begin(), v.end(), w) != v.end(); }

}  // namespace

TEST_CASE("the words seen after the context come first", "[predict]") {
    Setup m;
    REQUIRE(m.vi);
    REQUIRE(m.en);
    const auto after = m.predict({"<s>", "tôi", "không"});
    REQUIRE(after.size() == 5);
    // "không thể / có / được / biết" in the corpus.
    int seen = 0;
    for (const char* w : {"thể", "có", "được", "biết"}) seen += has(after, w) ? 1 : 0;
    CHECK(seen >= 3);
    // Nothing that isn't a word: no markers, numbers or punctuation.
    for (const auto& w : m.predict({"<s>", "tôi", "không"}, 30)) {
        CHECK(w[0] != '<');
        CHECK(w.find_first_of(".,!?") == std::string::npos);
    }
    // After an English word, English.
    const auto english = m.predict({"see", "you"});
    CHECK(english.at(0) == "tomorrow");  // "See you tomorrow." in the corpus
}

TEST_CASE("the owner's own pairs join in once typed twice", "[predict]") {
    Setup m;
    // A word the test model never saw after "không".
    CHECK_FALSE(has(m.predict({"<s>", "tôi", "không"}, 8), "nhé"));
    xk_user_learn(m.user, "không", "nhé");
    CHECK_FALSE(has(m.predict({"<s>", "tôi", "không"}, 8), "nhé"));
    for (int i = 0; i < 4; ++i) xk_user_learn(m.user, "không", "nhé");
    CHECK(has(m.predict({"<s>", "tôi", "không"}, 8), "nhé"));
}

TEST_CASE("predicting asks for sensible arguments", "[predict]") {
    Setup m;
    char* out[2] = {nullptr, nullptr};
    CHECK(xk_predict(nullptr, nullptr, 0, out, 2) == 0);
    CHECK(xk_predict(m.models, nullptr, 0, out, 0) == 0);
    // No context at all: the start of a sentence.
    const int n = xk_predict(m.models, nullptr, 0, out, 2);
    CHECK(n == 2);
    for (int i = 0; i < n; ++i) std::free(out[i]);
}

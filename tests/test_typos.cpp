// Typo correction in accent restoration, against the small test models
// (data/lm/test): words a key or two away on the QWERTY layout.
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "xrnikey.h"

namespace {

struct Models {
    XKLm* vi = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/vi.xklm").c_str());
    XKLm* en = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/en.xklm").c_str());
    XKModels* m = xk_models_new(vi, en);
    ~Models() {
        xk_models_free(m);
        xk_lm_free(vi);
        xk_lm_free(en);
    }
};

std::vector<std::string> run(XKModels* m, std::vector<const char*> ctx, std::vector<const char*> typed) {
    std::vector<char*> raw(typed.size(), nullptr);
    int n = xk_restore_run(m, ctx.data(), static_cast<int>(ctx.size()), typed.data(),
                           static_cast<int>(typed.size()), nullptr, raw.data());
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(raw[static_cast<size_t>(i)]);
        std::free(raw[static_cast<size_t>(i)]);
    }
    return out;
}

using V = std::vector<std::string>;

/* The candidates offered for `typed`, best first: text and slips. */
std::vector<std::pair<std::string, int>> offers(XKModels* m, std::vector<const char*> ctx, const char* typed) {
    std::vector<XKCandidate> c(64);
    int n = xk_restore_word(m, ctx.data(), static_cast<int>(ctx.size()), typed, c.data(), 64);
    std::vector<std::pair<std::string, int>> out;
    for (int i = 0; i < n; ++i) out.emplace_back(c[static_cast<size_t>(i)].text, c[static_cast<size_t>(i)].edit_distance);
    return out;
}

bool offered(const std::vector<std::pair<std::string, int>>& o, const std::string& text, int slips) {
    for (const auto& [t, k] : o)
        if (t == text && k == slips) return true;
    return false;
}

int most_slips(const std::vector<std::pair<std::string, int>>& o) {
    int m = 0;
    for (const auto& x : o) m = std::max(m, x.second);
    return m;
}

}  // namespace

TEST_CASE("a slip to a neighbouring key is put right in context", "[typos]") {
    Models t;
    REQUIRE(t.vi != nullptr);
    xk_models_set_correction(t.m, XK_CORRECT_FLEXIBLE);
    CHECK(run(t.m, {"<s>", "tôi"}, {"khonh"}) == V{"không"});            // g -> h
    CHECK(run(t.m, {"<s>", "chào"}, {"bab"}) == V{"bạn"});              // n -> b
    CHECK(offered(offers(t.m, {"<s>", "tôi", "không"}, "bieg"), "biết", 1));  // t -> g
    // The candidate says what it is.
    std::vector<const char*> ctx = {"<s>", "tôi"};
    XKCandidate c[4];
    REQUIRE(xk_restore_word(t.m, ctx.data(), 2, "khonh", c, 4) > 0);
    CHECK(std::string(c[0].text) == "không");
    CHECK(c[0].type == XK_CAND_CORRECTED);
    CHECK(c[0].edit_distance == 1);
}

TEST_CASE("correction is off unless asked, and leaves right words alone", "[typos]") {
    Models t;
    REQUIRE(t.vi != nullptr);
    CHECK(run(t.m, {"<s>", "tôi"}, {"khonh"}) == V{"khonh"});
    xk_models_set_correction(t.m, XK_CORRECT_FLEXIBLE);
    CHECK(run(t.m, {"<s>"}, {"Toi", "khong", "biet"}) == V{"Tôi", "không", "biết"});
    CHECK(run(t.m, {"<s>"}, {"we", "can", "chat"}) == V{"we", "can", "chat"});
    // Chat abbreviations and one-letter words are never corrected.
    CHECK(run(t.m, {"<s>", "tôi"}, {"ko"}) == V{"ko"});
    CHECK(run(t.m, {"<s>", "tôi"}, {"dc"}) == V{"dc"});
    CHECK(run(t.m, {"<s>", "tôi"}, {"oke"}) == V{"oke"});
    CHECK(run(t.m, {"<s>", "tôi"}, {"r"}) == V{"r"});
}

TEST_CASE("two slips: from 5 letters, or from 4 when strong", "[typos]") {
    Models t;
    REQUIRE(t.vi != nullptr);
    xk_models_set_correction(t.m, XK_CORRECT_FLEXIBLE);
    // Whether a correction two slips away wins depends on the model (this
    // test one is too small to pay for two); what is offered does not.
    CHECK(offered(offers(t.m, {"<s>", "tôi"}, "khlbg"), "không", 2));  // o -> l, n -> b
    CHECK(most_slips(offers(t.m, {"<s>", "tôi", "không"}, "bjey")) <= 1);  // 4 letters: one slip
    xk_models_set_correction(t.m, XK_CORRECT_STRONG);
    CHECK(offered(offers(t.m, {"<s>", "tôi", "không"}, "bjey"), "biết", 2));  // i -> j, t -> y
    xk_models_set_correction(t.m, XK_CORRECT_OFF);
    CHECK(most_slips(offers(t.m, {"<s>", "tôi"}, "khlbg")) == 0);
}

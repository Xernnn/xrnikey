// Accent restoration, against the small model built from the made-up text
// in data/lm/test/vi.txt (which is why "bàn" follows "cái" there, and
// "cố" only ever comes before "gắng").
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>
#include <vector>

#include "xrnikey.h"

namespace {

/* The Vietnamese model alone: restoration as it was before the English
 * model joined it (test_bilingual has both). */
struct Model {
    XKLm* vi = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/vi.xklm").c_str());
    XKModels* lm = xk_models_new(vi, nullptr);
    ~Model() {
        xk_models_free(lm);
        xk_lm_free(vi);
    }
};

std::vector<XKCandidate> restore(XKModels* lm, std::vector<const char*> ctx, const char* typed) {
    std::vector<XKCandidate> out(32);
    int n = xk_restore_word(lm, ctx.data(), static_cast<int>(ctx.size()), typed, out.data(),
                            static_cast<int>(out.size()));
    out.resize(static_cast<size_t>(n));
    return out;
}

std::string best(XKModels* lm, std::vector<const char*> ctx, const char* typed) {
    auto c = restore(lm, ctx, typed);
    return c.empty() ? "" : c[0].text;
}

std::vector<std::string> run(XKModels* lm, std::vector<const char*> ctx, std::vector<const char*> typed,
                             const char* next = nullptr) {
    std::vector<char*> raw(typed.size(), nullptr);
    int n = xk_restore_run(lm, ctx.data(), static_cast<int>(ctx.size()), typed.data(),
                           static_cast<int>(typed.size()), next, raw.data());
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(raw[static_cast<size_t>(i)]);
        std::free(raw[static_cast<size_t>(i)]);
    }
    return out;
}

bool offers(const std::vector<XKCandidate>& c, const std::string& text) {
    for (const auto& x : c)
        if (text == x.text) return true;
    return false;
}

}  // namespace

TEST_CASE("a word typed without marks gets its accents back", "[restore]") {
    Model m;
    REQUIRE(m.vi != nullptr);
    CHECK(best(m.lm, {"<s>", "tôi"}, "khong") == "không");
    CHECK(best(m.lm, {"tôi", "không"}, "duoc") == "được");
    // Every syllable of the shape is offered, not only the model's.
    auto c = restore(m.lm, {"<s>"}, "duoc");
    CHECK(offers(c, "được"));
    CHECK(offers(c, "dược"));
    CHECK(offers(c, "đuốc"));  // attested, never in the test text
    for (size_t i = 1; i < c.size(); ++i) CHECK(c[i - 1].score >= c[i].score);
    CHECK(c[0].type == XK_CAND_RESTORED);
}

TEST_CASE("the words before choose between readings of the same shape", "[restore]") {
    Model m;
    REQUIRE(m.vi != nullptr);
    CHECK(best(m.lm, {"<s>", "cái"}, "ban") == "bàn");
    CHECK(best(m.lm, {"<s>", "chào"}, "ban") == "bạn");
    CHECK(best(m.lm, {"công", "ty"}, "duoc") == "dược");
}

TEST_CASE("the typed letters' case carries over", "[restore]") {
    Model m;
    REQUIRE(m.vi != nullptr);
    CHECK(best(m.lm, {"<s>", "tôi"}, "Khong") == "Không");
    CHECK(best(m.lm, {"<s>", "tôi"}, "KHONG") == "KHÔNG");
    CHECK(best(m.lm, {"tôi", "không"}, "Duoc") == "Được");
}

TEST_CASE("words that are not Vietnamese stay as typed", "[restore]") {
    Model m;
    REQUIRE(m.vi != nullptr);
    auto c = restore(m.lm, {"<s>"}, "ko");  // no syllable has this shape
    REQUIRE(c.size() == 1);
    CHECK(std::string(c[0].text) == "ko");
    CHECK(c[0].type == XK_CAND_AS_TYPED);
    CHECK(best(m.lm, {"gửi"}, "file") == "file");  // a token the model knows
    // Not a word restoration takes: marks already typed, or not letters.
    CHECK(restore(m.lm, {}, "không").empty());
    CHECK(restore(m.lm, {}, "8h").empty());
    CHECK(restore(m.lm, {}, "").empty());
    CHECK(xk_restore_word(nullptr, nullptr, 0, "khong", nullptr, 0) == 0);
}

TEST_CASE("a run is decoded whole, so a later word fixes an earlier one", "[restore]") {
    Model m;
    REQUIRE(m.vi != nullptr);
    // Alone, "co" is "có"; with "gang" after it, the pair is "cố gắng".
    CHECK(best(m.lm, {"<s>"}, "co") == "có");
    CHECK(run(m.lm, {"<s>"}, {"co", "gang"}) == std::vector<std::string>{"cố", "gắng"});
    CHECK(run(m.lm, {"<s>"}, {"Toi", "khong", "biet"}) ==
          std::vector<std::string>{"Tôi", "không", "biết"});
    CHECK(run(m.lm, {"<s>"}, {"co", "8h"}).empty());
    CHECK(run(m.lm, {"<s>"}, {}).empty());
    std::vector<const char*> nine(9, "co");
    CHECK(run(m.lm, {}, nine).empty());
}

TEST_CASE("knowing the sentence ends picks the reading that ends it", "[restore]") {
    Model m;
    REQUIRE(m.vi != nullptr);
    // In the test text "nhẹ" follows "khá" most often but never ends a
    // sentence; "khá nhé" does. So "kha nhe" is "khá nhẹ" while the
    // sentence may go on, and "khá nhé" once a full stop ends it.
    CHECK(run(m.lm, {"<s>", "bạn"}, {"kha", "nhe"}) == std::vector<std::string>{"khá", "nhẹ"});
    CHECK(run(m.lm, {"<s>", "bạn"}, {"kha", "nhe"}, "</s>") == std::vector<std::string>{"khá", "nhé"});
    // A next token the model does not know changes nothing.
    CHECK(run(m.lm, {"<s>"}, {"co", "gang"}, "xyzzy") == std::vector<std::string>{"cố", "gắng"});
}

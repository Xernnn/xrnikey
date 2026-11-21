// The choice between a word's English and Vietnamese readings, in context,
// with the small models built from the made-up text in data/lm/test.
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>
#include <vector>

#include "xrnikey.h"

namespace {

struct Models {
    XKLm* vi = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/vi.xklm").c_str());
    XKLm* en = xk_lm_open((std::string(XRNIKEY_TEST_LM_DIR) + "/en.xklm").c_str());
    ~Models() {
        xk_lm_free(vi);
        xk_lm_free(en);
    }
};

void drop(std::string& doc, int n) {
    for (int i = 0; i < n && !doc.empty(); ++i) {
        while (!doc.empty() && (static_cast<unsigned char>(doc.back()) & 0xC0) == 0x80) doc.pop_back();
        if (!doc.empty()) doc.pop_back();
    }
}

uint32_t keysym(char c) {
    switch (c) {
        case '<': return 0xFF08;   // BackSpace
        case '\n': return 0xFF0D;  // Return
        default: return static_cast<unsigned char>(c);
    }
}

/* What the app shows after `keys`, the state set up by `setup`. */
template <typename Setup>
std::string type(const Models& m, bool use_models, bool instant, const std::string& keys, Setup setup) {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(s, instant ? 1 : 0);
    if (use_models) xk_state_set_models(s, m.vi, m.en);
    setup(s);
    std::string doc, preedit;
    for (char c : keys) {
        XKResult r = xk_process_key(s, keysym(c), 0);
        drop(doc, r.delete_before);
        doc += r.commit;
        preedit = r.preedit;
        if (!r.consumed) {
            if (c == '<') drop(doc, 1);
            else doc += c;
        }
    }
    xk_state_free(s);
    return doc + (instant ? "" : preedit);
}

std::string type(const Models& m, bool use_models, bool instant, const std::string& keys) {
    return type(m, use_models, instant, keys, [](XKState*) {});
}

struct Row {
    const char* keys;
    const char* want;
};

void check_rows(const std::vector<Row>& rows, bool use_models = true) {
    Models m;
    REQUIRE(m.vi != nullptr);
    REQUIRE(m.en != nullptr);
    for (bool instant : {false, true}) {
        for (const Row& r : rows) {
            INFO((instant ? "instant: " : "preedit: ") << r.keys);
            CHECK(type(m, use_models, instant, r.keys) == r.want);
        }
    }
}

}  // namespace

// The 19 English words whose Telex keys also type an attested syllable
// (vi-syllables.txt). With no context they came out Vietnamese everywhere.
TEST_CASE("the 19 words stay English after an English word", "[bilingual]") {
    check_rows({
        {"the boost ", "the boost "}, {"my boots ", "my boots "}, {"the corn ", "the corn "},
        {"two cups ", "two cups "}, {"it hangs ", "it hangs "}, {"she keeps ", "she keeps "},
        {"it lifts ", "it lifts "}, {"my list ", "my list "}, {"her lips ", "her lips "},
        {"the lisp ", "the lisp "}, {"the lust ", "the lust "}, {"my photos ", "my photos "},
        {"the row ", "the row "}, {"he runs ", "he runs "}, {"she sits ", "she sits "},
        {"the tips ", "the tips "}, {"your turn ", "your turn "}, {"the verse ", "the verse "},
        {"the virus ", "the virus "},
    });
}

TEST_CASE("the 19 words stay Vietnamese after a Vietnamese word", "[bilingual]") {
    check_rows({
        {"ddooi boost ", "đôi bốt "}, {"ddooi boots ", "đôi bốt "}, {"chuyeenj corn ", "chuyện cỏn "},
        {"giafnh cups ", "giành cúp "}, {"ddau hangs ", "đau háng "}, {"banhs keeps ", "bánh kếp "},
        {"hai lifts ", "hai lít "}, {"hai list ", "hai lít "}, {"booj lips ", "bộ líp "},
        {"booj lisp ", "bộ líp "}, {"ngaapj lust ", "ngập lút "}, {"dinhs photos ", "dính phốt "},
        {"bij row ", "bị rơ "}, {"loox runs ", "lỗ rún "}, {"raats sits ", "rất sít "},
        {"ddungs tips ", "đúng típ "}, {"ngawns turn ", "ngắn tủn "}, {"hai verse ", "hai vế "},
        {"nius virus ", "níu víu "},
    });
}

TEST_CASE("without both models the attested syllable still wins", "[bilingual]") {
    check_rows({{"my list ", "my lít "}, {"the turn ", "the tủn "}}, false);
    Models m;
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_models(s, m.vi, nullptr);  // one model is not enough to choose
    std::string doc;
    for (char c : std::string("my list ")) doc += xk_process_key(s, keysym(c), 0).commit;
    CHECK(doc == "my lít ");
    xk_state_free(s);
}

TEST_CASE("numbers and pauses are context, full stops and Return start afresh", "[bilingual]") {
    Models m;
    REQUIRE(m.vi != nullptr);
    // At the start of a sentence the test models make "list" Vietnamese,
    // and after "my" English, so each check below shows whether "my" was
    // still context.
    auto fresh = type(m, true, false, "list ");
    REQUIRE(fresh == "lít ");
    REQUIRE(type(m, true, false, "my list ") == "my list ");
    // A digit is a number token: "2 lít" as in "Mua 2 lít sữa".
    CHECK(type(m, true, false, "2 list ") == "2 lít ");
    // After a full stop or Return, "list" is decided as at the start.
    CHECK(type(m, true, false, "my. list ") == "my. lít ");
    CHECK(type(m, true, false, "my\nlist ") == "my\nlít ");
    // A comma is a pause, not a new sentence: "my" is still there.
    CHECK(type(m, true, false, "my, list ") == "my, list ");
    // BackSpace over committed text: the context it held is gone.
    CHECK(type(m, true, false, "my <<list ") == "mlít ");
}

TEST_CASE("a frontend can say what comes before the cursor", "[bilingual]") {
    Models m;
    REQUIRE(m.vi != nullptr);
    auto with = [&](std::vector<const char*> words) {
        return type(m, true, false, "list ", [&](XKState* s) {
            xk_state_set_context(s, words.data(), static_cast<int>(words.size()));
        });
    };
    CHECK(with({"my"}) == "list ");
    CHECK(with({"hai"}) == "lít ");
    CHECK(with({"Mua", "2"}) == "lít ");
    CHECK(with({}) == type(m, true, false, "list "));
    // xk_state_reset forgets it.
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_models(s, m.vi, m.en);
    std::vector<const char*> hai = {"hai"};
    xk_state_set_context(s, hai.data(), 1);
    xk_state_reset(s);
    std::string doc;
    for (char c : std::string("list ")) doc += xk_process_key(s, keysym(c), 0).commit;
    CHECK(doc == type(m, true, false, "list "));
    xk_state_free(s);
}

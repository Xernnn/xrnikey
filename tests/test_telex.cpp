#include <catch2/catch_test_macros.hpp>
#include <vietkey.h>
#include <cstring>
#include <string>

static VKState* make_telex() { return vk_state_new(VK_MODE_TELEX); }

static std::string process_string(VKState* s, const char* input) {
    std::string result;
    for (const char* p = input; *p; ++p) {
        VKResult r = vk_process_key(s, static_cast<uint32_t>(*p), 0);
        if (r.commit[0]) result += r.commit;
    }
    VKResult r = vk_process_key(s, 0x20, 0);
    if (r.commit[0]) result += r.commit;
    return result;
}

TEST_CASE("Telex basic vowels", "[telex]") {
    VKState* s = make_telex();

    SECTION("aa -> â") {
        VKResult r = vk_process_key(s, 'a', 0);
        r = vk_process_key(s, 'a', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("ee -> ê") {
        s = make_telex();
        vk_process_key(s, 'e', 0);
        VKResult r = vk_process_key(s, 'e', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("oo -> ô") {
        s = make_telex();
        vk_process_key(s, 'o', 0);
        VKResult r = vk_process_key(s, 'o', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("dd -> đ") {
        s = make_telex();
        vk_process_key(s, 'd', 0);
        VKResult r = vk_process_key(s, 'd', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("ow -> ơ") {
        s = make_telex();
        vk_process_key(s, 'o', 0);
        VKResult r = vk_process_key(s, 'w', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("aw -> ă") {
        s = make_telex();
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, 'w', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }
}

TEST_CASE("Telex tone marks", "[telex]") {
    SECTION("as -> á (sắc)") {
        VKState* s = make_telex();
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, 's', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("af -> à (huyền)") {
        VKState* s = make_telex();
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, 'f', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("ar -> ả (hỏi)") {
        VKState* s = make_telex();
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, 'r', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("ax -> ã (ngã)") {
        VKState* s = make_telex();
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, 'x', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("aj -> ạ (nặng)") {
        VKState* s = make_telex();
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, 'j', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }
}

TEST_CASE("Telex word composition", "[telex]") {
    SECTION("xin chao -> xin chào") {
        VKState* s = make_telex();
        std::string result = process_string(s, "xin chao");
        REQUIRE(result.find("xin") != std::string::npos);
        vk_state_free(s);
    }

    SECTION("viet nam") {
        VKState* s = make_telex();
        std::string result = process_string(s, "viet nam");
        REQUIRE(result.find("viet") != std::string::npos);
        vk_state_free(s);
    }
}

TEST_CASE("Telex backspace undo", "[telex]") {
    VKState* s = make_telex();
    vk_process_key(s, 'a', 0);
    vk_process_key(s, 'a', 0);
    VKResult r = vk_process_key(s, 0xFF08, 0);
    REQUIRE(r.consumed == 1);
    vk_state_free(s);
}

TEST_CASE("Telex pass-through English", "[telex]") {
    VKState* s = make_telex();
    std::string result = process_string(s, "hello");
    REQUIRE(result.find("hello") != std::string::npos);
    vk_state_free(s);
}

TEST_CASE("Telex Ctrl+Shift toggle", "[telex]") {
    VKState* s = make_telex();
    VKResult r = vk_process_key(s, 0, 0x05);
    REQUIRE(r.consumed == 1);
    REQUIRE(r.reset == 1);
    vk_state_free(s);
}

TEST_CASE("Telex multiple vowels", "[telex]") {
    SECTION("uuw -> ư") {
        VKState* s = make_telex();
        vk_process_key(s, 'u', 0);
        vk_process_key(s, 'u', 0);
        VKResult r = vk_process_key(s, 'w', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }
}

TEST_CASE("Telex tone on multi-char syllable", "[telex]") {
    SECTION("tooi -> tôi with tone") {
        VKState* s = make_telex();
        vk_process_key(s, 't', 0);
        vk_process_key(s, 'o', 0);
        vk_process_key(s, 'o', 0);
        vk_process_key(s, 'i', 0);
        VKResult r = vk_process_key(s, 's', 0);
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }
}

/* Regression: Telex `w` is a syllable-level modifier. It used to fire only
 * when the buffer ENDED in o/a/u, so "nguoiw" (ending in 'i') appended a
 * literal w and a following tone key landed on the wrong vowel --
 * "nguoiwf" came out "nguoìw". It also horned one vowel and stopped, so the
 * "uo" cluster Vietnamese writes "ươ" came out "uơ". */
TEST_CASE("Telex w applies across the whole vowel nucleus", "[telex]") {
    /* process_string() finalizes with Space, so expectations carry it. */
    SECTION("uo takes a horn on both vowels") {
        VKState* s = make_telex();
        REQUIRE(process_string(s, "uow") == "ươ "); /* ươ */
        vk_state_free(s);
    }

    SECTION("w reaches the nucleus from after a later nucleus vowel") {
        VKState* s = make_telex();
        REQUIRE(process_string(s, "nguoiwf") == "người "); /* người */
        vk_state_free(s);
    }

    SECTION("w reaches the nucleus from behind the coda") {
        VKState* s = make_telex();
        REQUIRE(process_string(s, "duongw") == "dương "); /* dương */
        vk_state_free(s);
    }

    SECTION("canonical spelling still works") {
        VKState* s = make_telex();
        REQUIRE(process_string(s, "nguwowif") == "người "); /* người */
        vk_state_free(s);
    }

    SECTION("w stays literal when the nucleus takes no mark") {
        VKState* s = make_telex();
        REQUIRE(process_string(s, "new") == "new ");
        vk_state_free(s);
    }
}

/* Bilingual auto-switch: a tone key must not fire inside a word Vietnamese
 * could not spell. "word" used to become "wỏd" because 'r' followed a vowel,
 * and "hello" became "helló". The decision comes from the real onset/coda
 * tables via can_begin_vn_syllable(), not from counting vowel-less letters --
 * that older heuristic rejected "ngh", so "nghieeng" never reached
 * "nghiêng". */
TEST_CASE("Telex leaves English words alone", "[telex]") {
    struct Case { const char* keys; const char* want; };
    /* Impossible onsets (w-, sh-, st-, br-, j-, z-, fl-) and impossible
     * codas (hel-, wor-, hel-p) each rule the word out. */
    const Case cases[] = {
        {"word", "word "},   {"show", "show "},   {"hello", "hello "},
        {"star", "star "},   {"brown", "brown "}, {"just", "just "},
        {"zero", "zero "},   {"flow", "flow "},   {"world", "world "},
        {"help", "help "},   {"first", "first "}, {"stress", "stress "},
    };
    for (const auto& c : cases) {
        VKState* s = make_telex();
        INFO("typing " << c.keys);
        REQUIRE(process_string(s, c.keys) == c.want);
        vk_state_free(s);
    }
}

TEST_CASE("Telex still composes real Vietnamese syllables", "[telex]") {
    struct Case { const char* keys; const char* want; };
    const Case cases[] = {
        {"xin", "xin "},
        {"chaof", "chào "},
        {"vieejt", "việt "},
        {"khoong", "không "},
        {"quoocs", "quốc "},
        {"ddaau", "đâu "},
        /* "ngh" is a real onset -- the old vowel-less-letter count rejected it */
        {"nghieeng", "nghiêng "},
        {"nghee", "nghê "},
    };
    for (const auto& c : cases) {
        VKState* s = make_telex();
        INFO("typing " << c.keys);
        REQUIRE(process_string(s, c.keys) == c.want);
        vk_state_free(s);
    }
}

/* Enter used to be unhandled: it reached the application while the composing
 * block was still pending, so the word was silently abandoned. It now
 * registers the pending syllable the way Space does, minus the trailing
 * character. */
TEST_CASE("Enter registers the pending word", "[telex]") {
    SECTION("Enter commits the composed syllable with no trailing char") {
        VKState* s = make_telex();
        for (const char* p = "chaof"; *p; ++p) {
            vk_process_key(s, static_cast<uint32_t>(*p), 0);
        }
        VKResult r = vk_process_key(s, 0xFF0D, 0); /* Return */
        REQUIRE(std::string(r.commit) == "chào");
        REQUIRE(r.consumed == 1);
        vk_state_free(s);
    }

    SECTION("Space still appends its space") {
        VKState* s = make_telex();
        for (const char* p = "chaof"; *p; ++p) {
            vk_process_key(s, static_cast<uint32_t>(*p), 0);
        }
        VKResult r = vk_process_key(s, 0x20, 0);
        REQUIRE(std::string(r.commit) == "chào ");
        vk_state_free(s);
    }

    SECTION("Enter with nothing pending passes through unconsumed") {
        VKState* s = make_telex();
        VKResult r = vk_process_key(s, 0xFF0D, 0);
        REQUIRE(r.consumed == 0);
        REQUIRE(r.commit[0] == '\0');
        vk_state_free(s);
    }
}

/* Found by round-tripping the whole dictionary: keystrokes -> engine -> word.
 * Open syllables took the tone on the FIRST vowel, which is right for two
 * vowels but wrong for a triphthong -- "ngoai"+f gave "ngòai". And "DD"
 * always produced lowercase đ. */
TEST_CASE("Telex tone lands correctly on open triphthongs", "[telex]") {
    struct Case { const char* keys; const char* want; };
    const Case cases[] = {
        {"ngoaif", "ngoài "},  {"xoaif", "xoài "},   {"loaij", "loại "},
        {"thoaij", "thoại "},  {"ngoaij", "ngoại "},
        /* two-vowel open syllables must be unchanged */
        {"chaof", "chào "},    {"hoaf", "hòa "},     {"cuar", "của "},
        /* coda still takes the last vowel */
        {"toans", "toán "},
    };
    for (const auto& c : cases) {
        VKState* s = make_telex();
        INFO("typing " << c.keys);
        REQUIRE(process_string(s, c.keys) == c.want);
        vk_state_free(s);
    }
}

TEST_CASE("Telex DD keeps its case", "[telex]") {
    VKState* s = make_telex();
    REQUIRE(process_string(s, "DDaf") == "Đà ");
    vk_state_free(s);
    s = make_telex();
    REQUIRE(process_string(s, "ddaf") == "đà ");
    vk_state_free(s);
}

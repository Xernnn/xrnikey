#include <catch2/catch_test_macros.hpp>
#include <xrnikey.h>
#include <string>

static std::string type_and_commit(XKState* s, const char* input) {
    std::string result;
    for (const char* p = input; *p; ++p) {
        XKResult r = xk_process_key(s, static_cast<uint32_t>(*p), 0);
        if (r.commit[0]) result += r.commit;
    }
    XKResult r = xk_process_key(s, 0x20, 0);
    if (r.commit[0]) result += r.commit;
    return result;
}

TEST_CASE("Full pipeline: xin chào thế giới", "[pipeline]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);

    SECTION("Type greeting via Telex") {
        std::string out = type_and_commit(s, "xin");
        REQUIRE(out.find("xin") != std::string::npos);

        xk_state_reset(s);
        out = type_and_commit(s, "chao");
        REQUIRE(!out.empty());

        xk_state_reset(s);
        out = type_and_commit(s, "the");
        REQUIRE(out.find("th") != std::string::npos);

        xk_state_reset(s);
        out = type_and_commit(s, "gioi");
        REQUIRE(!out.empty());
    }

    xk_state_free(s);
}

TEST_CASE("Full pipeline: bilingual paragraph", "[pipeline]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);

    SECTION("Vietnamese then English") {
        std::string vn = type_and_commit(s, "toi");
        REQUIRE(!vn.empty());

        xk_state_reset(s);
        std::string en = type_and_commit(s, "hello");
        REQUIRE(en.find("hello") != std::string::npos);
    }

    xk_state_free(s);
}

TEST_CASE("Full pipeline: VNI mode", "[pipeline]") {
    XKState* s = xk_state_new(XK_MODE_VNI);

    SECTION("Type with VNI numbers") {
        xk_process_key(s, 'a', 0);
        XKResult r = xk_process_key(s, '1', 0);
        REQUIRE(r.consumed == 1);
    }

    xk_state_free(s);
}

TEST_CASE("Full pipeline: state reset", "[pipeline]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_process_key(s, 'a', 0);
    xk_process_key(s, 'a', 0);
    xk_state_reset(s);

    char* preedit = xk_state_get_preedit(s);
    REQUIRE(preedit != nullptr);
    REQUIRE(preedit[0] == '\0');
    free(preedit);

    xk_state_free(s);
}

TEST_CASE("Full pipeline: syllable validation", "[pipeline]") {
    REQUIRE(xk_is_valid_syllable("xin") == 1);
    REQUIRE(xk_is_valid_syllable("hello") == 0);
    REQUIRE(xk_is_valid_syllable("nghe") == 1);
    REQUIRE(xk_is_valid_syllable("xyz") == 0);
}

TEST_CASE("Full pipeline: english word detection", "[pipeline]") {
    REQUIRE(xk_is_english_word("the") == 1);
    REQUIRE(xk_is_english_word("hello") == 1);
    REQUIRE(xk_is_english_word("computer") == 1);
}

TEST_CASE("Full pipeline: emoji trigger pass-through", "[pipeline]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);

    SECTION("Colon query committed as-is on space") {
        std::string out = type_and_commit(s, ":smile");
        REQUIRE(out.find(":") != std::string::npos);
    }

    xk_state_free(s);
}

TEST_CASE("Full pipeline: backspace undo chain", "[pipeline]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_process_key(s, 'a', 0);
    xk_process_key(s, 'a', 0);
    xk_process_key(s, 's', 0);
    XKResult r = xk_process_key(s, 0xFF08, 0);
    REQUIRE(r.consumed == 1);
    xk_state_free(s);
}

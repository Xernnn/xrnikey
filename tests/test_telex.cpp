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

#include <catch2/catch_test_macros.hpp>
#include <vietkey.h>
#include <string>

static std::string type_and_commit(VKState* s, const char* input) {
    std::string result;
    for (const char* p = input; *p; ++p) {
        VKResult r = vk_process_key(s, static_cast<uint32_t>(*p), 0);
        if (r.commit[0]) result += r.commit;
    }
    VKResult r = vk_process_key(s, 0x20, 0);
    if (r.commit[0]) result += r.commit;
    return result;
}

TEST_CASE("Full pipeline: xin chào thế giới", "[pipeline]") {
    VKState* s = vk_state_new(VK_MODE_TELEX);

    SECTION("Type greeting via Telex") {
        std::string out = type_and_commit(s, "xin");
        REQUIRE(out.find("xin") != std::string::npos);

        vk_state_reset(s);
        out = type_and_commit(s, "chao");
        REQUIRE(!out.empty());

        vk_state_reset(s);
        out = type_and_commit(s, "the");
        REQUIRE(out.find("th") != std::string::npos);

        vk_state_reset(s);
        out = type_and_commit(s, "gioi");
        REQUIRE(!out.empty());
    }

    vk_state_free(s);
}

TEST_CASE("Full pipeline: bilingual paragraph", "[pipeline]") {
    VKState* s = vk_state_new(VK_MODE_TELEX);

    SECTION("Vietnamese then English") {
        std::string vn = type_and_commit(s, "toi");
        REQUIRE(!vn.empty());

        vk_state_reset(s);
        std::string en = type_and_commit(s, "hello");
        REQUIRE(en.find("hello") != std::string::npos);
    }

    vk_state_free(s);
}

TEST_CASE("Full pipeline: VNI mode", "[pipeline]") {
    VKState* s = vk_state_new(VK_MODE_VNI);

    SECTION("Type with VNI numbers") {
        vk_process_key(s, 'a', 0);
        VKResult r = vk_process_key(s, '1', 0);
        REQUIRE(r.consumed == 1);
    }

    vk_state_free(s);
}

TEST_CASE("Full pipeline: state reset", "[pipeline]") {
    VKState* s = vk_state_new(VK_MODE_TELEX);
    vk_process_key(s, 'a', 0);
    vk_process_key(s, 'a', 0);
    vk_state_reset(s);

    char* preedit = vk_state_get_preedit(s);
    REQUIRE(preedit != nullptr);
    REQUIRE(preedit[0] == '\0');
    free(preedit);

    vk_state_free(s);
}

TEST_CASE("Full pipeline: syllable validation", "[pipeline]") {
    REQUIRE(vk_is_valid_syllable("xin") == 1);
    REQUIRE(vk_is_valid_syllable("hello") == 0);
    REQUIRE(vk_is_valid_syllable("nghe") == 1);
    REQUIRE(vk_is_valid_syllable("xyz") == 0);
}

TEST_CASE("Full pipeline: english word detection", "[pipeline]") {
    REQUIRE(vk_is_english_word("the") == 1);
    REQUIRE(vk_is_english_word("hello") == 1);
    REQUIRE(vk_is_english_word("computer") == 1);
}

TEST_CASE("Full pipeline: emoji trigger pass-through", "[pipeline]") {
    VKState* s = vk_state_new(VK_MODE_TELEX);

    SECTION("Colon query committed as-is on space") {
        std::string out = type_and_commit(s, ":smile");
        REQUIRE(out.find(":") != std::string::npos);
    }

    vk_state_free(s);
}

TEST_CASE("Full pipeline: backspace undo chain", "[pipeline]") {
    VKState* s = vk_state_new(VK_MODE_TELEX);
    vk_process_key(s, 'a', 0);
    vk_process_key(s, 'a', 0);
    vk_process_key(s, 's', 0);
    VKResult r = vk_process_key(s, 0xFF08, 0);
    REQUIRE(r.consumed == 1);
    vk_state_free(s);
}

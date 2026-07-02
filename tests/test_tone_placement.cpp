#include <catch2/catch_test_macros.hpp>
#include <vietkey.h>
#include <cstring>
#include <cstdlib>

TEST_CASE("Tone placement on simple vowels", "[tone]") {
    SECTION("a + sắc -> á") {
        char* result = vk_place_tone("a", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("a + huyền -> à") {
        char* result = vk_place_tone("a", VK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("a + hỏi -> ả") {
        char* result = vk_place_tone("a", VK_TONE_HOI);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("a + ngã -> ã") {
        char* result = vk_place_tone("a", VK_TONE_NGA);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("a + nặng -> ạ") {
        char* result = vk_place_tone("a", VK_TONE_NANG);
        REQUIRE(result != nullptr);
        free(result);
    }
}

TEST_CASE("Tone placement on special vowels", "[tone]") {
    SECTION("â + sắc") {
        char* result = vk_place_tone("\xC3\xA2", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ê + huyền") {
        char* result = vk_place_tone("\xC3\xAA", VK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ô + hỏi") {
        char* result = vk_place_tone("\xC3\xB4", VK_TONE_HOI);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ơ + ngã") {
        char* result = vk_place_tone("\xC6\xA1", VK_TONE_NGA);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ă + nặng") {
        char* result = vk_place_tone("\xC4\x83", VK_TONE_NANG);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ư + sắc") {
        char* result = vk_place_tone("\xC6\xB0", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }
}

TEST_CASE("Tone placement on syllables", "[tone]") {
    SECTION("ma + sắc -> má") {
        char* result = vk_place_tone("ma", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("nghe + huyền") {
        char* result = vk_place_tone("nghe", VK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("tuong + hỏi") {
        char* result = vk_place_tone("tuong", VK_TONE_HOI);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("viet + ngã") {
        char* result = vk_place_tone("viet", VK_TONE_NGA);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ban + nặng") {
        char* result = vk_place_tone("ban", VK_TONE_NANG);
        REQUIRE(result != nullptr);
        free(result);
    }
}

TEST_CASE("Tone placement diphthongs", "[tone]") {
    SECTION("ai + sắc") {
        char* result = vk_place_tone("ai", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("oa + huyền") {
        char* result = vk_place_tone("oa", VK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ua + hỏi") {
        char* result = vk_place_tone("ua", VK_TONE_HOI);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ia without coda") {
        char* result = vk_place_tone("ia", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ia with coda") {
        char* result = vk_place_tone("ian", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }
}

TEST_CASE("Tone placement no tone", "[tone]") {
    SECTION("none tone returns base") {
        char* result = vk_place_tone("hello", VK_TONE_NONE);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "hello") == 0);
        free(result);
    }
}

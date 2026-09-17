#include <catch2/catch_test_macros.hpp>
#include <vietkey.h>
#include <cstring>
#include <cstdlib>

TEST_CASE("Tone placement on simple vowels", "[tone]") {
    SECTION("a + sắc -> á") {
        char* result = vk_place_tone("a", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xC3\xA1") == 0);
        free(result);
    }

    SECTION("a + huyền -> à") {
        char* result = vk_place_tone("a", VK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xC3\xA0") == 0);
        free(result);
    }

    SECTION("a + hỏi -> ả") {
        /* Regression: this used to be wired to the same bytes as ngã (ã),
         * so "hỏi" and "ngã" were indistinguishable. */
        char* result = vk_place_tone("a", VK_TONE_HOI);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xA3") == 0);
        free(result);
    }

    SECTION("a + ngã -> ã") {
        char* result = vk_place_tone("a", VK_TONE_NGA);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xC3\xA3") == 0);
        free(result);
    }

    SECTION("a + nặng -> ạ") {
        char* result = vk_place_tone("a", VK_TONE_NANG);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xA1") == 0);
        free(result);
    }
}

TEST_CASE("Tone placement on special vowels", "[tone]") {
    /* Regression coverage: the vowel table used to collapse â/ê/ô/ơ/ă/ư back
     * to their plain base before applying a tone (e.g. â + sắc used to
     * produce "á" instead of "ấ"), silently dropping the circumflex/breve/
     * horn. Assert the real precomposed Unicode codepoints. */
    SECTION("â + sắc -> ấ") {
        char* result = vk_place_tone("\xC3\xA2", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xA5") == 0);
        free(result);
    }

    SECTION("ê + huyền -> ề") {
        char* result = vk_place_tone("\xC3\xAA", VK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\x81") == 0);
        free(result);
    }

    SECTION("ô + hỏi -> ổ") {
        char* result = vk_place_tone("\xC3\xB4", VK_TONE_HOI);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\x95") == 0);
        free(result);
    }

    SECTION("ơ + ngã -> ỡ") {
        char* result = vk_place_tone("\xC6\xA1", VK_TONE_NGA);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\xA1") == 0);
        free(result);
    }

    SECTION("ă + nặng -> ặ") {
        char* result = vk_place_tone("\xC4\x83", VK_TONE_NANG);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xB7") == 0);
        free(result);
    }

    SECTION("ư + sắc -> ứ") {
        char* result = vk_place_tone("\xC6\xB0", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\xA9") == 0);
        free(result);
    }

    SECTION("thê + sắc -> thế (modifier survives inside a full syllable)") {
        char* result = vk_place_tone("th\xC3\xAA", VK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "th\xE1\xBA\xBF") == 0);
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

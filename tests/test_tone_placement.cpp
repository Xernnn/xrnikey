#include <catch2/catch_test_macros.hpp>
#include <xrnikey.h>
#include <cstring>
#include <cstdlib>

TEST_CASE("Tone placement on simple vowels", "[tone]") {
    SECTION("a + sắc -> á") {
        char* result = xk_place_tone("a", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xC3\xA1") == 0);
        free(result);
    }

    SECTION("a + huyền -> à") {
        char* result = xk_place_tone("a", XK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xC3\xA0") == 0);
        free(result);
    }

    SECTION("a + hỏi -> ả") {
        /* Regression: this used to be wired to the same bytes as ngã (ã),
         * so "hỏi" and "ngã" were indistinguishable. */
        char* result = xk_place_tone("a", XK_TONE_HOI);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xA3") == 0);
        free(result);
    }

    SECTION("a + ngã -> ã") {
        char* result = xk_place_tone("a", XK_TONE_NGA);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xC3\xA3") == 0);
        free(result);
    }

    SECTION("a + nặng -> ạ") {
        char* result = xk_place_tone("a", XK_TONE_NANG);
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
        char* result = xk_place_tone("\xC3\xA2", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xA5") == 0);
        free(result);
    }

    SECTION("ê + huyền -> ề") {
        char* result = xk_place_tone("\xC3\xAA", XK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\x81") == 0);
        free(result);
    }

    SECTION("ô + hỏi -> ổ") {
        char* result = xk_place_tone("\xC3\xB4", XK_TONE_HOI);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\x95") == 0);
        free(result);
    }

    SECTION("ơ + ngã -> ỡ") {
        char* result = xk_place_tone("\xC6\xA1", XK_TONE_NGA);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\xA1") == 0);
        free(result);
    }

    SECTION("ă + nặng -> ặ") {
        char* result = xk_place_tone("\xC4\x83", XK_TONE_NANG);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBA\xB7") == 0);
        free(result);
    }

    SECTION("ư + sắc -> ứ") {
        char* result = xk_place_tone("\xC6\xB0", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "\xE1\xBB\xA9") == 0);
        free(result);
    }

    SECTION("thê + sắc -> thế (modifier survives inside a full syllable)") {
        char* result = xk_place_tone("th\xC3\xAA", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "th\xE1\xBA\xBF") == 0);
        free(result);
    }
}

TEST_CASE("Tone placement on syllables", "[tone]") {
    SECTION("ma + sắc -> má") {
        char* result = xk_place_tone("ma", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("nghe + huyền") {
        char* result = xk_place_tone("nghe", XK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("tuong + hỏi") {
        char* result = xk_place_tone("tuong", XK_TONE_HOI);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("viet + ngã") {
        char* result = xk_place_tone("viet", XK_TONE_NGA);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ban + nặng") {
        char* result = xk_place_tone("ban", XK_TONE_NANG);
        REQUIRE(result != nullptr);
        free(result);
    }
}

TEST_CASE("Tone placement diphthongs", "[tone]") {
    SECTION("ai + sắc") {
        char* result = xk_place_tone("ai", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("oa + huyền") {
        char* result = xk_place_tone("oa", XK_TONE_HUYEN);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ua + hỏi") {
        char* result = xk_place_tone("ua", XK_TONE_HOI);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ia without coda") {
        char* result = xk_place_tone("ia", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }

    SECTION("ia with coda") {
        char* result = xk_place_tone("ian", XK_TONE_SAC);
        REQUIRE(result != nullptr);
        free(result);
    }
}

TEST_CASE("Tone placement no tone", "[tone]") {
    SECTION("none tone returns base") {
        char* result = xk_place_tone("hello", XK_TONE_NONE);
        REQUIRE(result != nullptr);
        REQUIRE(std::strcmp(result, "hello") == 0);
        free(result);
    }
}

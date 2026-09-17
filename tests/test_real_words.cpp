#include <catch2/catch_test_macros.hpp>
#include <xrnikey.h>
#include <cstring>
#include <string>

/* Regression coverage for real Vietnamese words found broken while testing
 * against the opening lines of Nguyen Du's Truyen Kieu — every one of these
 * previously lost its tone or vowel modification entirely. */

static std::string type_telex(const char* input) {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    std::string doc;
    for (const char* p = input; *p; ++p) {
        XKResult r = xk_process_key(s, static_cast<uint32_t>(static_cast<unsigned char>(*p)), 0);
        if (!r.consumed) { doc += *p; continue; }
        if (r.commit[0]) doc += r.commit;
    }
    xk_state_free(s);
    return doc;
}

static std::string type_vni(const char* input) {
    XKState* s = xk_state_new(XK_MODE_VNI);
    std::string doc;
    for (const char* p = input; *p; ++p) {
        XKResult r = xk_process_key(s, static_cast<uint32_t>(static_cast<unsigned char>(*p)), 0);
        if (!r.consumed) { doc += *p; continue; }
        if (r.commit[0]) doc += r.commit;
    }
    xk_state_free(s);
    return doc;
}

TEST_CASE("Telex: triphthong/diphthong nucleus not misdetected as invalid", "[realwords]") {
    /* is_valid_vn_syllable used to reject "uoi"/"uou"/"ieu" shapes (missing
     * from the old fixed nucleus table), which made xk_is_english_word
     * treat them as English and silently drop the tone/vowel modification. */
    CHECK(type_telex("nguwowif ") == "người ");
    CHECK(type_telex("ddieeuf ") == "điều ");
    CHECK(type_telex("muwowif ") == "mười ");
    CHECK(type_telex("truwowngf ") == "trường ");
}

TEST_CASE("Telex: coincidental English-whitelist collision must not win over an explicit tone", "[realwords]") {
    /* "long" is in the English whitelist; "lòng" (heart/feelings) must
     * still win once the user has typed an explicit huyền tone key. */
    CHECK(type_telex("longf ") == "lòng ");
}

TEST_CASE("Telex: qu/gi onset digraphs don't take the tone themselves", "[realwords]") {
    CHECK(type_telex("quaf ") == "quà ");   // tone on 'a', not the glide 'u'
    CHECK(type_telex("giaf ") == "già ");   // tone on 'a', not the glide 'i'
    CHECK(type_telex("gif ") == "gì ");     // sole vowel -> i IS the nucleus
}

TEST_CASE("Telex: plain-vowel-cluster tone placement matches modern orthography", "[realwords]") {
    CHECK(type_telex("coix ") == "cõi ");     // open cluster -> first vowel
    CHECK(type_telex("cuar ") == "của ");     // open cluster -> first vowel
    CHECK(type_telex("chaof ") == "chào ");   // open cluster -> first vowel
    CHECK(type_telex("toans ") == "toán ");   // cluster + coda -> last vowel
    CHECK(type_telex("hoanf ") == "hoàn ");   // cluster + coda -> last vowel
}

TEST_CASE("Telex: punctuation ends a pending word like Space does", "[realwords]") {
    CHECK(type_telex("coix,") == "cõi,");
    CHECK(type_telex("nhau.") == "nhau.");
}

TEST_CASE("VNI: u7 -> ư (not just o7 -> ơ)", "[realwords]") {
    CHECK(type_vni("ngu7o7i2 ") == "người ");
    CHECK(type_vni("d9u7o75c ") == "được ");
}

TEST_CASE("VNI: tone digit works after the coda consonant, not just right after the vowel", "[realwords]") {
    CHECK(type_vni("lo2ng ") == "lòng ");
    CHECK(type_vni("toa1n ") == "toán ");
}

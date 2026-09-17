#include <catch2/catch_test_macros.hpp>
#include <xrnikey.h>
#include <cstring>
#include <string>

/* Simulates what a frontend must do with delete_before/commit: delete N
 * codepoints off the end of `doc`, then append `text`. Codepoints here are
 * approximated by scanning back over UTF-8 continuation bytes, mirroring
 * how a real client counts characters, not bytes. */
static void apply(std::string& doc, int delete_before, const char* text) {
    for (int removed = 0; removed < delete_before && !doc.empty(); ++removed) {
        /* Strip trailing continuation bytes first, then the lead byte. */
        while (!doc.empty() && (static_cast<unsigned char>(doc.back()) & 0xC0) == 0x80) {
            doc.pop_back();
        }
        if (!doc.empty()) doc.pop_back();
    }
    doc += text;
}

static std::string type_instant(XKState* s, const char* input) {
    std::string doc;
    for (const char* p = input; *p; ++p) {
        XKResult r = xk_process_key(s, static_cast<uint32_t>(*p), 0);
        apply(doc, r.delete_before, r.commit);
    }
    XKResult r = xk_process_key(s, 0x20, 0);
    apply(doc, r.delete_before, r.commit);
    return doc;
}

TEST_CASE("Instant commit off by default", "[instant]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_process_key(s, 'a', 0);
    XKResult r = xk_process_key(s, 's', 0);
    REQUIRE(r.delete_before == 0);
    REQUIRE(r.commit[0] == '\0');
    xk_state_free(s);
}

TEST_CASE("Instant commit corrects in place: as -> a then á", "[instant]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(s, 1);

    XKResult r1 = xk_process_key(s, 'a', 0);
    REQUIRE(r1.delete_before == 0);
    REQUIRE(std::string(r1.commit) == "a");

    XKResult r2 = xk_process_key(s, 's', 0);
    REQUIRE(r2.delete_before == 1);
    REQUIRE(std::string(r2.commit) == "\xC3\xA1"); /* á */

    xk_state_free(s);
}

TEST_CASE("Instant commit: xin types cleanly with no stray deletions", "[instant]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(s, 1);
    std::string doc = type_instant(s, "xin");
    REQUIRE(doc == "xin ");
    xk_state_free(s);
}

TEST_CASE("Instant commit: aa -> a doubles into â in place", "[instant]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(s, 1);

    XKResult r1 = xk_process_key(s, 'a', 0);
    REQUIRE(std::string(r1.commit) == "a");

    XKResult r2 = xk_process_key(s, 'a', 0);
    REQUIRE(r2.delete_before == 1);
    REQUIRE(std::string(r2.commit) == "\xC3\xA2"); /* â */

    xk_state_free(s);
}

TEST_CASE("Instant commit: tone retarget mid-word matches classic output", "[instant]") {
    /* "thees": tone starts pending on no vowel, "ee" doubles into ê, then
     * 's' retargets the tone onto ê. This exercises the trickiest case for
     * diff-commit: the visible text changes non-append-only mid-word.
     * Assert instant-commit reconstructs byte-for-byte what the classic
     * (buffer-then-commit-on-space) path produces, whatever that value is —
     * this test is about diff-commit fidelity, not tone-table correctness. */
    XKState* classic = xk_state_new(XK_MODE_TELEX);
    std::string classic_out;
    for (const char* p = "thees"; *p; ++p) {
        XKResult r = xk_process_key(classic, static_cast<uint32_t>(*p), 0);
        if (r.commit[0]) classic_out += r.commit;
    }
    XKResult rf = xk_process_key(classic, 0x20, 0);
    if (rf.commit[0]) classic_out += rf.commit;
    xk_state_free(classic);

    XKState* instant = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(instant, 1);
    std::string instant_out = type_instant(instant, "thees");
    xk_state_free(instant);

    REQUIRE(instant_out == classic_out);
}

TEST_CASE("Instant commit: backspace deletes the last committed transform", "[instant]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(s, 1);

    xk_process_key(s, 'a', 0);
    xk_process_key(s, 'a', 0); /* buffer is now â, committed = "â" */

    XKResult r = xk_process_key(s, 0xFF08, 0); /* backspace undoes aa->â */
    REQUIRE(r.delete_before == 1);
    REQUIRE(std::string(r.commit) == "a");

    xk_state_free(s);
}

TEST_CASE("Instant commit: full document reconstruction matches classic output", "[instant]") {
    XKState* classic = xk_state_new(XK_MODE_TELEX);
    std::string classic_out;
    for (const char* p = "chao"; *p; ++p) {
        XKResult r = xk_process_key(classic, static_cast<uint32_t>(*p), 0);
        if (r.commit[0]) classic_out += r.commit;
    }
    XKResult rf = xk_process_key(classic, 0x20, 0);
    if (rf.commit[0]) classic_out += rf.commit;
    xk_state_free(classic);

    XKState* instant = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(instant, 1);
    std::string instant_out = type_instant(instant, "chao");
    xk_state_free(instant);

    REQUIRE(instant_out == classic_out);
}

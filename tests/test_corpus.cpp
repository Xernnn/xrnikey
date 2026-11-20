// Table-driven typing corpus: keystrokes in, the text the application ends up
// showing out, in both the classic preedit mode and instant (in-place) commit.
//
// Key notation in `keys`: '<' is BackSpace, '\n' Return, '\t' Tab, '\x1b'
// Escape; every other character is that key. The expected text is what the
// user sees afterwards: what was committed, plus the preedit still showing (in
// preedit mode), with keys the engine let through applied as the app would.
//
// Each group of rows is one TEST_CASE. A group tagged [!mayfail] still has
// rows the engine gets wrong; the tag comes off in the commit that fixes them.
#include <catch2/catch_test_macros.hpp>
#include <xrnikey.h>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

enum Mode { PREEDIT = 1, INSTANT = 2, BOTH = 3 };

struct Opt {
    int option;
    int value;
};

struct Row {
    const char* keys;
    const char* want;
    int modes = BOTH;
    std::vector<Opt> opts = {};
};

uint32_t keysym_for(char c) {
    switch (c) {
        case '<': return 0xFF08;   // BackSpace
        case '\n': return 0xFF0D;  // Return
        case '\t': return 0xFF09;  // Tab
        case '\x1b': return 0xFF1B; // Escape
        default: return static_cast<unsigned char>(c);
    }
}

void delete_chars(std::string& doc, int n) {
    for (int i = 0; i < n && !doc.empty(); ++i) {
        while (!doc.empty() && (static_cast<unsigned char>(doc.back()) & 0xC0) == 0x80) doc.pop_back();
        if (!doc.empty()) doc.pop_back();
    }
}

// What the application does with a key the engine did not consume.
void app_key(std::string& doc, char c) {
    if (c == '<') delete_chars(doc, 1);
    else if (c == '\x1b') {}
    else doc += c;
}

std::string type(int mode, int instant, const Row& row) {
    const char* keys = row.keys;
    XKState* s = xk_state_new(mode);
    xk_state_set_instant_commit(s, instant);
    for (const Opt& o : row.opts) xk_state_set_option(s, o.option, o.value);
    std::string doc, preedit;
    for (const char* p = keys; *p; ++p) {
        XKResult r = xk_process_key(s, keysym_for(*p), 0);
        delete_chars(doc, r.delete_before);
        doc += r.commit;
        if (!r.consumed) app_key(doc, *p);
        preedit = r.preedit;
    }
    xk_state_free(s);
    return instant ? doc : doc + preedit;
}

void run(int im, const std::vector<Row>& rows) {
    for (const Row& row : rows) {
        for (int instant = 0; instant <= 1; ++instant) {
            if (!(row.modes & (instant ? INSTANT : PREEDIT))) continue;
            INFO((im == XK_MODE_VNI ? "VNI " : "Telex ")
                 << (instant ? "instant" : "preedit") << " typing \"" << row.keys << "\"");
            CHECK(type(im, instant, row) == row.want);
        }
    }
}

}  // namespace

TEST_CASE("corpus: words that already work", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"truowngf ", "trường "}, {"nguoiwf ", "người "}, {"quyeets ", "quyết "},
        {"giaf ", "già "}, {"gias ", "giá "}, {"uowng ", "ương "},
        {"Vieetj Nam ", "Việt Nam "}, {"DDaay ", "Đây "}, {"toasn ", "toán "},
        {"toans ", "toán "}, {"giuwax ", "giữa "}, {"nghieeng ", "nghiêng "},
        {"khuyur ", "khuỷu "}, {"ruowuj ", "rượu "}, {"quoocs ", "quốc "},
        {"chaof ", "chào "}, {"xin chaof ", "xin chào "}, {"cuar ", "của "},
        {"longf ", "lòng "}, {"ngoaif ", "ngoài "}, {"dduwowcj ", "được "},
        {"window ", "window "}, {"wow ", "wow "}, {"access ", "access "},
        {"status ", "status "}, {"class ", "class "}, {"linux ", "linux "},
        {"hello ", "hello "}, {"word ", "word "}, {"show ", "show "},
        {"coix,", "cõi,"}, {"xin <", "xin"},
        // Ambiguous: the Telex reading is real Vietnamese, so it wins (as in Bamboo).
        {"of ", "\xC3\xB2 "}, {"is ", "\xC3\xAD "},
    });
    run(XK_MODE_VNI, {
        {"ngu7o7i2 ", "người "}, {"d9u7o7c5 ", "được "}, {"toa1n ", "toán "},
        {"lo2ng ", "lòng "}, {"Vie6t5 Nam ", "Việt Nam "},
    });
}

// A word can start right after punctuation or a digit; the keys before it are
// text of their own, not the first keys of the word.
TEST_CASE("corpus: a word after punctuation or a digit still gets its marks", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"(vieetj) ", "(việt) "}, {"\"chaof\" ", "\"chào\" "}, {"'chaof' ", "'chào' "},
        {"-tieengs ", "-tiếng "}, {"2tieengs ", "2tiếng "}, {"((Vieetj ", "((Việt "},
        {"(", "("}, {"(chaof", "(chào"}, {"(chaof<", "(chà"}, {"(<", ""},
        // Text that only looks like words stays as typed.
        {"mp3 ", "mp3 "}, {"x86 ", "x86 "}, {"h2o ", "h2o "}, {"a@b.com ", "a@b.com "},
        {"don't ", "don't "}, {"e.g. ", "e.g. "}, {"C++ ", "C++ "}, {"10:30 ", "10:30 "},
        {"https://example.com/a?b=c ", "https://example.com/a?b=c "},
    });
    run(XK_MODE_VNI, {
        // A digit is a tone or mark key only after the letters it changes; a
        // leading one is text.
        {"(vie65t) ", "(việt) "}, {"\"cha2o\" ", "\"chào\" "}, {"2tie61ng ", "2tiếng "},
        {"mp3 ", "mp3 "}, {"x86 ", "x86 "}, {"h2o ", "h2o "}, {"10:30 ", "10:30 "},
        {"a@b.com ", "a@b.com "}, {"1a1 ", "1á "},
    });
}

TEST_CASE("corpus: English words come back as typed", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"password ", "password "}, {"user ", "user "}, {"text ", "text "},
        {"config ", "config "}, {"result ", "result "}, {"your ", "your "},
        {"good ", "good "}, {"push ", "push "}, {"use ", "use "},
        {"more ", "more "}, {"her ", "her "},
        // "tém" is not in core/data/vi-syllables.txt, so the English word wins.
        {"terms ", "terms "}, {"PASSWORD ", "PASSWORD "}, {"Result ", "Result "},
        {"pass ", "pass "}, {"sorry ", "sorry "},
        {"issue ", "issue "}, {"coffee ", "coffee "}, {"add ", "add "},
        // A word that stops being Vietnamese shows as typed while still in progress.
        {"text", "text"}, {"push", "push"},
        // Real syllables stay Vietnamese even when the keys spell English:
        // nơ (a bow) is attested, as is mí (eyelid).
        {"now ", "n\xC6\xA1 "}, {"longf ", "lòng "}, {"tooi ", "tôi "},
        {"ddi ", "đi "}, {"ddc ", "đc "}, {"ddt ", "đt "},
        // With auto-restore off, what the keys compose is what is committed.
        {"terms ", "tém ", BOTH, {{XK_OPT_AUTO_RESTORE, 0}}},
        {"text ", "tẽt ", BOTH, {{XK_OPT_AUTO_RESTORE, 0}}},
    });
}

TEST_CASE("corpus: BackSpace deletes one character", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"toans<", "t\xC3\xB3" "a"},  // tóa: the tone stays, re-placed for "toa"
        {"toans<n ", "toán "}, {"window<", "windo"}, {"hello<", "hell"},
        {"dduwowcj<", "đượ"}, {"nguoiwf<", "ngườ"}, {"chaof<", "chà"},
        {"vieejt<", "việ"}, {"khoong<", "khôn"}, {"giaf<", "gi"},
        {"thuowr<", "thu"}, {"text<", "tex"}, {"aa<", ""},
        {"tex<", "t"}, {"aas<", ""}, {"chaof<<<<", ""}, {"chaof<<<<<", ""},
    });
}

TEST_CASE("corpus: capitals follow the keys that typed them", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"As ", "Á "}, {"AAn ", "Ân "}, {"OOng ", "Ông "}, {"HOAF ", "HÒA "},
        {"VIEETJ ", "VIỆT "}, {"Owr ", "Ở "}, {"TRUOWNGF ", "TRƯỜNG "},
        {"DDUWOWCJ ", "ĐƯỢC "}, {"Vieetj ", "Việt "}, {"NGUOIWF ", "NGƯỜI "},
        {"DDaay ", "Đây "}, {"Ddi ", "Đi "}, {"dDi ", "đi "},
    });
}

TEST_CASE("corpus: typing a key twice undoes it", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"aaa ", "aa "}, {"eee ", "ee "}, {"ooo ", "oo "}, {"ddd ", "dd "},
        {"ass ", "as "}, {"aff ", "af "}, {"aww ", "aw "}, {"oww ", "ow "},
        {"aaaa ", "aaa "}, {"asss ", "ass "}, {"dddd ", "ddd "},
        {"uss ", "us "}, {"off ", "of "}, {"xooong ", "xoong "},
        {"miss ", "miss "}, {"noww ", "now "}, {"thiss ", "this "},
        {"toansz ", "toan "}, {"toanszz ", "toanz "}, {"asfs ", "á "},
        {"aaa<", "a"}, {"ass<", "a"},
    });
}

TEST_CASE("corpus: uo and uu take the horn the way Vietnamese spells them", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"thuowr ", "thuở "}, {"luuw ", "lưu "}, {"huow ", "huơ "},
        {"uow ", "ươ "}, {"quow ", "quơ "}, {"khuow ", "khuơ "},
        {"huowng ", "hương "}, {"muaw ", "mưa "}, {"tuoiw ", "tươi "},
        {"uowi ", "ươi "}, {"thuowngf ", "thường "}, {"huwowng ", "hương "},
        // "uo" with no horn is not a Vietnamese vowel, so the tone key stays a letter.
        {"thuor ", "thuor "},
    });
}

TEST_CASE("corpus: w on its own is u-horn", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"w ", "ư "}, {"tw ", "tư "}, {"nhw ", "như "}, {"thw ", "thư "},
        {"ww ", "w "}, {"www ", "ww "}, {"nguw ", "ngư "}, {"twf ", "từ "},
        {"W ", "Ư "}, {"Tw ", "Tư "}, {"w<", ""}, {"tw<", "t"},
        {"we ", "we "}, {"with ", "with "}, {"what ", "what "},
        {"window ", "window "}, {"wow ", "wow "}, {"new ", "new "},
        {"w ", "w ", BOTH, {{XK_OPT_W_SHORTCUT, 0}}},
        {"tw ", "tw ", BOTH, {{XK_OPT_W_SHORTCUT, 0}}},
        {"tuw ", "tư ", BOTH, {{XK_OPT_W_SHORTCUT, 0}}},
    });
}

TEST_CASE("corpus: marks and tones may follow the final consonant", "[corpus]") {
    const std::vector<Opt> strict = {{XK_OPT_FREE_MARKING, 0}};
    run(XK_MODE_TELEX, {
        {"dangd ", "đang "}, {"tienge ", "tiêng "}, {"tiengse ", "tiếng "},
        {"tienges ", "tiếng "}, {"khongo ", "không "}, {"duongwf ", "dường "},
        {"chana ", "chân "}, {"vietje ", "việt "}, {"dangdd ", "dangd "},
        {"caawn ", "căn "}, {"cawan ", "cân "},
        // Without free marking: a tone before the final consonant, a mark
        // right after its letter.
        {"dangd ", "dangd ", BOTH, strict}, {"toans ", "toans ", BOTH, strict},
        {"toasn ", "toán ", BOTH, strict}, {"ddi ", "đi ", BOTH, strict},
        {"khongo ", "khongo ", BOTH, strict}, {"khoong ", "không ", BOTH, strict},
        {"duongw ", "duongw ", BOTH, strict}, {"nguoiwf ", "người ", BOTH, strict},
    });
}

TEST_CASE("corpus: modern tone style moves the tone in oa oe uy", "[corpus]") {
    const std::vector<Opt> modern = {{XK_OPT_MODERN_STYLE, 1}};
    run(XK_MODE_TELEX, {
        {"hoaf ", "hòa "}, {"thuys ", "thúy "}, {"khoer ", "khỏe "},
        {"hoaf ", "hoà ", BOTH, modern}, {"thuys ", "thuý ", BOTH, modern},
        {"khoer ", "khoẻ ", BOTH, modern}, {"thoar ", "thoả ", BOTH, modern},
        {"HOAF ", "HOÀ ", BOTH, modern}, {"thuyr ", "thuỷ ", BOTH, modern},
        // Everything else is the same in both styles.
        {"cuar ", "của ", BOTH, modern}, {"muaf ", "mùa ", BOTH, modern},
        {"quys ", "quý ", BOTH, modern}, {"toans ", "toán ", BOTH, modern},
        {"ngoaif ", "ngoài ", BOTH, modern}, {"hoaij ", "hoại ", BOTH, modern},
        {"khuyur ", "khuỷu ", BOTH, modern}, {"giaf ", "già ", BOTH, modern},
        {"hoaf<", "ho", BOTH, modern}, {"hoaf<", "hò"},
    });
}

TEST_CASE("corpus: Enter Tab and Escape end the word and reach the app", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"chaof\n", "chào\n"}, {"chaof\t", "chào\t"}, {"chaof\x1b", "chào"},
        {"xin chaof\nban", "xin chào\nban"}, {"\n", "\n"},
        {"terms\n", "terms\n"}, {"chaof\n<", "chào"},
    });
}

namespace {

struct Pressed {
    XKResult r;
    std::string preedit;
};

Pressed press(XKState* s, uint32_t keysym, uint32_t mods = 0) {
    Pressed p{xk_process_key(s, keysym, mods), {}};
    char* pe = xk_state_get_preedit(s);
    p.preedit = pe ? pe : "";
    std::free(pe);
    return p;
}

}  // namespace

// Cursor and editing keys end the word and reach the app, so nothing stale
// is left behind: "vie" Left "et" must not go on editing "vie".
TEST_CASE("keys that are not text end the word and pass through", "[corpus]") {
    const uint32_t keys[] = {0xFF51 /* Left */, 0xFF53 /* Right */, 0xFF52 /* Up */,
                             0xFF54 /* Down */, 0xFF50 /* Home */, 0xFF57 /* End */,
                             0xFF55 /* PageUp */, 0xFF56 /* PageDown */, 0xFFFF /* Delete */,
                             0xFF09 /* Tab */, 0xFF1B /* Escape */, 0xFF0D /* Return */,
                             0xFF8D /* KP_Enter */, 0xFFBE /* F1 */};
    for (uint32_t key : keys) {
        INFO("keysym " << std::hex << key);
        for (int instant = 0; instant <= 1; ++instant) {
            XKState* s = xk_state_new(XK_MODE_TELEX);
            xk_state_set_instant_commit(s, instant);
            press(s, 'v');
            press(s, 'i');
            press(s, 'e');
            Pressed p = press(s, key);
            CHECK(p.r.consumed == 0);
            CHECK(p.preedit.empty());
            CHECK(p.r.delete_before == 0);
            CHECK(std::string(p.r.commit) == (instant ? "" : "vie"));
            p = press(s, 'e');
            CHECK(p.preedit == "e");
            CHECK(p.r.delete_before == 0);
            p = press(s, 't');
            CHECK(p.preedit == "et");
            CHECK(p.r.delete_before == 0);
            xk_state_free(s);
        }
    }
}

TEST_CASE("modifier keys alone keep the word and chords end it", "[corpus]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    press(s, 'v');
    Pressed p = press(s, 0xFFE1 /* Shift_L */, 0);
    CHECK(p.r.consumed == 0);
    CHECK(p.preedit == "v");
    p = press(s, 0xFFE3 /* Control_L */, 0);
    CHECK(p.preedit == "v");
    p = press(s, 'c', 0x04 /* Ctrl */);
    CHECK(p.r.consumed == 0);
    CHECK(std::string(p.r.commit) == "v");
    CHECK(p.preedit.empty());
    xk_state_free(s);
}

TEST_CASE("xk_state_flush finishes the word", "[corpus]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    for (const char* k = "terms"; *k; ++k) press(s, static_cast<unsigned char>(*k));
    XKResult r = xk_state_flush(s);
    CHECK(r.consumed == 0);
    CHECK(std::string(r.commit) == "terms");
    r = xk_state_flush(s);
    CHECK(std::string(r.commit).empty());
    xk_state_free(s);

    s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_instant_commit(s, 1);
    for (const char* k = "terms"; *k; ++k) press(s, static_cast<unsigned char>(*k));
    r = xk_state_flush(s);  // the document shows "tém"; restore it to "terms"
    CHECK(r.delete_before == 2);
    CHECK(std::string(r.commit) == "erms");
    xk_state_free(s);
}

TEST_CASE("corpus: VNI does what Telex does", "[corpus]") {
    const std::vector<Opt> strict = {{XK_OPT_FREE_MARKING, 0}};
    run(XK_MODE_VNI, {
        // Typing a digit again undoes it, and it stays a digit after that.
        {"a66 ", "a6 "}, {"a11 ", "a1 "}, {"d99 ", "d9 "}, {"o77 ", "o7 "},
        {"a88 ", "a8 "}, {"a111 ", "a11 "}, {"dang99 ", "dang9 "},
        // 0 removes the tone.
        {"toa1n0 ", "toan "},
        // Capitals.
        {"A1 ", "Á "}, {"D9u7o7c5 ", "Được "}, {"VIE6T5 ", "VIỆT "}, {"D9I ", "ĐI "},
        // uo / uu / ua.
        {"thuo73 ", "thuở "}, {"huo7 ", "huơ "}, {"huong7 ", "hương "},
        {"luu7 ", "lưu "}, {"mua7 ", "mưa "}, {"quo7 ", "quơ "},
        {"nguoi72 ", "người "},
        // BackSpace.
        {"toa1n<", "t\xC3\xB3" "a"}, {"a1<", ""}, {"d9u7o7c5<", "đượ"},
        // Free marking, and without it.
        {"dang9 ", "đang "}, {"tieng61 ", "tiếng "},
        {"dang9 ", "dang9 ", BOTH, strict}, {"toan1 ", "toan1 ", BOTH, strict},
        {"toa1n ", "toán ", BOTH, strict}, {"d9i ", "đi ", BOTH, strict},
        // Restore, modern style, and keys that end the word.
        {"password ", "password "}, {"text ", "text "},
        {"hoa2 ", "hoà ", BOTH, {{XK_OPT_MODERN_STYLE, 1}}}, {"hoa2 ", "hòa "},
        {"chao2\n", "chào\n"},
    });
}

// A syllable that ends in a stop (c ch p t) takes only sắc or nặng: "bát",
// "bạt", never "bat", "bàt", "bảt", "bãt".
TEST_CASE("syllables: stop codas take only sắc or nặng", "[corpus]") {
    CHECK(xk_is_valid_syllable("bát") == 1);
    CHECK(xk_is_valid_syllable("bạt") == 1);
    CHECK(xk_is_valid_syllable("thích") == 1);
    CHECK(xk_is_valid_syllable("bat") == 0);
    CHECK(xk_is_valid_syllable("bàt") == 0);
    CHECK(xk_is_valid_syllable("tẽt") == 0);
    CHECK(xk_is_valid_syllable("thich") == 0);
    CHECK(xk_is_valid_syllable("ban") == 1);
    CHECK(xk_is_valid_syllable("bàn") == 1);
    run(XK_MODE_TELEX, {{"xoot ", "xoot "}, {"tieep ", "tieep "}, {"tieeps ", "tiếp "}});
}

// After qu the y of quỳnh, quýt, quých is the vowel itself (the u belongs to
// the onset), as in huỳnh / huýt with the u as a glide.
TEST_CASE("syllables: qu takes the y rimes of quỳnh, quýt and quých", "[corpus]") {
    for (const char* s : {"quỳnh", "quýnh", "quynh", "quýt", "quỵt", "quých", "Quỳnh"}) {
        INFO(s);
        CHECK(xk_is_valid_syllable(s) == 1);
    }
    for (const char* s : {"quyt", "quỳt", "quych", "quuynh", "kynh", "ynh", "quyu"}) {
        INFO(s);
        CHECK(xk_is_valid_syllable(s) == 0);
    }
    run(XK_MODE_TELEX, {
        {"quyfnh ", "quỳnh "}, {"quynhf ", "quỳnh "}, {"Quyfnh ", "Quỳnh "},
        {"quyts ", "quýt "}, {"quytj ", "quỵt "}, {"quychs ", "quých "},
        {"quynhs ", "quýnh "}, {"QUYFNH ", "QUỲNH "}, {"huyfnh ", "huỳnh "},
        {"quyfnh ", "quỳnh ", BOTH, {{XK_OPT_MODERN_STYLE, 1}}},
        {"quyfnh<", "quỳn"}, {"quyts<", "quý"},
    });
    run(XK_MODE_VNI, {{"quy2nh ", "quỳnh "}, {"quynh2 ", "quỳnh "}, {"quyt1 ", "quýt "},
                      {"quy5t ", "quỵt "}});
}

TEST_CASE("options: defaults, unknown options, and the word in progress", "[corpus]") {
    XKState* s = xk_state_new(XK_MODE_TELEX);
    CHECK(xk_state_get_option(s, XK_OPT_AUTO_RESTORE) == 1);
    CHECK(xk_state_get_option(s, XK_OPT_MODERN_STYLE) == 0);
    CHECK(xk_state_get_option(s, XK_OPT_FREE_MARKING) == 1);
    CHECK(xk_state_get_option(s, XK_OPT_W_SHORTCUT) == 1);
    CHECK(xk_state_get_option(s, 99) == -1);
    xk_state_set_option(s, 99, 1);  // ignored
    for (const char* k = "hoaf"; *k; ++k) xk_process_key(s, static_cast<unsigned char>(*k), 0);
    xk_state_set_option(s, XK_OPT_MODERN_STYLE, 0);  // unchanged: word untouched
    char* p = xk_state_get_preedit(s);
    CHECK(std::string(p) == "hòa");
    std::free(p);
    xk_state_set_option(s, XK_OPT_MODERN_STYLE, 1);  // recomposed under the new style
    p = xk_state_get_preedit(s);
    CHECK(std::string(p) == "hoà");
    std::free(p);
    CHECK(xk_state_get_option(s, XK_OPT_MODERN_STYLE) == 1);
    xk_state_free(s);
}

// Nothing that long is Vietnamese: past 32 keys the word is committed in
// pieces (XKResult's fields are 64 bytes) and the rest stays as typed.
TEST_CASE("corpus: very long words stay as typed", "[corpus]") {
    run(XK_MODE_TELEX, {
        {"pneumonoultramicroscopicsilicovolcanoconiosis ",
         "pneumonoultramicroscopicsilicovolcanoconiosis "},
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ",
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa "},
    });
}

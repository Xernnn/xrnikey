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

TEST_CASE("corpus: marks and tones may follow the final consonant", "[corpus][!mayfail]") {
    run(XK_MODE_TELEX, {
        {"dangd ", "đang "}, {"tienge ", "tiêng "}, {"tiengse ", "tiếng "},
        {"tienges ", "tiếng "}, {"khongo ", "không "}, {"duongwf ", "dường "},
        {"chana ", "chân "}, {"vietje ", "việt "},
    });
}

TEST_CASE("corpus: Enter, Tab and Escape end the word and reach the app", "[corpus][!mayfail]") {
    run(XK_MODE_TELEX, {
        {"chaof\n", "chào\n"}, {"chaof\t", "chào\t"}, {"chaof\x1b", "chào"},
        {"xin chaof\nban", "xin chào\nban"}, {"\n", "\n"},
        {"terms\n", "terms\n"},
    });
}

TEST_CASE("corpus: VNI does what Telex does", "[corpus][!mayfail]") {
    run(XK_MODE_VNI, {
        {"a66 ", "a6 "}, {"a11 ", "a1 "}, {"d99 ", "d9 "}, {"toa1n0 ", "toan "},
        {"A1 ", "Á "}, {"D9u7o7c5 ", "Được "}, {"VIE6T5 ", "VIỆT "},
        {"thuo73 ", "thuở "}, {"luu7 ", "lưu "}, {"toa1n<", "t\xC3\xB3" "a"},
        {"dang9 ", "đang "}, {"tieng61 ", "tiếng "}, {"password ", "password "},
        {"a1<", ""}, {"chao2\n", "chào\n"},
    });
}
